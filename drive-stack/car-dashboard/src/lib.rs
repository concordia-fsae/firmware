use std::collections::{BTreeMap, BTreeSet};
use std::convert::Infallible;
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};
use askama::Template;
use clap::Parser;
use futures::StreamExt;
use log::{debug, info, warn};
use serde::{Deserialize, Serialize};
use socketcan::{CanSocket, EmbeddedFrame, Frame, Id, Socket};
use tokio::sync::{Mutex, RwLock, broadcast, mpsc};
use warp::sse::Event;
use warp::{Filter, Reply};

mod views;

use views::{ControllerTemplate, ErrorTemplate, HomeTemplate};

use yamcan_dashboard_body as body;
use yamcan_dashboard_body::NetworkBus as BodyNetworkBus;
use yamcan_dashboard_veh as veh;
use yamcan_dashboard_veh::NetworkBus as VehNetworkBus;

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

    #[arg(long, default_value = "vcanVeh")]
    pub veh_iface: String,

    #[arg(long, default_value = "vcanBody")]
    pub body_iface: String,
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

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ActiveFault {
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

#[derive(Debug, Clone)]
struct NormalizedUpdate {
    controller: String,
    seen_at_ms: u64,
    active_faults: Option<Vec<ActiveFault>>,
}

#[derive(Debug, Clone)]
struct ControllerRuntime {
    name: String,
    last_seen_at: Option<Instant>,
    last_seen_ms: Option<u64>,
    faults: Vec<ActiveFault>,
}

#[derive(Debug)]
struct DashboardStore {
    controllers: BTreeMap<String, ControllerRuntime>,
    offline_timeout: Duration,
}

#[derive(Clone)]
struct AppState {
    store: Arc<RwLock<DashboardStore>>,
    events: broadcast::Sender<String>,
    last_payload: Arc<Mutex<String>>,
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
            });
        }

        DashboardSnapshot { controllers }
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

    async fn publish_if_changed(&self) -> Result<()> {
        let payload = self.snapshot_json().await?;
        let mut last_payload = self.last_payload.lock().await;
        if *last_payload != payload {
            *last_payload = payload.clone();
            let subscriber_count = self.events.receiver_count();
            debug!(
                "publishing updated dashboard snapshot to {} SSE subscriber(s)",
                subscriber_count
            );
            let _ = self.events.send(payload);
        }
        Ok(())
    }
}

