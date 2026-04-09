use std::collections::{BTreeMap, BTreeSet};
use std::convert::Infallible;
use std::ffi::CString;
use std::io::{self, BufRead, BufReader, BufWriter, Write};
use std::mem::{size_of, zeroed};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::process::{Command, Stdio};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};
use clap::Parser;
use conUDS::config::Config as UdsConfig;
use conUDS::modules::uds::{
    DiagnosticSessionKind, DiagnosticSessionResponse, RoutineStartResponse, UdsSession,
};
use conUDS::SupportedResetTypes;
use futures::StreamExt;
use libc::{
    AF_CAN, CAN_EFF_FLAG, CAN_EFF_MASK, CAN_ERR_FLAG, CAN_RAW, CAN_RTR_FLAG, CAN_SFF_MASK,
    EINTR, SO_TIMESTAMPING, SOCK_RAW, SOF_TIMESTAMPING_RAW_HARDWARE,
    SOF_TIMESTAMPING_RX_HARDWARE, SOF_TIMESTAMPING_RX_SOFTWARE, SOF_TIMESTAMPING_SOFTWARE,
    SOL_SOCKET, bind, c_void, can_frame, if_nametoindex, iovec, msghdr, recvmsg, sa_family_t,
    sockaddr, sockaddr_can, socket, socklen_t,
};
use log::{debug, info, warn};
use serde::{Deserialize, Serialize};
use tokio::sync::{Mutex, RwLock, broadcast, mpsc};
use warp::sse::Event;
use warp::{Filter, Reply};

mod views;

use yamcan_dashboard as yamcan;
use yamcan_dashboard::NetworkBus;

const DEFAULT_PORT: u16 = 8091;
const DEFAULT_OFFLINE_TIMEOUT_SECS: u64 = 3;
const DEFAULT_SWEEP_INTERVAL_MS: u64 = 250;
const SUPPORTED_CONTROLLERS: &[&str] = &[
    "bmsb", "bmsw0", "bmsw1", "bmsw2", "bmsw3", "bmsw4", "bmsw5", "bmsw6", "bmsw7", "sws",
    "vcfront", "vcrear", "vcpdu",
];

#[derive(Debug, Parser, Clone)]
#[command(name = "dashboard", about = "Live carputer dashboard over CAN")]
pub struct Opts {
    #[arg(long, default_value_t = DEFAULT_PORT)]
    pub port: u16,

    #[arg(
        long,
        default_value = "/application/config/ota-agent/uds-manifest.yaml"
    )]
    pub uds_manifest: String,

    #[arg(
        long,
        default_value = "/application/config/ota-agent/uds-routines.yaml"
    )]
    pub routine_manifest: String,

    #[arg(long, default_value = "vcanVeh")]
    pub veh_iface: String,

    #[arg(long)]
    pub body_iface: Option<String>,

    #[arg(long, hide = true, default_value_t = false)]
    pub veh_worker: bool,

    #[arg(long, hide = true, default_value_t = false)]
    pub body_worker: bool,
}

#[derive(Debug, Deserialize)]
struct UdsManifest {
    nodes: BTreeMap<String, UdsNode>,
}

#[derive(Debug, Deserialize)]
struct UdsNode {
    request_id: u32,
    response_id: u32,
}

#[derive(Debug, Clone)]
struct ControllerCapability {
    name: String,
    request_id: u32,
    response_id: u32,
    uds_iface: String,
    sessions: Vec<DiagnosticSessionOption>,
    resets: Vec<ResetOption>,
    routines: Vec<RoutineCapability>,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct DiagnosticSessionOption {
    pub key: String,
    pub label: String,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct RoutineCapability {
    pub name: String,
    pub label: String,
    pub id_hex: String,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ResetOption {
    pub key: String,
    pub label: String,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct DashboardJob {
    pub id: String,
    pub controller: String,
    pub operation: String,
    pub state: String,
    pub detail: String,
    pub payload_text: Option<String>,
    pub payload_hex: Option<String>,
    pub created_at_ms: u64,
    pub started_at_ms: Option<u64>,
    pub finished_at_ms: Option<u64>,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct JobsSnapshot {
    pub jobs: Vec<DashboardJob>,
}

#[derive(Debug, Deserialize)]
struct SessionActionRequest {
    session: String,
}

#[derive(Debug, Deserialize)]
struct RoutineActionRequest {
    payload_hex: Option<String>,
}

#[derive(Debug, Deserialize)]
struct ResetActionRequest {
    reset: String,
}

#[derive(Debug, Serialize)]
struct ActionAcceptedResponse {
    ok: bool,
    job_id: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ActiveFault {
    pub signal_name: String,
    pub label: Option<String>,
    pub value: String,
    pub source_message: String,
    pub updated_at_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct LiveSignal {
    pub signal_name: String,
    pub label: Option<String>,
    pub value: String,
    pub source_message: String,
    pub updated_at_ms: u64,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ControllerStatus {
    pub name: String,
    pub online: bool,
    pub last_seen_ms: Option<u64>,
    pub faults: Vec<ActiveFault>,
    pub critical_signals: Vec<LiveSignal>,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct DashboardSnapshot {
    pub controllers: Vec<ControllerStatus>,
}

#[derive(Debug, Clone)]
struct PlainMeasurement {
    name: String,
    value: f64,
    label: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct NormalizedUpdate {
    controller: String,
    seen_at_ms: u64,
    active_faults: Option<Vec<ActiveFault>>,
    critical_signals: Option<Vec<LiveSignal>>,
}

#[derive(Debug, Clone)]
struct ControllerRuntime {
    name: String,
    last_seen_at: Option<Instant>,
    last_seen_ms: Option<u64>,
    faults: Vec<ActiveFault>,
    critical_signals: Vec<LiveSignal>,
}

#[derive(Debug)]
struct DashboardStore {
    controllers: BTreeMap<String, ControllerRuntime>,
    offline_timeout: Duration,
}

#[derive(Debug)]
struct JobStore {
    jobs: BTreeMap<String, DashboardJob>,
    active_by_controller: BTreeMap<String, String>,
    next_job_id: u64,
}

#[derive(Clone)]
struct AppState {
    store: Arc<RwLock<DashboardStore>>,
    capabilities: Arc<BTreeMap<String, ControllerCapability>>,
    state_events: broadcast::Sender<String>,
    job_events: broadcast::Sender<String>,
    last_state_payload: Arc<Mutex<String>>,
    last_jobs_payload: Arc<Mutex<String>>,
    jobs: Arc<RwLock<JobStore>>,
}

impl DashboardStore {
    fn new(controller_names: &[String], offline_timeout: Duration) -> Self {
        let controllers = controller_names
            .iter()
            .map(|name| {
                (
                    name.clone(),
                    ControllerRuntime {
                        name: name.clone(),
                        last_seen_at: None,
                        last_seen_ms: None,
                        faults: Vec::new(),
                        critical_signals: Vec::new(),
                    },
                )
            })
            .collect();

        Self {
            controllers,
            offline_timeout,
        }
    }

    fn apply_update(&mut self, update: NormalizedUpdate, now: Instant) {
        let Some(controller) = self.controllers.get_mut(&update.controller) else {
            return;
        };

        controller.last_seen_at = Some(now);
        controller.last_seen_ms = Some(update.seen_at_ms);

        if let Some(active_faults) = update.active_faults {
            controller.faults = active_faults;
        }

        if let Some(critical_signals) = update.critical_signals {
            controller.critical_signals = critical_signals;
        }
    }

    fn snapshot(&self, now: Instant) -> DashboardSnapshot {
        let mut controllers = Vec::with_capacity(self.controllers.len());
        for controller in self.controllers.values() {
            let online = controller
                .last_seen_at
                .map(|seen| now.duration_since(seen) < self.offline_timeout)
                .unwrap_or(false);

            controllers.push(ControllerStatus {
                name: controller.name.clone(),
                online,
                last_seen_ms: controller.last_seen_ms,
                faults: controller.faults.clone(),
                critical_signals: controller.critical_signals.clone(),
            });
        }

        DashboardSnapshot { controllers }
    }
}

impl JobStore {
    fn new() -> Self {
        Self {
            jobs: BTreeMap::new(),
            active_by_controller: BTreeMap::new(),
            next_job_id: 1,
        }
    }

    fn create_job(
        &mut self,
        controller: &str,
        operation: String,
        detail: String,
    ) -> Result<DashboardJob> {
        if let Some(active_job_id) = self.active_by_controller.get(controller) {
            return Err(anyhow::anyhow!(
                "controller '{controller}' already has active job '{active_job_id}'"
            ));
        }

        let id = format!("job-{:05}", self.next_job_id);
        self.next_job_id += 1;
        let job = DashboardJob {
            id: id.clone(),
            controller: controller.to_string(),
            operation,
            state: "queued".to_string(),
            detail,
            payload_text: None,
            payload_hex: None,
            created_at_ms: now_ms(),
            started_at_ms: None,
            finished_at_ms: None,
        };
        self.active_by_controller
            .insert(controller.to_string(), id.clone());
        self.jobs.insert(id.clone(), job.clone());
        Ok(job)
    }

    fn mark_started(&mut self, job_id: &str, detail: String) {
        if let Some(job) = self.jobs.get_mut(job_id) {
            job.state = "running".to_string();
            job.detail = detail;
            job.started_at_ms = Some(now_ms());
        }
    }

    fn mark_succeeded(
        &mut self,
        job_id: &str,
        detail: String,
        payload_text: Option<String>,
        payload_hex: Option<String>,
    ) {
        if let Some(job) = self.jobs.get_mut(job_id) {
            job.state = "succeeded".to_string();
            job.detail = detail;
            job.payload_text = payload_text;
            job.payload_hex = payload_hex;
            job.finished_at_ms = Some(now_ms());
            self.active_by_controller.remove(&job.controller);
        }
    }

    fn mark_failed(
        &mut self,
        job_id: &str,
        detail: String,
        payload_text: Option<String>,
        payload_hex: Option<String>,
    ) {
        if let Some(job) = self.jobs.get_mut(job_id) {
            job.state = "failed".to_string();
            job.detail = detail;
            job.payload_text = payload_text;
            job.payload_hex = payload_hex;
            job.finished_at_ms = Some(now_ms());
            self.active_by_controller.remove(&job.controller);
        }
    }

    fn snapshot(&self) -> JobsSnapshot {
        let mut jobs = self.jobs.values().cloned().collect::<Vec<_>>();
        jobs.sort_by(|a, b| {
            b.created_at_ms
                .cmp(&a.created_at_ms)
                .then_with(|| a.id.cmp(&b.id))
        });
        JobsSnapshot { jobs }
    }
}

impl AppState {
    async fn snapshot(&self) -> DashboardSnapshot {
        let store = self.store.read().await;
        store.snapshot(Instant::now())
    }

    async fn snapshot_json(&self) -> Result<String> {
        let snapshot = self.snapshot().await;
        serde_json::to_string(&snapshot).context("serializing dashboard snapshot")
    }

    async fn jobs_snapshot(&self) -> JobsSnapshot {
        let jobs = self.jobs.read().await;
        jobs.snapshot()
    }

    async fn jobs_snapshot_json(&self) -> Result<String> {
        serde_json::to_string(&self.jobs_snapshot().await).context("serializing jobs snapshot")
    }

    async fn publish_state_if_changed(&self) -> Result<()> {
        let payload = self.snapshot_json().await?;
        let mut last_payload = self.last_state_payload.lock().await;
        if *last_payload != payload {
            *last_payload = payload.clone();
            let _ = self.state_events.send(payload);
        }
        Ok(())
    }

    async fn publish_jobs_if_changed(&self) -> Result<()> {
        let payload = self.jobs_snapshot_json().await?;
        let mut last_payload = self.last_jobs_payload.lock().await;
        if *last_payload != payload {
            *last_payload = payload.clone();
            let _ = self.job_events.send(payload);
        }
        Ok(())
    }
}

pub async fn run(opts: Opts) -> Result<()> {
    if opts.veh_worker {
        return run_veh_worker(&opts);
    }
    if opts.body_worker {
        return run_body_worker(&opts);
    }

    info!(
        "initializing dashboard with uds_manifest='{}', routine_manifest='{}', veh_iface='{}', body_iface='{}', port={}",
        opts.uds_manifest,
        opts.routine_manifest,
        opts.veh_iface,
        opts.body_iface.as_deref().unwrap_or("disabled"),
        opts.port
    );
    let capabilities = Arc::new(load_controller_capabilities(
        &opts.uds_manifest,
        &opts.routine_manifest,
        &opts.veh_iface,
    )?);
    let controller_names = capabilities.keys().cloned().collect::<Vec<_>>();
    info!(
        "loaded {} deployable controller(s) from UDS manifest: {}",
        controller_names.len(),
        controller_names.join(", ")
    );
    let tracked_controllers = Arc::new(
        controller_names
            .iter()
            .cloned()
            .collect::<BTreeSet<String>>(),
    );
    let store = Arc::new(RwLock::new(DashboardStore::new(
        &controller_names,
        Duration::from_secs(DEFAULT_OFFLINE_TIMEOUT_SECS),
    )));
    let jobs = Arc::new(RwLock::new(JobStore::new()));
    let (state_events, _) = broadcast::channel(64);
    let (job_events, _) = broadcast::channel(64);
    let state = AppState {
        store,
        capabilities,
        state_events,
        job_events,
        last_state_payload: Arc::new(Mutex::new(String::new())),
        last_jobs_payload: Arc::new(Mutex::new(String::new())),
        jobs,
    };

    info!("seeding initial dashboard snapshot");
    state.publish_state_if_changed().await?;
    state.publish_jobs_if_changed().await?;

    let (updates_tx, mut updates_rx) = mpsc::unbounded_channel::<NormalizedUpdate>();

    let state_for_updates = state.clone();
    tokio::spawn(async move {
        info!("dashboard update task started");
        while let Some(update) = updates_rx.recv().await {
            let now = Instant::now();
            {
                let mut store = state_for_updates.store.write().await;
                store.apply_update(update, now);
            }
            if let Err(e) = state_for_updates.publish_state_if_changed().await {
                warn!("failed to publish dashboard update: {e}");
            }
        }
    });

    let state_for_sweep = state.clone();
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_millis(DEFAULT_SWEEP_INTERVAL_MS));
        loop {
            interval.tick().await;
            if let Err(e) = state_for_sweep.publish_state_if_changed().await {
                warn!("failed to publish sweep snapshot: {e}");
            }
        }
    });

    let state_filter = warp::any().map(move || state.clone());

    let home = warp::path::end()
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_home);

    let controller = warp::path!("controllers" / String)
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_controller);

    let enter_session = warp::path!("api" / "controllers" / String / "session")
        .and(warp::post())
        .and(warp::body::form())
        .and(state_filter.clone())
        .and_then(handle_enter_session);

    let run_routine = warp::path!("api" / "controllers" / String / "routines" / String)
        .and(warp::post())
        .and(warp::body::form())
        .and(state_filter.clone())
        .and_then(handle_run_routine);

    let reset_controller = warp::path!("api" / "controllers" / String / "reset")
        .and(warp::post())
        .and(warp::body::form())
        .and(state_filter.clone())
        .and_then(handle_reset_controller);

    let events = warp::path("events")
        .and(warp::get())
        .and(state_filter.clone())
        .map(|state: AppState| {
            let state_for_state_updates = state.clone();
            let state_for_job_updates = state.clone();
            let state_events = state.state_events.clone();
            let job_events = state.job_events.clone();
            info!(
                "SSE client connected; subscriber count will become {}",
                state_events.receiver_count() + 1
            );
            let initial_state = futures::stream::once({
                let state = state.clone();
                async move {
                    let payload = state.snapshot_json().await.unwrap_or_else(|_| {
                        serde_json::to_string(&DashboardSnapshot {
                            controllers: Vec::new(),
                        })
                        .unwrap()
                    });
                    Ok::<Event, Infallible>(Event::default().event("state").data(payload))
                }
            });

            let initial_jobs = futures::stream::once({
                let state = state.clone();
                async move {
                    let payload = state.jobs_snapshot_json().await.unwrap_or_else(|_| {
                        serde_json::to_string(&JobsSnapshot { jobs: Vec::new() }).unwrap()
                    });
                    Ok::<Event, Infallible>(Event::default().event("jobs").data(payload))
                }
            });

            let state_updates = futures::stream::unfold(state_events.subscribe(), move |mut rx| {
                let state = state_for_state_updates.clone();
                async move {
                    loop {
                        match rx.recv().await {
                            Ok(payload) => {
                                return Some((
                                    Ok::<Event, Infallible>(
                                        Event::default().event("state").data(payload),
                                    ),
                                    rx,
                                ));
                            }
                            Err(broadcast::error::RecvError::Lagged(_)) => {
                                warn!("SSE client lagged behind; resending latest snapshot");
                                let payload = state.snapshot_json().await.unwrap_or_else(|_| {
                                    serde_json::to_string(&DashboardSnapshot {
                                        controllers: Vec::new(),
                                    })
                                    .unwrap()
                                });
                                return Some((
                                    Ok::<Event, Infallible>(
                                        Event::default().event("state").data(payload),
                                    ),
                                    rx,
                                ));
                            }
                            Err(broadcast::error::RecvError::Closed) => {
                                info!("SSE stream closed");
                                return None;
                            }
                        }
                    }
                }
            });

            let job_updates = futures::stream::unfold(job_events.subscribe(), move |mut rx| {
                let state = state_for_job_updates.clone();
                async move {
                    loop {
                        match rx.recv().await {
                            Ok(payload) => {
                                return Some((
                                    Ok::<Event, Infallible>(
                                        Event::default().event("jobs").data(payload),
                                    ),
                                    rx,
                                ));
                            }
                            Err(broadcast::error::RecvError::Lagged(_)) => {
                                warn!("SSE jobs client lagged behind; resending latest snapshot");
                                let payload =
                                    state.jobs_snapshot_json().await.unwrap_or_else(|_| {
                                        serde_json::to_string(&JobsSnapshot { jobs: Vec::new() })
                                            .unwrap()
                                    });
                                return Some((
                                    Ok::<Event, Infallible>(
                                        Event::default().event("jobs").data(payload),
                                    ),
                                    rx,
                                ));
                            }
                            Err(broadcast::error::RecvError::Closed) => {
                                info!("jobs SSE stream closed");
                                return None;
                            }
                        }
                    }
                }
            });

            let stream = initial_state
                .chain(initial_jobs)
                .chain(state_updates)
                .chain(job_updates);
            warp::sse::reply(warp::sse::keep_alive().stream(stream))
        });

    let health = warp::path("healthz")
        .and(warp::get())
        .map(|| warp::reply::with_status("ok", warp::http::StatusCode::OK));

    let routes = home
        .or(controller)
        .or(enter_session)
        .or(run_routine)
        .or(reset_controller)
        .or(events)
        .or(health);
    let addr = ([0, 0, 0, 0], opts.port);
    info!(
        "starting HTTP server on http://0.0.0.0:{} with routes '/', '/controllers/:name', '/api/controllers/:name/session', '/api/controllers/:name/routines/:routine', '/api/controllers/:name/reset', '/events', '/healthz'",
        opts.port
    );
    let (_, server) = warp::serve(routes)
        .try_bind_ephemeral(addr)
        .context(format!("binding dashboard HTTP server to 0.0.0.0:{}", opts.port))?;

    spawn_veh_worker(
        opts.veh_iface.clone(),
        Arc::clone(&tracked_controllers),
        updates_tx.clone(),
    );
    if let Some(body_iface) = &opts.body_iface {
        spawn_body_worker(
            body_iface.clone(),
            Arc::clone(&tracked_controllers),
            updates_tx.clone(),
        );
    }

    server.await;
    Ok(())
}

async fn handle_home(state: AppState) -> Result<warp::reply::Response, Infallible> {
    debug!("serving dashboard homepage");
    let snapshot = state.snapshot().await;
    let initial_state_json = serde_json::to_string(&snapshot).unwrap_or_else(|_| {
        serde_json::to_string(&DashboardSnapshot {
            controllers: Vec::new(),
        })
        .unwrap()
    });
    Ok(render_template_response(
        views::render_home(&snapshot, initial_state_json),
        warp::http::StatusCode::OK,
    ))
}

async fn handle_controller(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    debug!("serving controller page for '{controller_name}'");
    let snapshot = state.snapshot().await;
    let Some(controller) = snapshot
        .controllers
        .iter()
        .find(|controller| controller.name == controller_name)
    else {
        return Ok(render_template_response(
            views::render_not_found(&controller_name),
            warp::http::StatusCode::NOT_FOUND,
        ));
    };
    let Some(capability) = state.capabilities.get(&controller_name) else {
        return Ok(render_template_response(
            views::render_not_found(&controller_name),
            warp::http::StatusCode::NOT_FOUND,
        ));
    };

    let initial_controller_json = serde_json::to_string(controller).unwrap_or_else(|_| {
        serde_json::to_string(&ControllerStatus {
            name: controller_name.clone(),
            online: false,
            last_seen_ms: None,
            faults: Vec::new(),
            critical_signals: Vec::new(),
        })
        .unwrap()
    });
    let initial_jobs_json = serde_json::to_string(&state.jobs_snapshot().await)
        .unwrap_or_else(|_| serde_json::to_string(&JobsSnapshot { jobs: Vec::new() }).unwrap());

    Ok(render_template_response(
        views::render_controller(
            controller,
            capability,
            initial_controller_json,
            initial_jobs_json,
        ),
        warp::http::StatusCode::OK,
    ))
}

async fn handle_enter_session(
    controller_name: String,
    request: SessionActionRequest,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let session = match parse_session_key(&request.session) {
        Ok(session) => session,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_REQUEST,
                &error.to_string(),
            ));
        }
    };

    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };

    if !capability
        .sessions
        .iter()
        .any(|option| option.key == request.session)
    {
        return Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            "unsupported diagnostic session for controller",
        ));
    }

    let detail = format!("Queued diagnostic session {}", request.session);
    let job = match create_job(&state, &controller_name, "enter-session", detail).await {
        Ok(job) => job,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::CONFLICT,
                &error.to_string(),
            ));
        }
    };

    let state_for_job = state.clone();
    let job_id = job.id.clone();
    tokio::spawn(async move {
        run_session_job(state_for_job, capability, job_id, session).await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id,
    }))
}