pub async fn run(opts: Opts) -> Result<()> {
    info!(
        "initializing dashboard with uds_manifest='{}', veh_iface='{}', body_iface='{}', port={}",
        opts.uds_manifest, opts.veh_iface, opts.body_iface, opts.port
    );
    let controller_names = load_controller_names(&opts.uds_manifest)?;
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
    let (events, _) = broadcast::channel(64);
    let state = AppState {
        store,
        events,
        last_payload: Arc::new(Mutex::new(String::new())),
    };

    info!("seeding initial dashboard snapshot");
    state.publish_if_changed().await?;

    let (updates_tx, mut updates_rx) = mpsc::unbounded_channel::<NormalizedUpdate>();

    spawn_veh_reader(
        opts.veh_iface.clone(),
        Arc::clone(&tracked_controllers),
        updates_tx.clone(),
    );
    spawn_body_reader(
        opts.body_iface.clone(),
        Arc::clone(&tracked_controllers),
        updates_tx,
    );

    let state_for_updates = state.clone();
    tokio::spawn(async move {
        info!("dashboard update task started");
        while let Some(update) = updates_rx.recv().await {
            let now = Instant::now();
            debug!(
                "received normalized update for controller='{}' faults_present={}",
                update.controller,
                update.active_faults.is_some()
            );
            {
                let mut store = state_for_updates.store.write().await;
                store.apply_update(update, now);
            }
            if let Err(e) = state_for_updates.publish_if_changed().await {
                warn!("failed to publish dashboard update: {e}");
            }
        }
    });

    let state_for_sweep = state.clone();
    tokio::spawn(async move {
        debug!(
            "dashboard liveness sweep started with interval={}ms",
            DEFAULT_SWEEP_INTERVAL_MS
        );
        let mut interval = tokio::time::interval(Duration::from_millis(DEFAULT_SWEEP_INTERVAL_MS));
        loop {
            interval.tick().await;
            if let Err(e) = state_for_sweep.publish_if_changed().await {
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

    let events = warp::path("events")
        .and(warp::get())
        .and(state_filter.clone())
        .map(|state: AppState| {
            info!(
                "SSE client connected; subscriber count will become {}",
                state.events.receiver_count() + 1
            );
            let initial = futures::stream::once({
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

            let updates = futures::stream::unfold(state.events.subscribe(), move |mut rx| {
                let state = state.clone();
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

            warp::sse::reply(warp::sse::keep_alive().stream(initial.chain(updates)))
        });

    let health = warp::path("healthz")
        .and(warp::get())
        .map(|| warp::reply::with_status("ok", warp::http::StatusCode::OK));

    let routes = home.or(controller).or(events).or(health);
    let addr = ([0, 0, 0, 0], opts.port);
    info!(
        "starting HTTP server on http://0.0.0.0:{} with routes '/', '/controllers/:name', '/events', '/healthz'",
        opts.port
    );
    warp::serve(routes).run(addr).await;
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
        &HomeTemplate::new(&snapshot, initial_state_json),
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
            &ErrorTemplate::not_found(&controller_name),
            warp::http::StatusCode::NOT_FOUND,
        ));
    };

    let initial_controller_json = serde_json::to_string(controller).unwrap_or_else(|_| {
        serde_json::to_string(&ControllerStatus {
            name: controller_name.clone(),
            online: false,
            last_seen_ms: None,
            faults: Vec::new(),
        })
        .unwrap()
    });

    Ok(render_template_response(
        &ControllerTemplate::new(controller, initial_controller_json),
        warp::http::StatusCode::OK,
    ))
}

fn load_controller_names(path: &str) -> Result<Vec<String>> {
    debug!("loading UDS manifest from '{}'", path);
    let manifest: UdsManifest = serde_yaml::from_reader(
        std::fs::File::open(path).with_context(|| format!("opening UDS manifest '{path}'"))?,
    )
    .with_context(|| format!("parsing UDS manifest '{path}'"))?;

    let mut controller_names: Vec<String> = manifest
        .nodes
        .into_iter()
        .filter_map(|(name, node)| {
            let controller_name = name.to_ascii_lowercase();
            if node.request_id == 0 || node.response_id == 0 {
                return None;
            }
            if !is_supported_controller(&controller_name) {
                info!("ignoring unsupported controller '{controller_name}' from UDS manifest");
                return None;
            }
            Some(controller_name)
        })
        .collect();
    controller_names.sort();
    Ok(controller_names)
}

fn is_supported_controller(controller_name: &str) -> bool {
    SUPPORTED_CONTROLLERS.contains(&controller_name)
}

fn spawn_veh_reader(
    iface: String,
    tracked_controllers: Arc<BTreeSet<String>>,
    updates_tx: mpsc::UnboundedSender<NormalizedUpdate>,
) {
    thread::spawn(move || {
        let iface_map = [("vcanVeh", veh::Bus::VcanVeh)];
        info!("spawning vehicle CAN reader thread for iface='{iface}'");
        loop {
            let binding = match veh::configure_iface(&iface, &iface_map) {
                Ok(binding) => binding,
                Err(e) => {
                    warn!("failed to configure veh decoder for {iface}: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };
            let socket = match CanSocket::open(&iface) {
                Ok(socket) => socket,
                Err(e) => {
                    warn!("failed to open {iface}: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };

            info!("listening on {iface} for vehicle dashboard updates");
            loop {
                let frame = match socket.read_frame() {
                    Ok(frame) => frame,
                    Err(e) => {
                        warn!("read error on {iface}: {e}");
                        break;
                    }
                };

                if let Some(update) = decode_veh_frame(&binding, frame, &tracked_controllers) {
                    debug!("veh frame attributed to controller='{}'", update.controller);
                    if updates_tx.send(update).is_err() {
                        warn!("vehicle update channel closed; stopping reader thread");
                        return;
                    }
                }
            }
        }
    });
}

fn spawn_body_reader(
    iface: String,
    tracked_controllers: Arc<BTreeSet<String>>,
    updates_tx: mpsc::UnboundedSender<NormalizedUpdate>,
) {
    thread::spawn(move || {
        let iface_map = [("vcanBody", body::Bus::VcanBody)];
        info!("spawning body CAN reader thread for iface='{iface}'");
        loop {
            let binding = match body::configure_iface(&iface, &iface_map) {
                Ok(binding) => binding,
                Err(e) => {
                    warn!("failed to configure body decoder for {iface}: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };
            let socket = match CanSocket::open(&iface) {
                Ok(socket) => socket,
                Err(e) => {
                    warn!("failed to open {iface}: {e}");
                    thread::sleep(Duration::from_secs(1));
                    continue;
                }
            };

            info!("listening on {iface} for body dashboard updates");
            loop {
                let frame = match socket.read_frame() {
                    Ok(frame) => frame,
                    Err(e) => {
                        warn!("read error on {iface}: {e}");
                        break;
                    }
                };

                if let Some(update) = decode_body_frame(&binding, frame, &tracked_controllers) {
                    debug!(
                        "body frame attributed to controller='{}'",
                        update.controller
                    );
                    if updates_tx.send(update).is_err() {
                        warn!("body update channel closed; stopping reader thread");
                        return;
                    }
                }
            }
        }
    });
}

fn decode_veh_frame(
    binding: &veh::BusBinding<veh::Bus>,
    frame: socketcan::CanFrame,
    tracked_controllers: &BTreeSet<String>,
) -> Option<NormalizedUpdate> {
    let can_frame = convert_veh_frame(&frame)?;
    let id = masked_frame_id(&frame)?;
    let decoded = veh::maybe_decode(Some(binding), &can_frame, id, true, true, &[], &[])?;
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

fn decode_body_frame(
    binding: &body::BusBinding<body::Bus>,
    frame: socketcan::CanFrame,
    tracked_controllers: &BTreeSet<String>,
) -> Option<NormalizedUpdate> {
    let can_frame = convert_body_frame(&frame)?;
    let id = masked_frame_id(&frame)?;
    let decoded = body::maybe_decode(Some(binding), &can_frame, id, true, true, &[], &[])?;
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

fn normalize_update(
    bus_name: &str,
    message_name: String,
    members: Vec<PlainMeasurement>,
    tracked_controllers: &BTreeSet<String>,
    seen_at_ms: u64,
) -> Option<NormalizedUpdate> {
    let controller = controller_key_for_message_name(&message_name, tracked_controllers)?;
    debug!(
        "normalized bus='{}' message='{}' to controller='{}'",
        bus_name, message_name, controller
    );
    let active_faults = if is_fault_message(&message_name) {
        let faults = extract_active_faults(&message_name, &members, seen_at_ms);
        info!(
            "fault message '{}' produced {} active fault(s) for controller='{}'",
            message_name,
            faults.len(),
            controller
        );
        Some(faults)
    } else {
        None
    };

    Some(NormalizedUpdate {
        controller,
        seen_at_ms,
        active_faults,
    })
}

fn convert_veh_frame(frame: &socketcan::CanFrame) -> Option<veh::CanFrame> {
    if frame.is_remote_frame() || frame.is_error_frame() {
        return None;
    }

    let data = frame.data();
    let mut payload = [0u8; 8];
    payload[..data.len().min(8)].copy_from_slice(&data[..data.len().min(8)]);

    Some(veh::CanFrame {
        can_id: raw_can_id(frame),
        can_dlc: data.len() as u8,
        data: payload,
    })
}

fn convert_body_frame(frame: &socketcan::CanFrame) -> Option<body::CanFrame> {
    if frame.is_remote_frame() || frame.is_error_frame() {
        return None;
    }

    let data = frame.data();
    let mut payload = [0u8; 8];
    payload[..data.len().min(8)].copy_from_slice(&data[..data.len().min(8)]);

    Some(body::CanFrame {
        can_id: raw_can_id(frame),
        can_dlc: data.len() as u8,
        data: payload,
    })
}

fn raw_can_id(frame: &socketcan::CanFrame) -> u32 {
    match frame.id() {
        Id::Standard(id) => id.as_raw() as u32,
        Id::Extended(id) => id.as_raw(),
    }
}

fn masked_frame_id(frame: &socketcan::CanFrame) -> Option<u32> {
    if frame.is_remote_frame() || frame.is_error_frame() {
        None
    } else {
        Some(raw_can_id(frame))
    }
}

fn is_fault_message(message_name: &str) -> bool {
    message_name.to_ascii_lowercase().ends_with("_faults")
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

fn now_ms() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}

fn render_template_response<T: Template>(
    template: &T,
    status: warp::http::StatusCode,
) -> warp::reply::Response {
    match template.render() {
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
    fn snapshot_marks_controller_offline_after_timeout() {
        let mut store = DashboardStore::new(&["vcfront".to_string()], Duration::from_secs(3));
        let base = Instant::now();
        store.apply_update(
            NormalizedUpdate {
                controller: "vcfront".to_string(),
                seen_at_ms: 100,
                active_faults: None,
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
            },
            now,
        );
        store.apply_update(
            NormalizedUpdate {
                controller: "vcfront".to_string(),
                seen_at_ms: 200,
                active_faults: Some(Vec::new()),
            },
            now + Duration::from_secs(1),
        );

        let snapshot = store.snapshot(now + Duration::from_secs(1));
        assert!(snapshot.controllers[0].faults.is_empty());
    }
}