async fn handle_run_routine(
    controller_name: String,
    routine_name: String,
    request: RoutineActionRequest,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };
    let Some(routine) = capability
        .routines
        .iter()
        .find(|routine| routine.name == routine_name)
        .cloned()
    else {
        return Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            "unsupported routine for controller",
        ));
    };

    let payload = match parse_hex_payload(request.payload_hex.as_deref()) {
        Ok(payload) => payload,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_REQUEST,
                &error.to_string(),
            ));
        }
    };

    let detail = format!("Queued routine {}", routine.name);
    let job = match create_job(&state, &controller_name, "run-routine", detail).await {
        Ok(job) => job,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::CONFLICT,
                &error.to_string(),
            ));
        }
    };

    let state_for_job = state.clone();
    let job_id = job.id.clone();
    tokio::spawn(async move {
        run_routine_job(state_for_job, capability, job_id, routine, payload).await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id,
    }))
}

async fn handle_reset_controller(
    controller_name: String,
    request: ResetActionRequest,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let reset_type = match parse_reset_key(&request.reset) {
        Ok(reset_type) => reset_type,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_REQUEST,
                &error.to_string(),
            ));
        }
    };

    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };

    if !capability
        .resets
        .iter()
        .any(|option| option.key == request.reset)
    {
        return Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            "unsupported reset type for controller",
        ));
    }

    let detail = format!("Queued {} reset", request.reset);
    let job = match create_job(&state, &controller_name, "ecu-reset", detail).await {
        Ok(job) => job,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::CONFLICT,
                &error.to_string(),
            ));
        }
    };

    let state_for_job = state.clone();
    let job_id = job.id.clone();
    tokio::spawn(async move {
        run_reset_job(state_for_job, capability, job_id, reset_type).await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id,
    }))
}

fn load_controller_capabilities(
    uds_manifest: &str,
    routine_manifest: &str,
    veh_iface: &str,
) -> Result<BTreeMap<String, ControllerCapability>> {
    debug!(
        "loading controller capabilities from uds_manifest='{}' routine_manifest='{}'",
        uds_manifest, routine_manifest
    );
    let config = UdsConfig::new(uds_manifest, routine_manifest)?;
    let mut capabilities = BTreeMap::new();
    for (name, node) in config.nodes {
        let controller_name = name.to_ascii_lowercase();
        if node.request_id == 0 || node.response_id == 0 {
            continue;
        }
        if !is_supported_controller(&controller_name) {
            info!("ignoring unsupported controller '{controller_name}' from UDS manifest");
            continue;
        }

        let routines = node
            .routines
            .into_iter()
            .map(|(routine_name, routine)| RoutineCapability {
                label: humanize_identifier(&routine_name),
                name: routine_name,
                id_hex: format!("0x{:04X}", routine.id),
            })
            .collect::<Vec<_>>();

        capabilities.insert(
            controller_name.clone(),
            ControllerCapability {
                name: controller_name,
                request_id: node.request_id,
                response_id: node.response_id,
                uds_iface: veh_iface.to_string(),
                sessions: default_session_options(),
                resets: default_reset_options(),
                routines,
            },
        );
    }
    Ok(capabilities)
}

fn is_supported_controller(controller_name: &str) -> bool {
    SUPPORTED_CONTROLLERS.contains(&controller_name)
}

fn default_session_options() -> Vec<DiagnosticSessionOption> {
    vec![
        DiagnosticSessionOption {
            key: "default".to_string(),
            label: "Default".to_string(),
        },
        DiagnosticSessionOption {
            key: "extended".to_string(),
            label: "Extended".to_string(),
        },
        DiagnosticSessionOption {
            key: "programming".to_string(),
            label: "Programming".to_string(),
        },
    ]
}

fn default_reset_options() -> Vec<ResetOption> {
    vec![
        ResetOption {
            key: "hard".to_string(),
            label: "Hard".to_string(),
        },
        ResetOption {
            key: "soft".to_string(),
            label: "Soft".to_string(),
        },
    ]
}

fn humanize_identifier(value: &str) -> String {
    if value.is_empty() {
        return String::new();
    }

    let mut out = String::with_capacity(value.len() + 4);
    let mut previous_was_lower = false;
    for ch in value.chars() {
        if ch == '_' || ch == '-' {
            out.push(' ');
            previous_was_lower = false;
            continue;
        }
        if ch.is_uppercase() && previous_was_lower {
            out.push(' ');
        }
        if out.is_empty() {
            out.push(ch.to_ascii_uppercase());
        } else {
            out.push(ch);
        }
        previous_was_lower = ch.is_ascii_lowercase();
    }
    out
}

fn parse_session_key(session: &str) -> Result<DiagnosticSessionKind> {
    match session {
        "default" => Ok(DiagnosticSessionKind::Default),
        "extended" => Ok(DiagnosticSessionKind::Extended),
        "programming" => Ok(DiagnosticSessionKind::Programming),
        "safety-system" => Ok(DiagnosticSessionKind::SafetySystem),
        _ => Err(anyhow::anyhow!(
            "unsupported diagnostic session '{session}'"
        )),
    }
}

fn parse_hex_payload(input: Option<&str>) -> Result<Option<Vec<u8>>> {
    let Some(input) = input.map(str::trim) else {
        return Ok(None);
    };
    if input.is_empty() {
        return Ok(None);
    }

    let normalized = input.replace([' ', '\n', '\r', '\t', ','], "");
    if normalized.len() % 2 != 0 {
        return Err(anyhow::anyhow!(
            "routine payload must contain an even number of hex digits"
        ));
    }

    let mut bytes = Vec::with_capacity(normalized.len() / 2);
    for chunk in normalized.as_bytes().chunks(2) {
        let text = std::str::from_utf8(chunk)?;
        bytes.push(
            u8::from_str_radix(text, 16)
                .with_context(|| format!("invalid hex byte '{text}' in routine payload"))?,
        );
    }
    Ok(Some(bytes))
}

fn parse_reset_key(reset: &str) -> Result<SupportedResetTypes> {
    match reset {
        "hard" => Ok(SupportedResetTypes::Hard),
        "soft" => Ok(SupportedResetTypes::Soft),
        _ => Err(anyhow::anyhow!("unsupported reset type '{reset}'")),
    }
}

async fn create_job(
    state: &AppState,
    controller_name: &str,
    operation: &str,
    detail: String,
) -> Result<DashboardJob> {
    let job = {
        let mut jobs = state.jobs.write().await;
        jobs.create_job(controller_name, operation.to_string(), detail)?
    };
    state.publish_jobs_if_changed().await?;
    Ok(job)
}

async fn run_session_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
    session: DiagnosticSessionKind,
) {
    info!(
        "starting session job '{}' for controller='{}' session='{:?}' on iface='{}'",
        job_id, capability.name, session, capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(
            &job_id,
            format!("Entering {} session", session_key_label(session)),
        );
    }
    let _ = state.publish_jobs_if_changed().await;

    let mut uds = UdsSession::new(
        &capability.uds_iface,
        capability.request_id,
        capability.response_id,
        false,
    )
    .await;
    let result = uds.client.enter_diagnostic_session(session).await;
    uds.teardown().await;

    match result {
        Ok(response) => {
            let (detail, payload_hex) = format_session_response(response);
            {
                let mut jobs = state.jobs.write().await;
                jobs.mark_succeeded(&job_id, detail, None, payload_hex);
            }
        }
        Err(error) => {
            let mut jobs = state.jobs.write().await;
            jobs.mark_failed(&job_id, error.to_string(), None, None);
        }
    }

    let _ = state.publish_jobs_if_changed().await;
}

async fn run_routine_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
    routine: RoutineCapability,
    payload: Option<Vec<u8>>,
) {
    info!(
        "starting routine job '{}' for controller='{}' routine='{}' on iface='{}'",
        job_id, capability.name, routine.name, capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, format!("Running routine {}", routine.name));
    }
    let _ = state.publish_jobs_if_changed().await;

    let routine_id = match u16::from_str_radix(routine.id_hex.trim_start_matches("0x"), 16) {
        Ok(routine_id) => routine_id,
        Err(error) => {
            {
                let mut jobs = state.jobs.write().await;
                jobs.mark_failed(
                    &job_id,
                    format!("invalid routine id '{}': {}", routine.id_hex, error),
                    None,
                    None,
                );
            }
            let _ = state.publish_jobs_if_changed().await;
            return;
        }
    };

    let mut uds = UdsSession::new(
        &capability.uds_iface,
        capability.request_id,
        capability.response_id,
        false,
    )
    .await;
    let result = uds.client.routine_start(routine_id, payload).await;
    uds.teardown().await;

    match result {
        Ok(response) => {
            let (detail, payload_text, payload_hex) =
                format_routine_response(&routine.name, response);
            {
                let mut jobs = state.jobs.write().await;
                jobs.mark_succeeded(&job_id, detail, payload_text, payload_hex);
            }
        }
        Err(error) => {
            let mut jobs = state.jobs.write().await;
            jobs.mark_failed(&job_id, error.to_string(), None, None);
        }
    }

    let _ = state.publish_jobs_if_changed().await;
}

async fn run_reset_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
    reset_type: SupportedResetTypes,
) {
    let reset_label = reset_key_label(&reset_type).to_string();
    info!(
        "starting reset job '{}' for controller='{}' reset='{}' on iface='{}'",
        job_id,
        capability.name,
        reset_label,
        capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, format!("Performing {reset_label} reset"));
    }
    let _ = state.publish_jobs_if_changed().await;

    let mut uds = UdsSession::new(
        &capability.uds_iface,
        capability.request_id,
        capability.response_id,
        false,
    )
    .await;
    let result = uds.reset_node(reset_type).await;
    uds.teardown().await;

    match result {
        Ok(()) => {
            let mut jobs = state.jobs.write().await;
            jobs.mark_succeeded(
                &job_id,
                format!("{reset_label} reset completed"),
                None,
                None,
            );
        }
        Err(error) => {
            let mut jobs = state.jobs.write().await;
            jobs.mark_failed(&job_id, error.to_string(), None, None);
        }
    }

    let _ = state.publish_jobs_if_changed().await;
}

fn session_key_label(session: DiagnosticSessionKind) -> &'static str {
    match session {
        DiagnosticSessionKind::Default => "Default",
        DiagnosticSessionKind::Programming => "Programming",
        DiagnosticSessionKind::Extended => "Extended",
        DiagnosticSessionKind::SafetySystem => "Safety System",
    }
}

fn reset_key_label(reset: &SupportedResetTypes) -> &'static str {
    match reset {
        SupportedResetTypes::Hard => "Hard",
        SupportedResetTypes::Soft => "Soft",
    }
}

fn format_session_response(response: DiagnosticSessionResponse) -> (String, Option<String>) {
    match response {
        DiagnosticSessionResponse::Positive {
            session, payload, ..
        } => (
            format!("Entered {} session", session_key_label(session)),
            (!payload.is_empty()).then(|| hex_string(&payload)),
        ),
        DiagnosticSessionResponse::Negative {
            session,
            nrc,
            description,
            payload,
            ..
        } => (
            format!(
                "{} session rejected: NRC 0x{:02X} ({})",
                session_key_label(session),
                nrc,
                description
            ),
            (!payload.is_empty()).then(|| hex_string(&payload)),
        ),
    }
}

fn format_routine_response(
    routine_name: &str,
    response: RoutineStartResponse,
) -> (String, Option<String>, Option<String>) {
    match response {
        RoutineStartResponse::Positive { text, payload, .. } => (
            format!("Routine '{}' completed successfully", routine_name),
            text,
            (!payload.is_empty()).then(|| hex_string(&payload)),
        ),
        RoutineStartResponse::Negative {
            nrc,
            description,
            payload,
            text,
            ..
        } => (
            format!(
                "Routine '{}' rejected: NRC 0x{:02X} ({})",
                routine_name, nrc, description
            ),
            text,
            (!payload.is_empty()).then(|| hex_string(&payload)),
        ),
    }
}

fn hex_string(payload: &[u8]) -> String {
    payload
        .iter()
        .map(|byte| format!("{byte:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

fn json_error_response(status: warp::http::StatusCode, message: &str) -> warp::reply::Response {
    let body = serde_json::json!({
        "ok": false,
        "error": message,
    });
    warp::reply::with_status(warp::reply::json(&body), status).into_response()
}

fn json_success_response(body: ActionAcceptedResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::ACCEPTED)
        .into_response()
}

fn spawn_veh_worker(
    iface: String,
    tracked_controllers: Arc<BTreeSet<String>>,
    updates_tx: mpsc::UnboundedSender<NormalizedUpdate>,
) {
    thread::spawn(move || {
        info!("spawning vehicle CAN worker supervisor for iface='{iface}'");
        loop {
            let exe = match std::env::current_exe() {
                Ok(exe) => exe,
                Err(e) => {
                    warn!("failed to resolve dashboard executable for worker: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };
            let mut child = match Command::new(exe)
                .arg("--veh-worker")
                .arg("--veh-iface")
                .arg(&iface)
                .stdout(Stdio::piped())
                .stderr(Stdio::inherit())
                .spawn()
            {
                Ok(child) => child,
                Err(e) => {
                    warn!("failed to spawn vehicle worker for {iface}: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };

            info!("vehicle worker started for iface='{iface}'");
            let Some(stdout) = child.stdout.take() else {
                warn!("vehicle worker stdout unavailable for iface='{iface}'");
                let _ = child.kill();
                thread::sleep(Duration::from_secs(1));
                continue;
            };

            let reader = BufReader::new(stdout);
            for line in reader.lines() {
                let line = match line {
                    Ok(line) => line,
                    Err(e) => {
                        warn!("failed reading vehicle worker output for {iface}: {e}");
                        break;
                    }
                };
                if line.trim().is_empty() {
                    continue;
                }
                let update: NormalizedUpdate = match serde_json::from_str(&line) {
                    Ok(update) => update,
                    Err(e) => {
                        warn!("failed parsing vehicle worker update for {iface}: {e}");
                        continue;
                    }
                };
                if !tracked_controllers.contains(&update.controller) {
                    continue;
                }
                if updates_tx.send(update).is_err() {
                    warn!("vehicle update channel closed; stopping worker supervisor");
                    let _ = child.kill();
                    return;
                }
            }

            match child.wait() {
                Ok(status) => warn!("vehicle worker exited for {iface} with status {status}"),
                Err(e) => warn!("failed waiting for vehicle worker on {iface}: {e}"),
            }
            thread::sleep(Duration::from_secs(1));
        }
    });
}

fn spawn_body_worker(
    iface: String,
    tracked_controllers: Arc<BTreeSet<String>>,
    updates_tx: mpsc::UnboundedSender<NormalizedUpdate>,
) {
    thread::spawn(move || {
        info!("spawning body CAN worker supervisor for iface='{iface}'");
        loop {
            let exe = match std::env::current_exe() {
                Ok(exe) => exe,
                Err(e) => {
                    warn!("failed to resolve dashboard executable for worker: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };
            let mut child = match Command::new(exe)
                .arg("--body-worker")
                .arg("--body-iface")
                .arg(&iface)
                .stdout(Stdio::piped())
                .stderr(Stdio::inherit())
                .spawn()
            {
                Ok(child) => child,
                Err(e) => {
                    warn!("failed to spawn body worker for {iface}: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };

            info!("body worker started for iface='{iface}'");
            let Some(stdout) = child.stdout.take() else {
                warn!("body worker stdout unavailable for iface='{iface}'");
                let _ = child.kill();
                thread::sleep(Duration::from_secs(1));
                continue;
            };

            let reader = BufReader::new(stdout);
            for line in reader.lines() {
                let line = match line {
                    Ok(line) => line,
                    Err(e) => {
                        warn!("failed reading body worker output for {iface}: {e}");
                        break;
                    }
                };
                if line.trim().is_empty() {
                    continue;
                }
                let update: NormalizedUpdate = match serde_json::from_str(&line) {
                    Ok(update) => update,
                    Err(e) => {
                        warn!("failed parsing body worker update for {iface}: {e}");
                        continue;
                    }
                };
                if !tracked_controllers.contains(&update.controller) {
                    continue;
                }
                if updates_tx.send(update).is_err() {
                    warn!("body update channel closed; stopping worker supervisor");
                    let _ = child.kill();
                    return;
                }
            }

            match child.wait() {
                Ok(status) => warn!("body worker exited for {iface} with status {status}"),
                Err(e) => warn!("failed waiting for body worker on {iface}: {e}"),
            }
            thread::sleep(Duration::from_secs(1));
        }
    });
}

fn decode_veh_frame(
    binding: &yamcan::BusBinding<yamcan::Bus>,
    frame: yamcan::CanFrame,
    id: u32,
    tracked_controllers: &BTreeSet<String>,
) -> Option<NormalizedUpdate> {
    let decoded = yamcan::maybe_decode(Some(binding), &frame, id, true, true, &[], &[])?;
    if controller_key_for_message_name(&decoded.message_name, tracked_controllers).is_none() {
        return None;
    }
    if !is_fault_message(&decoded.message_name) && !is_critical_data_message(&decoded.message_name)
    {
        return None;
    }
    Some(normalize_update(
        binding.bus.as_str(),
        decoded.message_name,
        decoded
            .members
            .into_iter()
            .map(|member| PlainMeasurement {
                name: member.name,
                value: member.value,
                label: member.label,
            })
            .collect(),
        tracked_controllers,
        now_ms(),
    )?)
}

fn run_veh_worker(opts: &Opts) -> Result<()> {
    let iface = opts.veh_iface.clone();
    let iface_map = [(iface.as_str(), yamcan::Bus::VcanVeh)];
    let binding = yamcan::configure_iface(&iface, &iface_map)
        .map_err(|e| anyhow::anyhow!("failed to configure veh decoder for {iface}: {e}"))?;
    run_can_worker(&iface, "vehicle", &binding)
}

fn run_body_worker(opts: &Opts) -> Result<()> {
    let iface = opts
        .body_iface
        .clone()
        .ok_or_else(|| anyhow::anyhow!("--body-worker requires --body-iface"))?;
    let iface_map = [(iface.as_str(), yamcan::Bus::VcanBody)];
    let binding = yamcan::configure_iface(&iface, &iface_map)
        .map_err(|e| anyhow::anyhow!("failed to configure body decoder for {iface}: {e}"))?;
    run_can_worker(&iface, "body", &binding)
}

fn run_can_worker(
    iface: &str,
    bus_label: &str,
    binding: &yamcan::BusBinding<yamcan::Bus>,
) -> Result<()> {
    let tracked_controllers = SUPPORTED_CONTROLLERS
        .iter()
        .map(|controller| controller.to_string())
        .collect::<BTreeSet<_>>();
    let socket = open_raw_can_socket(iface).with_context(|| format!("failed to open {iface}"))?;

    info!("listening on {iface} for {bus_label} dashboard updates");
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    loop {
        let (frame, id) = recv_veh_frame(&socket).with_context(|| format!("read error on {iface}"))?;
        let Some(update) = decode_veh_frame(binding, frame, id, &tracked_controllers) else {
            continue;
        };
        serde_json::to_writer(&mut writer, &update)
            .context("serializing worker update")?;
        writer.write_all(b"\n").context("writing worker newline")?;
        writer.flush().context("flushing worker update")?;
    }
}

fn normalize_update(
    _bus_name: &str,
    message_name: String,
    members: Vec<PlainMeasurement>,
    tracked_controllers: &BTreeSet<String>,
    seen_at_ms: u64,
) -> Option<NormalizedUpdate> {
    let controller = controller_key_for_message_name(&message_name, tracked_controllers)?;
    let active_faults = if is_fault_message(&message_name) {
        Some(extract_active_faults(&message_name, &members, seen_at_ms))
    } else {
        None
    };

    let critical_signals = if is_critical_data_message(&message_name)
        && controller_supports_critical_data(&controller)
    {
        Some(extract_live_signals(&message_name, &members, seen_at_ms))
    } else {
        None
    };

    Some(NormalizedUpdate {
        controller,
        seen_at_ms,
        active_faults,
        critical_signals,
    })
}

fn open_raw_can_socket(iface: &str) -> io::Result<OwnedFd> {
    let fd = unsafe { socket(AF_CAN, SOCK_RAW, CAN_RAW) };
    if fd < 0 {
        return Err(io::Error::last_os_error());
    }
    let fd = unsafe { OwnedFd::from_raw_fd(fd) };

    let ts_flags = SOF_TIMESTAMPING_RX_HARDWARE
        | SOF_TIMESTAMPING_RAW_HARDWARE
        | SOF_TIMESTAMPING_RX_SOFTWARE
        | SOF_TIMESTAMPING_SOFTWARE;

    let rc = unsafe {
        libc::setsockopt(
            fd.as_raw_fd(),
            SOL_SOCKET,
            SO_TIMESTAMPING,
            &ts_flags as *const _ as *const c_void,
            size_of::<i32>() as socklen_t,
        )
    };
    if rc != 0 {
        return Err(io::Error::last_os_error());
    }

    let ifname = CString::new(iface)
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "invalid iface"))?;
    let ifindex = unsafe { if_nametoindex(ifname.as_ptr()) };
    if ifindex == 0 {
        return Err(io::Error::last_os_error());
    }

    let mut addr: sockaddr_can = unsafe { zeroed() };
    addr.can_family = AF_CAN as sa_family_t;
    addr.can_ifindex = ifindex as i32;

    let rc = unsafe {
        bind(
            fd.as_raw_fd(),
            &addr as *const _ as *const sockaddr,
            size_of::<sockaddr_can>() as socklen_t,
        )
    };
    if rc != 0 {
        return Err(io::Error::last_os_error());
    }

    Ok(fd)
}

fn recv_veh_frame(fd: &OwnedFd) -> io::Result<(yamcan::CanFrame, u32)> {
    loop {
        let mut raw: can_frame = unsafe { zeroed() };
        let mut name: sockaddr_can = unsafe { zeroed() };
        let mut iov = iovec {
            iov_base: (&mut raw as *mut can_frame) as *mut c_void,
            iov_len: size_of::<can_frame>(),
        };
        let mut cbuf = [0u8; 256];
        let mut msg: msghdr = unsafe { zeroed() };
        msg.msg_name = (&mut name as *mut sockaddr_can) as *mut c_void;
        msg.msg_namelen = size_of::<sockaddr_can>() as socklen_t;
        msg.msg_iov = &mut iov as *mut iovec;
        msg.msg_iovlen = 1;
        msg.msg_control = cbuf.as_mut_ptr() as *mut c_void;
        msg.msg_controllen = cbuf.len();

        let n = unsafe { recvmsg(fd.as_raw_fd(), &mut msg, 0) };
        if n <= 0 {
            let err = io::Error::last_os_error();
            if err.raw_os_error() == Some(EINTR) {
                continue;
            }
            return Err(err);
        }
        if n as usize != size_of::<can_frame>() {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "short CAN frame read",
            ));
        }
        if (raw.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0 {
            continue;
        }

        let id = if (raw.can_id & CAN_EFF_FLAG) != 0 {
            raw.can_id & CAN_EFF_MASK
        } else {
            raw.can_id & CAN_SFF_MASK
        };
        return Ok((
            yamcan::CanFrame {
                can_id: raw.can_id,
                can_dlc: raw.can_dlc,
                data: raw.data,
            },
            id,
        ));
    }
}

fn is_fault_message(message_name: &str) -> bool {
    message_name.to_ascii_lowercase().ends_with("_faults")
}

fn is_critical_data_message(message_name: &str) -> bool {
    message_name.to_ascii_lowercase().ends_with("_criticaldata")
}

fn controller_supports_critical_data(controller_name: &str) -> bool {
    controller_name == "bmsb" || controller_name.starts_with("bmsw")
}

fn controller_key_for_message_name(
    message_name: &str,
    tracked_controllers: &BTreeSet<String>,
) -> Option<String> {
    let controller = message_name.split('_').next()?.to_ascii_lowercase();
    tracked_controllers
        .contains(&controller)
        .then_some(controller)
}

fn extract_active_faults(
    source_message: &str,
    members: &[PlainMeasurement],
    updated_at_ms: u64,
) -> Vec<ActiveFault> {
    let mut active_faults = members
        .iter()
        .filter(|member| member_is_active_fault(member))
        .map(|member| ActiveFault {
            signal_name: member.name.clone(),
            label: member.label.clone(),
            value: format_numeric(member.value),
            source_message: source_message.to_string(),
            updated_at_ms,
        })
        .collect::<Vec<_>>();
    active_faults.sort_by(|a, b| a.signal_name.cmp(&b.signal_name));
    active_faults
}

fn extract_live_signals(
    source_message: &str,
    members: &[PlainMeasurement],
    updated_at_ms: u64,
) -> Vec<LiveSignal> {
    let mut signals = members
        .iter()
        .map(|member| LiveSignal {
            signal_name: member.name.clone(),
            label: member.label.clone(),
            value: format_measurement_value(member),
            source_message: source_message.to_string(),
            updated_at_ms,
        })
        .collect::<Vec<_>>();
    signals.sort_by(|a, b| a.signal_name.cmp(&b.signal_name));
    signals
}

fn member_is_active_fault(member: &PlainMeasurement) -> bool {
    if let Some(label) = member.label.as_deref() {
        let label = label.to_ascii_uppercase();
        if matches!(
            label.as_str(),
            "OK" | "CLEAR" | "CLEARED" | "NONE" | "OFF" | "FALSE" | "INACTIVE"
        ) {
            return false;
        }
        if matches!(
            label.as_str(),
            "FAULT" | "FAULT_LATCHED" | "ERROR" | "ON" | "TRUE" | "SET"
        ) {
            return true;
        }
    }

    member.value.is_finite() && member.value.abs() > f64::EPSILON
}

fn format_numeric(value: f64) -> String {
    if value.fract().abs() < f64::EPSILON {
        format!("{}", value as i64)
    } else {
        format!("{value:.3}")
    }
}

fn format_measurement_value(member: &PlainMeasurement) -> String {
    member
        .label
        .clone()
        .unwrap_or_else(|| format_numeric(member.value))
}

fn now_ms() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}

fn render_template_response(
    rendered: Result<String>,
    status: warp::http::StatusCode,
) -> warp::reply::Response {
    match rendered {
        Ok(body) => warp::reply::with_status(warp::reply::html(body), status).into_response(),
        Err(error) => {
            warn!("failed to render template: {error}");
            warp::reply::with_status(
                "template render error".to_string(),
                warp::http::StatusCode::INTERNAL_SERVER_ERROR,
            )
            .into_response()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn controller_name_normalization_only_accepts_tracked_controllers() {
        let tracked = BTreeSet::from([
            "vcfront".to_string(),
            "vcpdu".to_string(),
            "bmsw0".to_string(),
        ]);

        assert_eq!(
            controller_key_for_message_name("VCFRONT_faults", &tracked),
            Some("vcfront".to_string())
        );
        assert_eq!(
            controller_key_for_message_name("BMSW0_criticalData", &tracked),
            Some("bmsw0".to_string())
        );
        assert_eq!(
            controller_key_for_message_name("PM100DX_faults", &tracked),
            None
        );
    }

    #[test]
    fn supported_controller_allowlist_rejects_stw() {
        assert!(is_supported_controller("vcfront"));
        assert!(is_supported_controller("bmsw0"));
        assert!(!is_supported_controller("stw"));
    }

    #[test]
    fn only_bms_controllers_support_critical_data_tables() {
        assert!(controller_supports_critical_data("bmsb"));
        assert!(controller_supports_critical_data("bmsw0"));
        assert!(!controller_supports_critical_data("vcfront"));
    }

    #[test]
    fn active_fault_extraction_filters_out_ok_values() {
        let faults = extract_active_faults(
            "VCFRONT_faults",
            &[
                PlainMeasurement {
                    name: "appsDisabled".to_string(),
                    value: 1.0,
                    label: Some("FAULT".to_string()),
                },
                PlainMeasurement {
                    name: "faulted5vCritical".to_string(),
                    value: 0.0,
                    label: Some("OK".to_string()),
                },
            ],
            123,
        );

        assert_eq!(faults.len(), 1);
        assert_eq!(faults[0].signal_name, "appsDisabled");
        assert_eq!(faults[0].label.as_deref(), Some("FAULT"));
    }

    #[test]
    fn critical_data_extraction_preserves_signal_values() {
        let signals = extract_live_signals(
            "BMSW0_criticalData",
            &[
                PlainMeasurement {
                    name: "cellVoltage".to_string(),
                    value: 3.712,
                    label: None,
                },
                PlainMeasurement {
                    name: "balancingState".to_string(),
                    value: 1.0,
                    label: Some("ACTIVE".to_string()),
                },
            ],
            123,
        );

        assert_eq!(signals.len(), 2);
        assert_eq!(signals[0].signal_name, "balancingState");
        assert_eq!(signals[0].value, "ACTIVE");
        assert_eq!(signals[1].signal_name, "cellVoltage");
        assert_eq!(signals[1].value, "3.712");
    }

    #[test]
    fn snapshot_marks_controller_offline_after_timeout() {
        let mut store = DashboardStore::new(&["vcfront".to_string()], Duration::from_secs(3));
        let base = Instant::now();
        store.apply_update(
            NormalizedUpdate {
                controller: "vcfront".to_string(),
                seen_at_ms: 100,
                active_faults: None,
                critical_signals: None,
            },
            base,
        );

        let online = store.snapshot(base + Duration::from_secs(2));
        assert!(online.controllers[0].online);

        let offline = store.snapshot(base + Duration::from_secs(4));
        assert!(!offline.controllers[0].online);
    }

    #[test]
    fn newer_fault_message_replaces_previous_faults() {
        let mut store = DashboardStore::new(&["vcfront".to_string()], Duration::from_secs(3));
        let now = Instant::now();
        store.apply_update(
            NormalizedUpdate {
                controller: "vcfront".to_string(),
                seen_at_ms: 100,
                active_faults: Some(vec![ActiveFault {
                    signal_name: "appsDisabled".to_string(),
                    label: Some("FAULT".to_string()),
                    value: "1".to_string(),
                    source_message: "VCFRONT_faults".to_string(),
                    updated_at_ms: 100,
                }]),
                critical_signals: None,
            },
            now,
        );
        store.apply_update(
            NormalizedUpdate {
                controller: "vcfront".to_string(),
                seen_at_ms: 200,
                active_faults: Some(Vec::new()),
                critical_signals: None,
            },
            now + Duration::from_secs(1),
        );

        let snapshot = store.snapshot(now + Duration::from_secs(1));
        assert!(snapshot.controllers[0].faults.is_empty());
    }
}
