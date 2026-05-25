use std::collections::{BTreeMap, BTreeSet};
use std::convert::Infallible;
use std::ffi::CString;
use std::fs;
use std::io::{self, BufRead, BufReader, BufWriter, Write};
use std::mem::{size_of, zeroed};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};
use chrono::SecondsFormat;
use clap::Parser;
use conUDS::FlashStatus;
use conUDS::SupportedResetTypes;
use conUDS::config::Config as UdsConfig;
use conUDS::modules::uds::{
    DiagnosticSessionKind, DiagnosticSessionResponse, RoutineStartResponse, UdsWorkerHandle,
};
use futures::StreamExt;
use libc::{
    AF_CAN, CAN_EFF_FLAG, CAN_EFF_MASK, CAN_ERR_FLAG, CAN_RAW, CAN_RTR_FLAG, CAN_SFF_MASK, EINTR,
    SO_TIMESTAMPING, SOCK_RAW, SOF_TIMESTAMPING_RAW_HARDWARE, SOF_TIMESTAMPING_RX_HARDWARE,
    SOF_TIMESTAMPING_RX_SOFTWARE, SOF_TIMESTAMPING_SOFTWARE, SOL_SOCKET, bind, c_void, can_frame,
    if_nametoindex, iovec, msghdr, recvmsg, sa_family_t, sockaddr, sockaddr_can, socket, socklen_t,
};
use log::{debug, info, warn};
use net_detec::{Client as MdnsClient, DiscoveryFilter};
use reqwest::StatusCode as HttpStatusCode;
use serde::{Deserialize, Serialize};
use tokio::sync::{Mutex, RwLock, broadcast, mpsc, oneshot};
use warp::Buf;
use warp::multipart::{FormData, Part};
use warp::sse::Event;
use warp::{Filter, Reply};

mod views;

use yamcan_dashboard as yamcan;
use yamcan_dashboard::NetworkBus;

const DEFAULT_PORT: u16 = 8091;
const DEFAULT_DATABASE_PORT: u16 = 8086;
const DEFAULT_OFFLINE_TIMEOUT_SECS: u64 = 3;
const DEFAULT_SWEEP_INTERVAL_MS: u64 = 250;
const OTA_AGENT_DISCOVERY_TIMEOUT_SECS: u64 = 2;
const OTA_AGENT_SERVICE_NAME: &str = "_ota-agent._tcp.local.";
const SUPPORTED_CONTROLLERS: &[&str] = &[
    "bmsb", "bmsw0", "bmsw1", "bmsw2", "bmsw3", "bmsw4", "bmsw5", "bmsw6", "bmsw7", "sws",
    "vcfront", "vcrear", "vcpdu", "pm100dx",
];
const DEFAULT_MAP_STORE_DIR: &str = "/var/lib/car-dashboard/maps";
const OPENSTREETMAP_MAP_TILE_SOURCE_ID: &str = "streets";
const OPENSTREETMAP_MAP_TILE_TEMPLATE: &str = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
const DEFAULT_MAP_TILE_SOURCE_ID: &str = OPENSTREETMAP_MAP_TILE_SOURCE_ID;
const DEFAULT_MAP_TILE_TEMPLATE: &str = OPENSTREETMAP_MAP_TILE_TEMPLATE;
const MAP_TILE_USER_AGENT: &str = "cfr-car-dashboard/0.1 offline-map-cache";
const MAX_MAP_ZOOM: u8 = 19;
const SIGNAL_SAMPLE_BATCH_INTERVAL_MS: u64 = 100;
const SIGNAL_EVENT_QUEUE_CAPACITY: usize = 4096;
const SIGNAL_BROADCAST_QUEUE_CAPACITY: usize = 8;
const MAX_MAP_TILE_UPLOAD_BYTES: u64 = 1024 * 1024;
const PNG_SIGNATURE: &[u8; 8] = b"\x89PNG\r\n\x1a\n";
const JPEG_SIGNATURE: &[u8; 3] = b"\xff\xd8\xff";
const OSM_BLOCKED_HEADER: &str = "x-blocked";
const WEB_MERCATOR_MAX_LAT: f64 = 85.05112878;
const EARTH_KM_PER_DEG_LAT: f64 = 111.32;

#[derive(Debug, Clone, Copy)]
struct MapTileSource {
    id: &'static str,
    name: &'static str,
    template: &'static str,
}

const MAP_TILE_SOURCES: &[MapTileSource] = &[MapTileSource {
    id: OPENSTREETMAP_MAP_TILE_SOURCE_ID,
    name: "Streets",
    template: OPENSTREETMAP_MAP_TILE_TEMPLATE,
}];

#[derive(Debug, Parser, Clone)]
#[command(name = "dashboard", about = "Live carputer dashboard over CAN")]
pub struct Opts {
    #[arg(long, default_value_t = DEFAULT_PORT)]
    pub port: u16,

    #[arg(long, default_value_t = DEFAULT_DATABASE_PORT)]
    pub database_port: u16,

    #[arg(long)]
    pub database_viewer_token: Option<String>,

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

    #[arg(
        long,
        default_value = "/application/config/ota-agent/deploy-targets.yaml"
    )]
    pub deploy_targets_manifest: String,

    #[arg(long, default_value = "can0")]
    pub veh_iface: String,

    #[arg(long, default_value = "can1")]
    pub body_iface: Option<String>,

    #[arg(long, default_value = DEFAULT_MAP_STORE_DIR)]
    pub map_store_dir: String,

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

#[derive(Debug, Deserialize)]
struct DeployTargetsManifest {
    targets: BTreeMap<String, DeployTargetConfig>,
}

#[derive(Debug, Deserialize)]
struct DeployTargetConfig {
    kind: String,
    #[serde(default)]
    requires_manual_recovery: bool,
}

#[derive(Debug, Clone)]
struct ControllerCapability {
    name: String,
    request_id: u32,
    response_id: u32,
    uds_iface: String,
    supports_manual_recovery: bool,
    requires_manual_recovery: bool,
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
    job: DashboardJob,
}

#[derive(Debug, Deserialize)]
struct OtaAgentRecoverReply {
    status: String,
    node: String,
}

#[derive(Debug, Deserialize)]
struct OtaAgentErrorReply {
    error: Option<String>,
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

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum SignalPlotKind {
    Numeric,
    Boolean,
    Enum,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct SignalManifestEntry {
    pub id: String,
    pub bus: String,
    pub message_name: String,
    pub message_id: u32,
    pub signal_name: String,
    pub unit: Option<String>,
    pub kind: SignalPlotKind,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct SignalManifestResponse {
    pub signals: Vec<SignalManifestEntry>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SignalSample {
    pub signal_id: String,
    pub signal_name: String,
    pub value: f64,
    pub label: Option<String>,
    pub unit: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SignalSampleEvent {
    pub timestamp_ms: u64,
    pub bus: String,
    pub message_name: String,
    pub message_id: u32,
    pub samples: Vec<SignalSample>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SignalSampleBatch {
    pub events: Vec<SignalSampleEvent>,
}

#[derive(Debug, Clone)]
struct PlainMeasurement {
    name: String,
    value: f64,
    unit: Option<String>,
    label: Option<String>,
}

struct FlashUpload {
    path: PathBuf,
    display_name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct NormalizedUpdate {
    controller: String,
    seen_at_ms: u64,
    active_faults: Option<Vec<ActiveFault>>,
    critical_signals: Option<Vec<LiveSignal>>,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct SignalEventQuery {
    #[serde(default)]
    signals: String,
}

#[derive(Debug, Clone)]
struct MapStoreConfig {
    root: PathBuf,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
struct MapBounds {
    min_lat: f64,
    max_lat: f64,
    min_lon: f64,
    max_lon: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
struct MapTileLevel {
    zoom: u8,
    min_x: u32,
    max_x: u32,
    min_y: u32,
    max_y: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
struct MapView {
    id: String,
    name: String,
    center_lat: f64,
    center_lon: f64,
    radius_km: f64,
    min_zoom: u8,
    max_zoom: u8,
    created_at_ms: u64,
    tile_count: usize,
    #[serde(default)]
    tile_source_id: String,
    #[serde(default)]
    tile_source_name: String,
    tile_source: String,
    bounds: MapBounds,
    levels: Vec<MapTileLevel>,
    #[serde(default)]
    tiles: Vec<TileCoord>,
}

#[derive(Debug, Clone, Serialize)]
struct MapViewsResponse {
    ok: bool,
    store_dir: String,
    views: Vec<MapView>,
}

#[derive(Debug, Clone, Serialize)]
struct MapDebugResponse {
    ok: bool,
    store_dir: String,
    views: Vec<MapDebugView>,
}

#[derive(Debug, Clone, Serialize)]
struct MapDebugView {
    id: String,
    name: String,
    center_lat: f64,
    center_lon: f64,
    tile_count: usize,
    zooms: Vec<u8>,
    sample_tiles: Vec<MapDebugTile>,
}

#[derive(Debug, Clone, Serialize)]
struct MapDebugTile {
    z: u8,
    x: u32,
    y: u32,
    center_lat: f64,
    center_lon: f64,
    path: String,
    exists: bool,
    png: bool,
    image_type: Option<String>,
    valid: bool,
    size_bytes: Option<u64>,
    detail: Option<String>,
}

#[derive(Debug, Clone, Deserialize)]
struct MapDownloadRequest {
    name: Option<String>,
    #[serde(default)]
    source: Option<String>,
    lat: f64,
    lon: f64,
    radius_km: Option<f64>,
    min_zoom: Option<u8>,
    max_zoom: Option<u8>,
}

#[derive(Debug, Clone, Serialize)]
struct MapDownloadResponse {
    ok: bool,
    view: MapView,
    downloaded_tiles: usize,
    existing_tiles: usize,
    failed_tiles: usize,
    first_error: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
struct MapTileDownloadPlan {
    source: String,
    z: u8,
    x: u32,
    y: u32,
    url: String,
    cached: bool,
}

#[derive(Debug, Clone, Serialize)]
struct MapDownloadPlanResponse {
    ok: bool,
    view: MapView,
    tiles: Vec<MapTileDownloadPlan>,
    existing_tiles: usize,
    missing_tiles: usize,
}

#[derive(Debug, Clone, Deserialize)]
struct MapCommitRequest {
    view: MapView,
}

#[derive(Debug, Clone, Serialize)]
struct MapCommitResponse {
    ok: bool,
    view: MapView,
    existing_tiles: usize,
    missing_tiles: usize,
}

#[derive(Debug, Clone, Serialize)]
struct MapTileUploadResponse {
    ok: bool,
    z: u8,
    x: u32,
    y: u32,
    size_bytes: usize,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct MapTileQuery {
    #[serde(default)]
    source: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
struct MapDeleteResponse {
    ok: bool,
    deleted_view: MapView,
    removed_tiles: usize,
    remaining_views: usize,
}

#[derive(Debug, Clone, Serialize)]
struct ClockResponse {
    ok: bool,
    unix_ms: u64,
    local_time: String,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
struct TileCoord {
    z: u8,
    x: u32,
    y: u32,
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

struct TesterPresentHandle {
    stop_tx: oneshot::Sender<String>,
}

impl MapStoreConfig {
    fn views_path(&self) -> PathBuf {
        self.root.join("views.json")
    }

    fn tiles_dir(&self) -> PathBuf {
        self.root.join("tiles")
    }

    fn legacy_tile_path(&self, z: u8, x: u32, y: u32) -> PathBuf {
        self.tiles_dir()
            .join(z.to_string())
            .join(x.to_string())
            .join(format!("{y}.png"))
    }

    fn tile_path_for_source_id(&self, source_id: &str, z: u8, x: u32, y: u32) -> PathBuf {
        self.tiles_dir()
            .join(source_id)
            .join(z.to_string())
            .join(x.to_string())
            .join(format!("{y}.tile"))
    }

    fn tile_paths_for_source_id(&self, source_id: &str, z: u8, x: u32, y: u32) -> Vec<PathBuf> {
        let mut paths = vec![self.tile_path_for_source_id(source_id, z, x, y)];
        if source_id == OPENSTREETMAP_MAP_TILE_SOURCE_ID {
            paths.push(self.legacy_tile_path(z, x, y));
        }
        paths
    }
}

#[derive(Debug, Serialize)]
struct TesterPresentStateResponse {
    ok: bool,
    enabled: bool,
    job_id: Option<String>,
    job: Option<DashboardJob>,
}

#[derive(Debug, Serialize)]
struct CurrentSessionResponse {
    ok: bool,
    session_key: String,
    session_label: String,
    session_value: u8,
}

#[derive(Clone)]
struct AppState {
    store: Arc<RwLock<DashboardStore>>,
    capabilities: Arc<BTreeMap<String, ControllerCapability>>,
    uds_workers: Arc<BTreeMap<String, UdsWorkerHandle>>,
    signal_manifest: Arc<SignalManifestResponse>,
    veh_iface: Arc<String>,
    body_iface: Arc<Option<String>>,
    state_events: broadcast::Sender<String>,
    job_events: broadcast::Sender<String>,
    signal_events: broadcast::Sender<Arc<SignalSampleBatch>>,
    last_state_payload: Arc<Mutex<String>>,
    last_jobs_payload: Arc<Mutex<String>>,
    jobs: Arc<RwLock<JobStore>>,
    tester_present: Arc<Mutex<BTreeMap<String, TesterPresentHandle>>>,
    map_store: Arc<MapStoreConfig>,
    map_store_lock: Arc<Mutex<()>>,
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

    fn create_background_job(
        &mut self,
        controller: &str,
        operation: String,
        detail: String,
    ) -> DashboardJob {
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
        self.jobs.insert(id, job.clone());
        job
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
            if self.active_by_controller.get(&job.controller) == Some(&job.id) {
                self.active_by_controller.remove(&job.controller);
            }
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
            if self.active_by_controller.get(&job.controller) == Some(&job.id) {
                self.active_by_controller.remove(&job.controller);
            }
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

    fn signal_manifest_json(&self) -> Result<String> {
        serde_json::to_string(&*self.signal_manifest).context("serializing signal manifest")
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
        "initializing dashboard with uds_manifest='{}', routine_manifest='{}', deploy_targets_manifest='{}', ota_agent_service_name='{}', veh_iface='{}', body_iface='{}', map_store_dir='{}', port={}",
        opts.uds_manifest,
        opts.routine_manifest,
        opts.deploy_targets_manifest,
        OTA_AGENT_SERVICE_NAME,
        opts.veh_iface,
        opts.body_iface.as_deref().unwrap_or("disabled"),
        opts.map_store_dir,
        opts.port
    );
    let capabilities = Arc::new(load_controller_capabilities(
        &opts.uds_manifest,
        &opts.routine_manifest,
        &opts.deploy_targets_manifest,
        &opts.veh_iface,
    )?);
    let signal_manifest = Arc::new(build_signal_manifest());
    let controller_names = tracked_controller_names();
    let uds_workers = Arc::new(
        capabilities
            .iter()
            .map(|(name, capability)| {
                (
                    name.clone(),
                    UdsWorkerHandle::new(
                        capability.uds_iface.clone(),
                        capability.request_id,
                        capability.response_id,
                        false,
                    ),
                )
            })
            .collect::<BTreeMap<_, _>>(),
    );
    info!(
        "loaded {} deployable controller(s) from UDS manifest: {}",
        capabilities.len(),
        capabilities.keys().cloned().collect::<Vec<_>>().join(", ")
    );
    info!(
        "tracking {} controller(s) on the dashboard homepage: {}",
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
    let map_store = Arc::new(MapStoreConfig {
        root: PathBuf::from(&opts.map_store_dir),
    });
    let (state_events, _) = broadcast::channel(64);
    let (job_events, _) = broadcast::channel(64);
    let (signal_events, _) = broadcast::channel(SIGNAL_BROADCAST_QUEUE_CAPACITY);
    let state = AppState {
        store,
        capabilities,
        uds_workers,
        signal_manifest,
        veh_iface: Arc::new(opts.veh_iface.clone()),
        body_iface: Arc::new(opts.body_iface.clone()),
        state_events,
        job_events,
        signal_events,
        last_state_payload: Arc::new(Mutex::new(String::new())),
        last_jobs_payload: Arc::new(Mutex::new(String::new())),
        jobs,
        tester_present: Arc::new(Mutex::new(BTreeMap::new())),
        map_store,
        map_store_lock: Arc::new(Mutex::new(())),
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
        }
    });

    let state_for_sweep = state.clone();
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_millis(DEFAULT_SWEEP_INTERVAL_MS));
        loop {
            interval.tick().await;
            if state_for_sweep.state_events.receiver_count() == 0 {
                continue;
            }
            if let Err(e) = state_for_sweep.publish_state_if_changed().await {
                warn!("failed to publish sweep snapshot: {e}");
            }
        }
    });

    let signal_events_for_worker = state.signal_events.clone();
    let state_filter = warp::any().map(move || state.clone());

    let home = warp::path::end()
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_home);

    let signals = warp::path("signals")
        .and(warp::path::end())
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_signals);

    let gps = warp::path("gps")
        .and(warp::path::end())
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_gps);

    let controller = warp::path!("controllers" / String)
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_controller);

    let database_port = opts.database_port;
    let database_viewer_token = opts.database_viewer_token.clone();
    let database = warp::path("database")
        .and(warp::path::end())
        .and(warp::get())
        .and(warp::header::optional::<String>("host"))
        .map(move |host_header: Option<String>| {
            handle_database_redirect(host_header, database_port, database_viewer_token.as_deref())
        });

    let signal_manifest = warp::path!("api" / "signals" / "manifest")
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_signal_manifest);

    let map_views = warp::path!("api" / "maps" / "views")
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_map_views);

    let map_debug = warp::path!("api" / "maps" / "debug")
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_map_debug);

    let create_map_view = warp::path!("api" / "maps" / "views")
        .and(warp::post())
        .and(warp::body::content_length_limit(32 * 1024))
        .and(warp::body::json())
        .and(state_filter.clone())
        .and_then(handle_create_map_view);

    let delete_map_view = warp::path!("api" / "maps" / "views" / String)
        .and(warp::delete())
        .and(state_filter.clone())
        .and_then(handle_delete_map_view);

    let plan_map_view = warp::path!("api" / "maps" / "views" / "plan")
        .and(warp::post())
        .and(warp::body::content_length_limit(32 * 1024))
        .and(warp::body::json())
        .and(state_filter.clone())
        .and_then(handle_plan_map_view);

    let commit_map_view = warp::path!("api" / "maps" / "views" / "commit")
        .and(warp::post())
        .and(warp::body::content_length_limit(256 * 1024))
        .and(warp::body::json())
        .and(state_filter.clone())
        .and_then(handle_commit_map_view);

    let map_tile = warp::path!("api" / "maps" / "tiles" / u8 / u32 / u32)
        .and(warp::get())
        .and(warp::query::<MapTileQuery>())
        .and(state_filter.clone())
        .and_then(handle_map_tile);

    let upload_map_tile = warp::path!("api" / "maps" / "tiles" / u8 / u32 / u32)
        .and(warp::put())
        .and(warp::query::<MapTileQuery>())
        .and(warp::body::content_length_limit(MAX_MAP_TILE_UPLOAD_BYTES))
        .and(warp::body::bytes())
        .and(state_filter.clone())
        .and_then(handle_upload_map_tile);

    let uplot_js = warp::path!("assets" / "uPlot.iife.min.js")
        .and(warp::get())
        .map(handle_uplot_js);

    let uplot_css = warp::path!("assets" / "uPlot.min.css")
        .and(warp::get())
        .map(handle_uplot_css);

    let signal_cache_worker_js = warp::path!("assets" / "signal-cache-worker.js")
        .and(warp::get())
        .map(handle_signal_cache_worker_js);

    let enter_session = warp::path!("api" / "controllers" / String / "session")
        .and(warp::post())
        .and(warp::body::form())
        .and(state_filter.clone())
        .and_then(handle_enter_session);

    let read_current_session = warp::path!("api" / "controllers" / String / "current-session")
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_current_session);

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

    let flash_controller = warp::path!("api" / "controllers" / String / "flash")
        .and(warp::post())
        .and(warp::multipart::form().max_length(64 * 1024 * 1024))
        .and(state_filter.clone())
        .and_then(handle_flash_controller);

    let recover_controller = warp::path!("api" / "controllers" / String / "recover")
        .and(warp::post())
        .and(state_filter.clone())
        .and_then(handle_recover_controller);

    let start_tester_present = warp::path!("api" / "controllers" / String / "tester-present")
        .and(warp::post())
        .and(state_filter.clone())
        .and_then(handle_start_tester_present);

    let send_tester_present =
        warp::path!("api" / "controllers" / String / "tester-present" / "request")
            .and(warp::post())
            .and(state_filter.clone())
            .and_then(handle_send_tester_present);

    let stop_tester_present = warp::path!("api" / "controllers" / String / "tester-present")
        .and(warp::delete())
        .and(state_filter.clone())
        .and_then(handle_stop_tester_present);

    let tester_present_state = warp::path!("api" / "controllers" / String / "tester-present")
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_tester_present_state);

    let controller_jobs = warp::path!("api" / "controllers" / String / "jobs")
        .and(warp::get())
        .and(state_filter.clone())
        .and_then(handle_controller_jobs);

    let signal_events_with_query = warp::path("signal-events")
        .and(warp::get())
        .and(warp::query::<SignalEventQuery>())
        .and(state_filter.clone())
        .map(|query: SignalEventQuery, state: AppState| {
            let selected_ids = query
                .signals
                .split(',')
                .filter(|value| !value.is_empty())
                .map(str::to_string)
                .collect::<BTreeSet<_>>();
            signal_events_reply(state, selected_ids)
        });

    let clock = warp::path!("api" / "clock")
        .and(warp::get())
        .map(handle_clock);

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
        .or(signals)
        .or(gps)
        .or(controller)
        .or(database)
        .or(signal_manifest)
        .or(map_views)
        .or(map_debug)
        .or(create_map_view)
        .or(delete_map_view)
        .or(plan_map_view)
        .or(commit_map_view)
        .or(map_tile)
        .or(upload_map_tile)
        .or(uplot_js)
        .or(uplot_css)
        .or(signal_cache_worker_js)
        .or(read_current_session)
        .or(enter_session)
        .or(run_routine)
        .or(reset_controller)
        .or(flash_controller)
        .or(recover_controller)
        .or(send_tester_present)
        .or(start_tester_present)
        .or(stop_tester_present)
        .or(tester_present_state)
        .or(controller_jobs)
        .or(signal_events_with_query)
        .or(clock)
        .or(events)
        .or(health);
    let addr = ([0, 0, 0, 0], opts.port);
    info!(
        "starting HTTP server on http://0.0.0.0:{} with routes '/', '/signals', '/gps', '/database', '/controllers/:name', '/api/signals/manifest', '/api/maps/views', '/api/maps/debug', '/api/maps/views/:id', '/api/maps/views/plan', '/api/maps/views/commit', '/api/maps/tiles/:z/:x/:y', '/api/clock', '/assets/uPlot.iife.min.js', '/assets/uPlot.min.css', '/assets/signal-cache-worker.js', '/api/controllers/:name/current-session', '/api/controllers/:name/session', '/api/controllers/:name/routines/:routine', '/api/controllers/:name/reset', '/api/controllers/:name/flash', '/api/controllers/:name/recover', '/api/controllers/:name/tester-present', '/api/controllers/:name/tester-present/request', '/api/controllers/:name/jobs', '/events', '/signal-events', '/healthz'",
        opts.port
    );
    let (_, server) = warp::serve(routes)
        .try_bind_ephemeral(addr)
        .context(format!(
            "binding dashboard HTTP server to 0.0.0.0:{}",
            opts.port
        ))?;

    spawn_veh_worker(
        opts.veh_iface.clone(),
        Arc::clone(&tracked_controllers),
        updates_tx.clone(),
    );
    spawn_signal_broadcast_worker(
        opts.veh_iface.clone(),
        yamcan::Bus::Veh,
        signal_events_for_worker,
    );

    server.await;
    Ok(())
}

async fn handle_home(state: AppState) -> Result<warp::reply::Response, Infallible> {
    debug!("serving dashboard homepage");
    let snapshot = state.snapshot().await;
    let deployable_controllers = state.capabilities.keys().cloned().collect::<BTreeSet<_>>();
    let initial_state_json = serde_json::to_string(&snapshot).unwrap_or_else(|_| {
        serde_json::to_string(&DashboardSnapshot {
            controllers: Vec::new(),
        })
        .unwrap()
    });
    Ok(render_template_response(
        views::render_home(&snapshot, &deployable_controllers, initial_state_json),
        warp::http::StatusCode::OK,
    ))
}

async fn handle_signals(state: AppState) -> Result<warp::reply::Response, Infallible> {
    debug!("serving signal explorer page");
    let initial_manifest_json = state.signal_manifest_json().unwrap_or_else(|_| {
        serde_json::to_string(&SignalManifestResponse {
            signals: Vec::new(),
        })
        .unwrap()
    });
    Ok(render_template_response(
        views::render_signals(&initial_manifest_json),
        warp::http::StatusCode::OK,
    ))
}

fn handle_database_redirect(
    host_header: Option<String>,
    database_port: u16,
    viewer_token: Option<&str>,
) -> warp::reply::Response {
    let host = database_host_from_header(host_header.as_deref())
        .unwrap_or_else(|| "localhost".to_string());
    let mut location = format!("http://{}:{}/", host, database_port);
    if let Some(token) = viewer_token {
        location.push_str("?token=");
        location.push_str(&percent_encode_query_value(token));
    }

    let location = match warp::http::HeaderValue::from_str(&location) {
        Ok(value) => value,
        Err(error) => {
            warn!("rejecting database redirect for invalid host header: {error}");
            return warp::reply::with_status(
                "invalid database redirect host",
                warp::http::StatusCode::BAD_REQUEST,
            )
            .into_response();
        }
    };

    let mut response =
        warp::reply::with_status("", warp::http::StatusCode::TEMPORARY_REDIRECT).into_response();
    response
        .headers_mut()
        .insert(warp::http::header::LOCATION, location);
    response
}

fn database_host_from_header(host_header: Option<&str>) -> Option<String> {
    let host = host_header?.trim();
    if host.is_empty() {
        return None;
    }

    if host.starts_with('[') {
        let end = host.find(']')?;
        return Some(host[..=end].to_string());
    }

    let colon_count = host.bytes().filter(|byte| *byte == b':').count();
    if colon_count == 1 {
        let (hostname, _) = host.split_once(':')?;
        let hostname = hostname.trim();
        return (!hostname.is_empty()).then(|| hostname.to_string());
    }
    if colon_count > 1 {
        return Some(format!("[{host}]"));
    }

    Some(host.to_string())
}

fn percent_encode_query_value(value: &str) -> String {
    let mut encoded = String::with_capacity(value.len());
    for byte in value.bytes() {
        match byte {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'.' | b'_' | b'~' => {
                encoded.push(byte as char);
            }
            _ => encoded.push_str(&format!("%{byte:02X}")),
        }
    }
    encoded
}

async fn handle_gps(_state: AppState) -> Result<warp::reply::Response, Infallible> {
    debug!("serving GPS map page");
    Ok(render_template_response(
        views::render_gps(),
        warp::http::StatusCode::OK,
    ))
}

async fn handle_signal_manifest(state: AppState) -> Result<warp::reply::Response, Infallible> {
    Ok(warp::reply::with_status(
        warp::reply::json(&*state.signal_manifest),
        warp::http::StatusCode::OK,
    )
    .into_response())
}

fn handle_clock() -> warp::reply::Response {
    warp::reply::with_status(
        warp::reply::json(&ClockResponse {
            ok: true,
            unix_ms: now_ms(),
            local_time: chrono::Local::now().to_rfc3339_opts(SecondsFormat::Secs, false),
        }),
        warp::http::StatusCode::OK,
    )
    .into_response()
}

async fn handle_map_views(state: AppState) -> Result<warp::reply::Response, Infallible> {
    match load_map_views(&state.map_store).await {
        Ok(views) => Ok(json_map_views_response(MapViewsResponse {
            ok: true,
            store_dir: state.map_store.root.display().to_string(),
            views,
        })),
        Err(error) => Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            &error.to_string(),
        )),
    }
}

async fn handle_map_debug(state: AppState) -> Result<warp::reply::Response, Infallible> {
    match debug_map_cache(&state.map_store).await {
        Ok(response) => Ok(json_map_debug_response(response)),
        Err(error) => Ok(json_error_response(
            warp::http::StatusCode::INTERNAL_SERVER_ERROR,
            &error.to_string(),
        )),
    }
}

async fn handle_create_map_view(
    request: MapDownloadRequest,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let _guard = state.map_store_lock.lock().await;
    match download_map_view(&state.map_store, request).await {
        Ok(response) => Ok(json_map_download_response(response)),
        Err(error) => {
            let message = error.to_string();
            let status = if message.contains("no map tiles could be downloaded") {
                warp::http::StatusCode::BAD_GATEWAY
            } else {
                warp::http::StatusCode::BAD_REQUEST
            };
            Ok(json_error_response(status, &message))
        }
    }
}

async fn handle_delete_map_view(
    view_id: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let _guard = state.map_store_lock.lock().await;
    match delete_map_view(&state.map_store, &view_id).await {
        Ok(response) => Ok(json_map_delete_response(response)),
        Err(error) => {
            let message = error.to_string();
            let status = if message.contains("not found") {
                warp::http::StatusCode::NOT_FOUND
            } else {
                warp::http::StatusCode::INTERNAL_SERVER_ERROR
            };
            Ok(json_error_response(status, &message))
        }
    }
}

async fn handle_plan_map_view(
    request: MapDownloadRequest,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    match plan_map_view(&state.map_store, request).await {
        Ok(response) => Ok(json_map_plan_response(response)),
        Err(error) => Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            &error.to_string(),
        )),
    }
}

async fn handle_commit_map_view(
    request: MapCommitRequest,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let _guard = state.map_store_lock.lock().await;
    match commit_map_view(&state.map_store, request.view).await {
        Ok(response) => Ok(json_map_commit_response(response)),
        Err(error) => Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            &error.to_string(),
        )),
    }
}

async fn handle_map_tile(
    z: u8,
    x: u32,
    y: u32,
    query: MapTileQuery,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    if !valid_tile_coord(z, x, y) {
        return Ok(
            warp::reply::with_status("tile not found", warp::http::StatusCode::NOT_FOUND)
                .into_response(),
        );
    }

    let source = match selected_map_tile_source(query.source.as_deref()) {
        Ok(source) => source,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_REQUEST,
                &error.to_string(),
            ));
        }
    };
    let tile = TileCoord { z, x, y };

    match read_cached_map_tile(&state.map_store, source, tile).await {
        Ok(Some(data)) => {
            let content_type = map_tile_content_type(&data).unwrap_or("application/octet-stream");
            Ok(warp::http::Response::builder()
                .status(warp::http::StatusCode::OK)
                .header("content-type", content_type)
                .header("cache-control", "no-store")
                .body(warp::hyper::Body::from(data))
                .unwrap_or_else(|_| {
                    warp::reply::with_status(
                        "tile response error",
                        warp::http::StatusCode::INTERNAL_SERVER_ERROR,
                    )
                    .into_response()
                }))
        }
        Ok(None) => Ok(warp::reply::with_status(
            "tile not found",
            warp::http::StatusCode::NOT_FOUND,
        )
        .into_response()),
        Err(error) => Ok(json_error_response(
            warp::http::StatusCode::INTERNAL_SERVER_ERROR,
            &error.to_string(),
        )),
    }
}

async fn handle_upload_map_tile(
    z: u8,
    x: u32,
    y: u32,
    query: MapTileQuery,
    data: warp::hyper::body::Bytes,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    if !valid_tile_coord(z, x, y) {
        return Ok(
            warp::reply::with_status("tile not found", warp::http::StatusCode::NOT_FOUND)
                .into_response(),
        );
    }
    if data.is_empty() {
        return Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            "tile upload cannot be empty",
        ));
    }

    let tile = TileCoord { z, x, y };
    let source = match selected_map_tile_source(query.source.as_deref()) {
        Ok(source) => source,
        Err(error) => {
            warn!("rejected uploaded map tile z={z} x={x} y={y}: {error}");
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_REQUEST,
                &error.to_string(),
            ));
        }
    };
    match write_map_tile_bytes(&state.map_store, source, tile, data.as_ref()).await {
        Ok(()) => Ok(json_map_tile_upload_response(MapTileUploadResponse {
            ok: true,
            z,
            x,
            y,
            size_bytes: data.len(),
        })),
        Err(error) => {
            let message = error.to_string();
            warn!(
                "rejected uploaded map tile source={} z={} x={} y={} size={} bytes: {}",
                source.id,
                z,
                x,
                y,
                data.len(),
                message
            );
            let status = if message.starts_with("tile data ")
                || message.starts_with("tile PNG ")
                || message.starts_with("tile JPEG ")
                || message.starts_with("tile image ")
            {
                warp::http::StatusCode::BAD_REQUEST
            } else {
                warp::http::StatusCode::INTERNAL_SERVER_ERROR
            };
            Ok(json_error_response(status, &message))
        }
    }
}

fn handle_uplot_js() -> warp::reply::Response {
    warp::reply::with_header(
        include_str!("../static/uPlot.iife.min.js"),
        "content-type",
        "application/javascript; charset=utf-8",
    )
    .into_response()
}

fn handle_uplot_css() -> warp::reply::Response {
    warp::reply::with_header(
        include_str!("../static/uPlot.min.css"),
        "content-type",
        "text/css; charset=utf-8",
    )
    .into_response()
}

fn handle_signal_cache_worker_js() -> warp::reply::Response {
    warp::reply::with_header(
        include_str!("../static/signal-cache-worker.js"),
        "content-type",
        "application/javascript; charset=utf-8",
    )
    .into_response()
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

async fn handle_current_session(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    if !state.capabilities.contains_key(&controller_name) {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    }
    let Some(worker) = state.uds_workers.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::BAD_GATEWAY,
            "UDS worker unavailable for controller",
        ));
    };

    let result = worker.read_current_session().await;

    let session = match result {
        Ok(session) => session,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_GATEWAY,
                &error.to_string(),
            ));
        }
    };

    Ok(json_current_session_response(CurrentSessionResponse {
        ok: true,
        session_key: session.key().to_string(),
        session_label: session.label().to_string(),
        session_value: session.raw_value(),
    }))
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
        job_id: job.id.clone(),
        job,
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
        job_id: job.id.clone(),
        job,
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
        job_id: job.id.clone(),
        job,
    }))
}

async fn handle_flash_controller(
    controller_name: String,
    form: FormData,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };

    let upload = match receive_flash_upload(form).await {
        Ok(upload) => upload,
        Err(error) => {
            return Ok(json_error_response(
                warp::http::StatusCode::BAD_REQUEST,
                &error.to_string(),
            ));
        }
    };

    let detail = format!("Queued flash {}", upload.display_name);
    let job = match create_job(&state, &controller_name, "flash-ecu", detail).await {
        Ok(job) => job,
        Err(error) => {
            let _ = fs::remove_file(&upload.path);
            return Ok(json_error_response(
                warp::http::StatusCode::CONFLICT,
                &error.to_string(),
            ));
        }
    };

    let state_for_job = state.clone();
    let job_id = job.id.clone();
    tokio::spawn(async move {
        run_flash_job(
            state_for_job,
            capability,
            job_id,
            upload.path,
            upload.display_name,
        )
        .await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id.clone(),
        job,
    }))
}

async fn handle_recover_controller(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };

    if !capability.supports_manual_recovery {
        return Ok(json_error_response(
            warp::http::StatusCode::BAD_REQUEST,
            "manual recovery is not configured for this controller",
        ));
    }

    let detail = if capability.requires_manual_recovery {
        "Queued required manual recovery".to_string()
    } else {
        "Queued baseline recovery".to_string()
    };
    let job = match create_job(&state, &controller_name, "manual-recovery", detail).await {
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
        run_manual_recovery_job(state_for_job, capability, job_id).await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id.clone(),
        job,
    }))
}

async fn handle_controller_jobs(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    if !state.capabilities.contains_key(&controller_name) {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    }

    let snapshot = state.jobs_snapshot().await;
    Ok(json_jobs_response(snapshot))
}

async fn handle_start_tester_present(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };

    {
        let tester_present = state.tester_present.lock().await;
        if tester_present.contains_key(&controller_name) {
            return Ok(json_error_response(
                warp::http::StatusCode::CONFLICT,
                "persistent tester present is already active for this controller",
            ));
        }
    }

    let job = {
        let mut jobs = state.jobs.write().await;
        jobs.create_background_job(
            &controller_name,
            "tester-present".to_string(),
            "Queued persistent tester present".to_string(),
        )
    };
    if let Err(error) = state.publish_jobs_if_changed().await {
        return Ok(json_error_response(
            warp::http::StatusCode::INTERNAL_SERVER_ERROR,
            &error.to_string(),
        ));
    }

    let (stop_tx, stop_rx) = oneshot::channel();
    {
        let mut tester_present = state.tester_present.lock().await;
        tester_present.insert(controller_name.clone(), TesterPresentHandle { stop_tx });
    }

    let state_for_job = state.clone();
    let job_id = job.id.clone();
    tokio::spawn(async move {
        run_tester_present_job(state_for_job, capability, job_id, stop_rx).await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id.clone(),
        job,
    }))
}

async fn handle_send_tester_present(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    let Some(capability) = state.capabilities.get(&controller_name).cloned() else {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    };

    let job = match create_job(
        &state,
        &controller_name,
        "tester-present-request",
        "Queued tester present with response".to_string(),
    )
    .await
    {
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
        run_tester_present_request_job(state_for_job, capability, job_id).await;
    });

    Ok(json_success_response(ActionAcceptedResponse {
        ok: true,
        job_id: job.id.clone(),
        job,
    }))
}

async fn handle_stop_tester_present(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    if !state.capabilities.contains_key(&controller_name) {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    }

    let handle = {
        let mut tester_present = state.tester_present.lock().await;
        tester_present.remove(&controller_name)
    };

    let Some(handle) = handle else {
        return Ok(json_error_response(
            warp::http::StatusCode::CONFLICT,
            "persistent tester present is not active for this controller",
        ));
    };

    let job = {
        let mut jobs = state.jobs.write().await;
        jobs.create_background_job(
            &controller_name,
            "tester-present-stop".to_string(),
            "Queued stop persistent tester present".to_string(),
        )
    };
    if let Err(error) = state.publish_jobs_if_changed().await {
        return Ok(json_error_response(
            warp::http::StatusCode::INTERNAL_SERVER_ERROR,
            &error.to_string(),
        ));
    }

    let _ = handle.stop_tx.send(job.id.clone());
    Ok(json_tester_present_state_response(
        TesterPresentStateResponse {
            ok: true,
            enabled: false,
            job_id: Some(job.id.clone()),
            job: Some(job),
        },
        warp::http::StatusCode::OK,
    ))
}

async fn handle_tester_present_state(
    controller_name: String,
    state: AppState,
) -> Result<warp::reply::Response, Infallible> {
    let controller_name = controller_name.to_ascii_lowercase();
    if !state.capabilities.contains_key(&controller_name) {
        return Ok(json_error_response(
            warp::http::StatusCode::NOT_FOUND,
            "unknown controller",
        ));
    }

    let enabled = {
        let tester_present = state.tester_present.lock().await;
        tester_present.contains_key(&controller_name)
    };

    Ok(json_tester_present_state_response(
        TesterPresentStateResponse {
            ok: true,
            enabled,
            job_id: None,
            job: None,
        },
        warp::http::StatusCode::OK,
    ))
}

fn load_controller_capabilities(
    uds_manifest: &str,
    routine_manifest: &str,
    deploy_targets_manifest: &str,
    veh_iface: &str,
) -> Result<BTreeMap<String, ControllerCapability>> {
    debug!(
        "loading controller capabilities from uds_manifest='{}' routine_manifest='{}' deploy_targets_manifest='{}'",
        uds_manifest, routine_manifest, deploy_targets_manifest
    );
    let config = UdsConfig::new(uds_manifest, routine_manifest)?;
    let manual_recovery = load_manual_recovery_config(deploy_targets_manifest)?;
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
                name: controller_name.clone(),
                request_id: node.request_id,
                response_id: node.response_id,
                uds_iface: veh_iface.to_string(),
                supports_manual_recovery: manual_recovery.contains_key(&controller_name),
                requires_manual_recovery: manual_recovery
                    .get(&controller_name)
                    .copied()
                    .unwrap_or(false),
                sessions: default_session_options(),
                resets: default_reset_options(),
                routines,
            },
        );
    }
    Ok(capabilities)
}

fn load_manual_recovery_config(manifest_path: &str) -> Result<BTreeMap<String, bool>> {
    let raw = match fs::read_to_string(manifest_path) {
        Ok(raw) => raw,
        Err(error) if error.kind() == io::ErrorKind::NotFound => {
            warn!(
                "deploy targets manifest '{}' not found; manual recovery controls disabled",
                manifest_path
            );
            return Ok(BTreeMap::new());
        }
        Err(error) => {
            return Err(error)
                .with_context(|| format!("reading deploy targets manifest '{}'", manifest_path));
        }
    };

    let manifest: DeployTargetsManifest = serde_yaml::from_str(&raw)
        .with_context(|| format!("parsing deploy targets manifest '{}'", manifest_path))?;

    Ok(manifest
        .targets
        .into_iter()
        .filter_map(|(node, target)| {
            if target.kind == "uds" {
                Some((node.to_ascii_lowercase(), target.requires_manual_recovery))
            } else {
                None
            }
        })
        .collect())
}

fn is_supported_controller(controller_name: &str) -> bool {
    SUPPORTED_CONTROLLERS.contains(&controller_name)
}

fn tracked_controller_names() -> Vec<String> {
    SUPPORTED_CONTROLLERS
        .iter()
        .map(|controller| controller.to_string())
        .collect()
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
        DiagnosticSessionOption {
            key: "safety-system".to_string(),
            label: "Safety Diagnostic".to_string(),
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

async fn receive_flash_upload(form: FormData) -> Result<FlashUpload> {
    let mut parts = form;
    while let Some(part_result) = parts.next().await {
        let part = part_result?;
        if part.name() != "firmware" {
            continue;
        }
        return save_flash_upload(part).await;
    }

    Err(anyhow::anyhow!("firmware file is required"))
}

async fn save_flash_upload(part: Part) -> Result<FlashUpload> {
    let filename = part
        .filename()
        .map(str::to_string)
        .ok_or_else(|| anyhow::anyhow!("firmware filename is required"))?;
    let display_name = sanitize_upload_name(&filename);
    if display_name.is_empty() {
        return Err(anyhow::anyhow!("firmware filename is invalid"));
    }

    let mut data = Vec::new();
    let mut stream = part.stream();
    while let Some(chunk_result) = stream.next().await {
        let chunk = chunk_result?;
        data.extend_from_slice(chunk.chunk());
    }

    if data.is_empty() {
        return Err(anyhow::anyhow!("firmware upload is empty"));
    }

    let temp_name = format!("dashboard-flash-{}-{}", now_ms(), display_name);
    let path = std::env::temp_dir().join(temp_name);
    fs::write(&path, data)?;

    Ok(FlashUpload { path, display_name })
}

fn sanitize_upload_name(name: &str) -> String {
    let base = name.rsplit(['/', '\\']).next().unwrap_or(name);
    base.chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() || ch == '.' || ch == '-' || ch == '_' {
                ch
            } else {
                '_'
            }
        })
        .collect()
}

async fn load_map_views(config: &MapStoreConfig) -> Result<Vec<MapView>> {
    let mut views = load_stored_map_views(config).await?;
    for view in &mut views {
        hydrate_map_view_tiles(config, view).await?;
    }
    sort_map_views(&mut views);
    Ok(views)
}

async fn load_stored_map_views(config: &MapStoreConfig) -> Result<Vec<MapView>> {
    tokio::fs::create_dir_all(&config.root)
        .await
        .with_context(|| format!("creating map store '{}'", config.root.display()))?;

    let path = config.views_path();
    let raw = match tokio::fs::read_to_string(&path).await {
        Ok(raw) => raw,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(Vec::new()),
        Err(error) => {
            return Err(error).with_context(|| format!("reading map views '{}'", path.display()));
        }
    };

    if raw.trim().is_empty() {
        return Ok(Vec::new());
    }

    let mut views = serde_json::from_str::<Vec<MapView>>(&raw)
        .with_context(|| format!("parsing map views '{}'", path.display()))?;
    views.retain_mut(|view| match normalize_map_view_source(view) {
        Ok(_) => true,
        Err(error) => {
            warn!(
                "ignoring unsupported cached map view '{}': {error}",
                view.name
            );
            false
        }
    });
    Ok(views)
}

fn sort_map_views(views: &mut [MapView]) {
    views.sort_by(|a, b| {
        b.created_at_ms
            .cmp(&a.created_at_ms)
            .then_with(|| a.name.cmp(&b.name))
    });
}

async fn write_map_views(config: &MapStoreConfig, views: &[MapView]) -> Result<()> {
    tokio::fs::create_dir_all(&config.root)
        .await
        .with_context(|| format!("creating map store '{}'", config.root.display()))?;

    let path = config.views_path();
    let tmp_path = path.with_extension("json.tmp");
    let data = serde_json::to_string_pretty(views).context("serializing map views")?;
    tokio::fs::write(&tmp_path, data)
        .await
        .with_context(|| format!("writing temporary map views '{}'", tmp_path.display()))?;
    tokio::fs::rename(&tmp_path, &path)
        .await
        .with_context(|| format!("committing map views '{}'", path.display()))?;
    Ok(())
}

async fn debug_map_cache(config: &MapStoreConfig) -> Result<MapDebugResponse> {
    let mut views = load_stored_map_views(config).await?;
    sort_map_views(&mut views);
    let mut debug_views = Vec::with_capacity(views.len());
    for view in views {
        let source = map_tile_source_for_view(&view)?;
        let declared_tiles = map_view_tiles(&view)?;
        let mut zooms = declared_tiles.iter().map(|tile| tile.z).collect::<Vec<_>>();
        zooms.sort_unstable();
        zooms.dedup();

        let mut sample_tiles = Vec::new();
        for tile in declared_tiles.iter().take(12) {
            let (path, data) = read_cached_map_tile_with_path(config, source, *tile)
                .await?
                .unwrap_or_else(|| {
                    (
                        config.tile_path_for_source_id(source.id, tile.z, tile.x, tile.y),
                        Vec::new(),
                    )
                });
            let data = if data.is_empty() { None } else { Some(data) };
            let exists = data.is_some();
            let size_bytes = data.as_ref().map(|data| data.len() as u64);
            let png = data
                .as_ref()
                .map(|data| data.starts_with(PNG_SIGNATURE))
                .unwrap_or(false);
            let image_type = data
                .as_deref()
                .and_then(|data| map_tile_image_type(data).ok())
                .map(str::to_string);
            let validation = data.as_deref().map(validate_map_tile_bytes).transpose();
            let valid = matches!(validation, Ok(Some(())));
            let detail = validation.err().map(|error| error.to_string());
            let (center_lat, center_lon) = tile_center_lat_lon(*tile);
            sample_tiles.push(MapDebugTile {
                z: tile.z,
                x: tile.x,
                y: tile.y,
                center_lat,
                center_lon,
                path: path.display().to_string(),
                exists,
                png,
                image_type,
                valid,
                size_bytes,
                detail,
            });
        }

        debug_views.push(MapDebugView {
            id: view.id,
            name: view.name,
            center_lat: view.center_lat,
            center_lon: view.center_lon,
            tile_count: view.tile_count,
            zooms,
            sample_tiles,
        });
    }

    Ok(MapDebugResponse {
        ok: true,
        store_dir: config.root.display().to_string(),
        views: debug_views,
    })
}

async fn plan_map_view(
    config: &MapStoreConfig,
    request: MapDownloadRequest,
) -> Result<MapDownloadPlanResponse> {
    let source = selected_map_tile_source(request.source.as_deref())?;
    let (view, tiles) = build_map_view(&request, source)?;

    let mut existing_tiles = 0usize;
    let mut planned_tiles = Vec::with_capacity(tiles.len());
    for tile in tiles {
        let cached = cached_map_tile_exists(config, source, tile).await;
        if cached {
            existing_tiles += 1;
        }
        planned_tiles.push(MapTileDownloadPlan {
            source: source.id.to_string(),
            z: tile.z,
            x: tile.x,
            y: tile.y,
            url: map_tile_url(source.template, tile),
            cached,
        });
    }
    let mut planned_view = view;
    planned_view.tiles = planned_tiles
        .iter()
        .map(|tile| TileCoord {
            z: tile.z,
            x: tile.x,
            y: tile.y,
        })
        .collect();

    let missing_tiles = planned_tiles.len().saturating_sub(existing_tiles);
    Ok(MapDownloadPlanResponse {
        ok: true,
        view: planned_view,
        tiles: planned_tiles,
        existing_tiles,
        missing_tiles,
    })
}

async fn commit_map_view(config: &MapStoreConfig, view: MapView) -> Result<MapCommitResponse> {
    let source = map_tile_source_for_view(&view)?;

    let request = MapDownloadRequest {
        name: Some(view.name.clone()),
        source: Some(source.id.to_string()),
        lat: view.center_lat,
        lon: view.center_lon,
        radius_km: Some(view.radius_km),
        min_zoom: Some(view.min_zoom),
        max_zoom: Some(view.max_zoom),
    };
    let (expected_view, tiles) = build_map_view(&request, source)?;

    let mut existing_tiles = 0usize;
    let mut cached_tiles = Vec::new();
    for tile in &tiles {
        if cached_map_tile_exists(config, source, *tile).await {
            existing_tiles += 1;
            cached_tiles.push(*tile);
        }
    }
    if existing_tiles == 0 {
        return Err(anyhow::anyhow!(
            "no map tiles are cached for this view; download at least one tile before saving"
        ));
    }

    let mut committed = view;
    committed.name = expected_view.name;
    committed.center_lat = expected_view.center_lat;
    committed.center_lon = expected_view.center_lon;
    committed.radius_km = expected_view.radius_km;
    committed.min_zoom = expected_view.min_zoom;
    committed.max_zoom = expected_view.max_zoom;
    committed.tile_source_id = expected_view.tile_source_id;
    committed.tile_source_name = expected_view.tile_source_name;
    committed.tile_source = expected_view.tile_source;
    committed.bounds = expected_view.bounds;
    committed.levels = tile_levels_from_tiles(&cached_tiles);
    committed.tiles = cached_tiles;
    committed.tile_count = existing_tiles;
    if committed.id.trim().is_empty() {
        committed.id = format!(
            "{}-{}",
            slugify_map_view_name(&committed.name),
            committed.created_at_ms
        );
    }

    let mut views = load_map_views(config).await?;
    views.retain(|existing| existing.id != committed.id);
    views.push(committed.clone());
    sort_map_views(&mut views);
    write_map_views(config, &views).await?;

    let missing_tiles = tiles.len().saturating_sub(existing_tiles);
    Ok(MapCommitResponse {
        ok: true,
        view: committed,
        existing_tiles,
        missing_tiles,
    })
}

async fn delete_map_view(config: &MapStoreConfig, view_id: &str) -> Result<MapDeleteResponse> {
    let trimmed_id = view_id.trim();
    if trimmed_id.is_empty() {
        return Err(anyhow::anyhow!("map view id cannot be empty"));
    }

    let mut views = load_stored_map_views(config).await?;
    let index = views
        .iter()
        .position(|view| view.id == trimmed_id)
        .ok_or_else(|| anyhow::anyhow!("map view '{trimmed_id}' not found"))?;
    let deleted_view = views.remove(index);
    let deleted_source = map_tile_source_for_view(&deleted_view)?;

    let mut remaining_tiles = BTreeSet::<(String, TileCoord)>::new();
    for view in &views {
        let source = map_tile_source_for_view(view)?;
        remaining_tiles.extend(
            map_view_tiles(view)?
                .into_iter()
                .map(|tile| (source.id.to_string(), tile)),
        );
    }

    sort_map_views(&mut views);
    write_map_views(config, &views).await?;

    let deleted_tiles = map_view_tiles(&deleted_view)?;
    let mut removed_tiles = 0usize;
    for tile in deleted_tiles {
        if remaining_tiles.contains(&(deleted_source.id.to_string(), tile)) {
            continue;
        }
        if remove_cached_map_tile(config, deleted_source, tile).await? {
            removed_tiles += 1;
        }
    }

    Ok(MapDeleteResponse {
        ok: true,
        deleted_view,
        removed_tiles,
        remaining_views: views.len(),
    })
}

async fn download_map_view(
    config: &MapStoreConfig,
    request: MapDownloadRequest,
) -> Result<MapDownloadResponse> {
    let source = selected_map_tile_source(request.source.as_deref())?;
    let (mut view, tiles) = build_map_view(&request, source)?;

    tokio::fs::create_dir_all(config.tiles_dir())
        .await
        .with_context(|| format!("creating map tile store '{}'", config.tiles_dir().display()))?;

    let client = build_map_tile_client()?;

    let mut downloaded_tiles = 0usize;
    let mut existing_tiles = 0usize;
    let mut failed_tiles = 0usize;
    let mut first_error = None::<String>;
    let mut cached_tiles = Vec::new();
    for tile in tiles {
        if cached_map_tile_exists(config, source, tile).await {
            existing_tiles += 1;
            cached_tiles.push(tile);
            continue;
        }

        let path = config.tile_path_for_source_id(source.id, tile.z, tile.x, tile.y);
        if let Err(error) = download_map_tile(&client, source.template, tile, &path).await {
            failed_tiles += 1;
            let error_text = format_anyhow_chain(&error);
            if first_error.is_none() {
                first_error = Some(error_text.clone());
            }
            warn!(
                "failed to download map tile z={} x={} y={}: {}",
                tile.z, tile.x, tile.y, error_text
            );
        } else {
            downloaded_tiles += 1;
            cached_tiles.push(tile);
        }
    }

    if downloaded_tiles + existing_tiles == 0 && failed_tiles > 0 {
        let detail = first_error
            .as_deref()
            .unwrap_or("provider returned no usable tile data");
        return Err(anyhow::anyhow!(
            "no map tiles could be downloaded from the configured tile source; first error: {detail}"
        ));
    }

    view.tile_count = cached_tiles.len();
    view.levels = tile_levels_from_tiles(&cached_tiles);
    view.tiles = cached_tiles;
    let mut views = load_map_views(config).await?;
    views.retain(|existing| existing.id != view.id);
    views.push(view.clone());
    sort_map_views(&mut views);
    write_map_views(config, &views).await?;

    Ok(MapDownloadResponse {
        ok: true,
        view,
        downloaded_tiles,
        existing_tiles,
        failed_tiles,
        first_error,
    })
}

async fn download_map_tile(
    client: &reqwest::Client,
    tile_template: &str,
    tile: TileCoord,
    path: &Path,
) -> Result<usize> {
    let Some(parent) = path.parent() else {
        return Err(anyhow::anyhow!("tile path has no parent"));
    };
    tokio::fs::create_dir_all(parent)
        .await
        .with_context(|| format!("creating tile directory '{}'", parent.display()))?;

    let url = map_tile_url(tile_template, tile);
    let response = client
        .get(&url)
        .send()
        .await
        .with_context(|| format!("requesting map tile '{url}'"))?;
    let status = response.status();
    if !status.is_success() {
        return Err(anyhow::anyhow!("map tile request returned HTTP {status}"));
    }
    if let Some(blocked) = response.headers().get(OSM_BLOCKED_HEADER) {
        let detail = blocked.to_str().unwrap_or("present");
        return Err(anyhow::anyhow!(
            "map tile provider blocked the request ({OSM_BLOCKED_HEADER}: {detail})"
        ));
    }
    let bytes = response
        .bytes()
        .await
        .with_context(|| format!("reading map tile '{url}'"))?;

    write_map_tile_bytes_to_path(path, bytes.as_ref()).await?;
    Ok(bytes.len())
}

fn build_map_tile_client() -> Result<reqwest::Client> {
    reqwest::Client::builder()
        .user_agent(MAP_TILE_USER_AGENT)
        .use_rustls_tls()
        .timeout(Duration::from_secs(20))
        .build()
        .context("building map tile HTTP client")
}

async fn write_map_tile_bytes(
    config: &MapStoreConfig,
    source: &MapTileSource,
    tile: TileCoord,
    data: &[u8],
) -> Result<()> {
    write_map_tile_bytes_to_path(
        &config.tile_path_for_source_id(source.id, tile.z, tile.x, tile.y),
        data,
    )
    .await
}

async fn write_map_tile_bytes_to_path(path: &Path, data: &[u8]) -> Result<()> {
    validate_map_tile_bytes(data)?;
    let Some(parent) = path.parent() else {
        return Err(anyhow::anyhow!("tile path has no parent"));
    };
    tokio::fs::create_dir_all(parent)
        .await
        .with_context(|| format!("creating tile directory '{}'", parent.display()))?;

    let tmp_path = path.with_extension("tmp");
    tokio::fs::write(&tmp_path, data)
        .await
        .with_context(|| format!("writing temporary tile '{}'", tmp_path.display()))?;
    if let Err(error) = tokio::fs::rename(&tmp_path, path).await {
        let _ = tokio::fs::remove_file(&tmp_path).await;
        return Err(error).with_context(|| format!("committing tile '{}'", path.display()));
    }
    Ok(())
}

async fn hydrate_map_view_tiles(config: &MapStoreConfig, view: &mut MapView) -> Result<()> {
    let source = normalize_map_view_source(view)?;
    let declared_tiles = map_view_tiles(view)?;
    let mut cached_tiles = Vec::new();
    for tile in declared_tiles {
        if cached_map_tile_exists(config, source, tile).await {
            cached_tiles.push(tile);
        }
    }

    view.tile_count = cached_tiles.len();
    view.levels = tile_levels_from_tiles(&cached_tiles);
    view.tiles = cached_tiles;
    Ok(())
}

async fn cached_map_tile_exists(
    config: &MapStoreConfig,
    source: &MapTileSource,
    tile: TileCoord,
) -> bool {
    matches!(
        read_cached_map_tile(config, source, tile).await,
        Ok(Some(_data))
    )
}

async fn read_cached_map_tile(
    config: &MapStoreConfig,
    source: &MapTileSource,
    tile: TileCoord,
) -> Result<Option<Vec<u8>>> {
    Ok(read_cached_map_tile_with_path(config, source, tile)
        .await?
        .map(|(_path, data)| data))
}

async fn read_cached_map_tile_with_path(
    config: &MapStoreConfig,
    source: &MapTileSource,
    tile: TileCoord,
) -> Result<Option<(PathBuf, Vec<u8>)>> {
    for path in config.tile_paths_for_source_id(source.id, tile.z, tile.x, tile.y) {
        let data = match tokio::fs::read(&path).await {
            Ok(data) => data,
            Err(error) if error.kind() == io::ErrorKind::NotFound => continue,
            Err(error) => {
                return Err(error).with_context(|| format!("reading tile '{}'", path.display()));
            }
        };
        if let Err(error) = validate_map_tile_bytes(&data) {
            debug!(
                "ignoring invalid cached map tile source={} z={} x={} y={} at '{}': {error}",
                source.id,
                tile.z,
                tile.x,
                tile.y,
                path.display()
            );
            continue;
        }
        return Ok(Some((path, data)));
    }
    Ok(None)
}

async fn remove_cached_map_tile(
    config: &MapStoreConfig,
    source: &MapTileSource,
    tile: TileCoord,
) -> Result<bool> {
    let mut removed = false;
    for path in config.tile_paths_for_source_id(source.id, tile.z, tile.x, tile.y) {
        match tokio::fs::remove_file(&path).await {
            Ok(()) => {
                removed = true;
                cleanup_empty_tile_path_dirs(&path).await;
            }
            Err(error) if error.kind() == io::ErrorKind::NotFound => {}
            Err(error) => {
                return Err(error).with_context(|| {
                    format!(
                        "removing cached map tile source={} z={} x={} y={}",
                        source.id, tile.z, tile.x, tile.y
                    )
                });
            }
        }
    }
    Ok(removed)
}

async fn cleanup_empty_tile_path_dirs(path: &Path) {
    let mut current = path.parent();
    for _ in 0..3 {
        let Some(dir) = current else {
            return;
        };
        current = dir.parent();
        match tokio::fs::remove_dir(dir).await {
            Ok(()) => {}
            Err(error)
                if error.kind() == io::ErrorKind::NotFound
                    || error.kind() == io::ErrorKind::DirectoryNotEmpty => {}
            Err(error) => debug!(
                "failed to remove empty map tile dir '{}': {error}",
                dir.display()
            ),
        }
    }
}

fn validate_map_tile_bytes(data: &[u8]) -> Result<()> {
    if data.is_empty() {
        return Err(anyhow::anyhow!("tile data cannot be empty"));
    }
    map_tile_image_type(data)?;
    Ok(())
}

fn map_tile_image_type(data: &[u8]) -> Result<&'static str> {
    if data.starts_with(PNG_SIGNATURE) {
        return Ok("PNG");
    }
    if data.starts_with(JPEG_SIGNATURE) {
        return Ok("JPEG");
    }
    Err(anyhow::anyhow!("tile data is not a PNG or JPEG image"))
}

fn map_tile_content_type(data: &[u8]) -> Option<&'static str> {
    if data.starts_with(PNG_SIGNATURE) {
        return Some("image/png");
    }
    if data.starts_with(JPEG_SIGNATURE) {
        return Some("image/jpeg");
    }
    None
}

fn map_view_tiles(view: &MapView) -> Result<BTreeSet<TileCoord>> {
    if !view.tiles.is_empty() {
        let mut tiles = BTreeSet::new();
        for tile in &view.tiles {
            if !valid_tile_coord(tile.z, tile.x, tile.y) {
                return Err(anyhow::anyhow!(
                    "map view '{}' references invalid cached tile z={} x={} y={}",
                    view.name,
                    tile.z,
                    tile.x,
                    tile.y
                ));
            }
            tiles.insert(*tile);
        }
        return Ok(tiles);
    }

    let mut tiles = BTreeSet::new();
    for level in &view.levels {
        if level.zoom > MAX_MAP_ZOOM {
            return Err(anyhow::anyhow!(
                "map view '{}' references unsupported zoom {}",
                view.name,
                level.zoom
            ));
        }

        let axis = tile_axis_count(level.zoom);
        if level.min_x >= axis
            || level.max_x >= axis
            || level.min_y >= axis
            || level.max_y >= axis
            || level.min_x > level.max_x
            || level.min_y > level.max_y
        {
            return Err(anyhow::anyhow!(
                "map view '{}' has invalid cached tile bounds at zoom {}",
                view.name,
                level.zoom
            ));
        }

        for x in level.min_x..=level.max_x {
            for y in level.min_y..=level.max_y {
                tiles.insert(TileCoord {
                    z: level.zoom,
                    x,
                    y,
                });
            }
        }
    }
    Ok(tiles)
}

fn tile_levels_from_tiles(tiles: &[TileCoord]) -> Vec<MapTileLevel> {
    let mut levels_by_zoom = BTreeMap::<u8, MapTileLevel>::new();
    for tile in tiles {
        levels_by_zoom
            .entry(tile.z)
            .and_modify(|level| {
                level.min_x = level.min_x.min(tile.x);
                level.max_x = level.max_x.max(tile.x);
                level.min_y = level.min_y.min(tile.y);
                level.max_y = level.max_y.max(tile.y);
            })
            .or_insert(MapTileLevel {
                zoom: tile.z,
                min_x: tile.x,
                max_x: tile.x,
                min_y: tile.y,
                max_y: tile.y,
            });
    }
    levels_by_zoom.into_values().collect()
}

fn build_map_view(
    request: &MapDownloadRequest,
    source: &MapTileSource,
) -> Result<(MapView, Vec<TileCoord>)> {
    validate_map_tile_template(source.template)?;

    let lat = request.lat;
    let lon = request.lon;
    if !lat.is_finite() || !lon.is_finite() {
        return Err(anyhow::anyhow!(
            "map center latitude and longitude must be finite"
        ));
    }
    if !(-WEB_MERCATOR_MAX_LAT..=WEB_MERCATOR_MAX_LAT).contains(&lat) {
        return Err(anyhow::anyhow!(
            "map center latitude must be between -{WEB_MERCATOR_MAX_LAT} and {WEB_MERCATOR_MAX_LAT}"
        ));
    }
    if !(-180.0..=180.0).contains(&lon) {
        return Err(anyhow::anyhow!(
            "map center longitude must be between -180 and 180"
        ));
    }

    let radius_km = request.radius_km.unwrap_or(2.0);
    if !radius_km.is_finite() || radius_km <= 0.0 || radius_km > 100.0 {
        return Err(anyhow::anyhow!(
            "map radius must be greater than 0km and no more than 100km"
        ));
    }

    let min_zoom = request.min_zoom.unwrap_or(14);
    let max_zoom = request.max_zoom.unwrap_or(17);
    if min_zoom > max_zoom {
        return Err(anyhow::anyhow!("minimum zoom cannot exceed maximum zoom"));
    }
    if max_zoom > MAX_MAP_ZOOM {
        return Err(anyhow::anyhow!("maximum zoom cannot exceed {MAX_MAP_ZOOM}"));
    }

    let lat_delta = radius_km / EARTH_KM_PER_DEG_LAT;
    let lon_scale = lat.to_radians().cos().abs().max(0.1);
    let lon_delta = radius_km / (EARTH_KM_PER_DEG_LAT * lon_scale);
    let bounds = MapBounds {
        min_lat: (lat - lat_delta).clamp(-WEB_MERCATOR_MAX_LAT, WEB_MERCATOR_MAX_LAT),
        max_lat: (lat + lat_delta).clamp(-WEB_MERCATOR_MAX_LAT, WEB_MERCATOR_MAX_LAT),
        min_lon: (lon - lon_delta).clamp(-180.0, 180.0),
        max_lon: (lon + lon_delta).clamp(-180.0, 180.0),
    };

    let mut levels = Vec::new();
    let mut tiles = Vec::new();
    for zoom in min_zoom..=max_zoom {
        let west_x = lon_to_tile_x(bounds.min_lon, zoom);
        let east_x = lon_to_tile_x(bounds.max_lon, zoom);
        let north_y = lat_to_tile_y(bounds.max_lat, zoom);
        let south_y = lat_to_tile_y(bounds.min_lat, zoom);
        let min_x = west_x.min(east_x);
        let max_x = west_x.max(east_x);
        let min_y = north_y.min(south_y);
        let max_y = north_y.max(south_y);

        levels.push(MapTileLevel {
            zoom,
            min_x,
            max_x,
            min_y,
            max_y,
        });

        for x in min_x..=max_x {
            for y in min_y..=max_y {
                tiles.push(TileCoord { z: zoom, x, y });
            }
        }
    }

    let name = clean_map_view_name(request.name.as_deref(), lat, lon);
    let created_at_ms = now_ms();
    let view = MapView {
        id: format!("{}-{created_at_ms}", slugify_map_view_name(&name)),
        name,
        center_lat: lat,
        center_lon: lon,
        radius_km,
        min_zoom,
        max_zoom,
        created_at_ms,
        tile_count: tiles.len(),
        tile_source_id: source.id.to_string(),
        tile_source_name: source.name.to_string(),
        tile_source: source.template.to_string(),
        bounds,
        levels,
        tiles: tiles.clone(),
    };
    Ok((view, tiles))
}

fn map_tile_source_for_id(source_id: &str) -> Option<&'static MapTileSource> {
    MAP_TILE_SOURCES
        .iter()
        .find(|source| source.id.eq_ignore_ascii_case(source_id.trim()))
}

fn map_tile_source_for_template(tile_template: &str) -> Option<&'static MapTileSource> {
    MAP_TILE_SOURCES
        .iter()
        .find(|source| source.template == tile_template.trim())
}

fn selected_map_tile_source(source_id: Option<&str>) -> Result<&'static MapTileSource> {
    let selected = source_id
        .map(str::trim)
        .filter(|source_id| !source_id.is_empty())
        .unwrap_or(DEFAULT_MAP_TILE_SOURCE_ID);

    let Some(source) = map_tile_source_for_id(selected) else {
        return Err(anyhow::anyhow!(
            "unknown map tile source '{selected}'; expected one of: {}",
            MAP_TILE_SOURCES
                .iter()
                .map(|source| source.id)
                .collect::<Vec<_>>()
                .join(", ")
        ));
    };
    if source.id == DEFAULT_MAP_TILE_SOURCE_ID && source.template != DEFAULT_MAP_TILE_TEMPLATE {
        return Err(anyhow::anyhow!("default map tile source is misconfigured"));
    }
    validate_map_tile_template(source.template)?;
    Ok(source)
}

fn map_tile_source_for_view(view: &MapView) -> Result<&'static MapTileSource> {
    if !view.tile_source_id.trim().is_empty() {
        if let Some(source) = map_tile_source_for_id(&view.tile_source_id) {
            return Ok(source);
        }
    }

    if let Some(source) = map_tile_source_for_template(&view.tile_source) {
        return Ok(source);
    }

    Err(anyhow::anyhow!(
        "map view '{}' uses unknown map tile source",
        view.name
    ))
}

fn normalize_map_view_source(view: &mut MapView) -> Result<&'static MapTileSource> {
    let source = map_tile_source_for_view(view)?;
    view.tile_source_id = source.id.to_string();
    view.tile_source_name = source.name.to_string();
    view.tile_source = source.template.to_string();
    Ok(source)
}

fn validate_map_tile_template(tile_template: &str) -> Result<()> {
    if tile_template.len() > 1024 {
        return Err(anyhow::anyhow!("tile source URL template is too long"));
    }
    if !tile_template.starts_with("https://") && !tile_template.starts_with("http://") {
        return Err(anyhow::anyhow!(
            "tile source URL template must use http or https"
        ));
    }
    if !tile_template.contains("{z}")
        || !tile_template.contains("{x}")
        || !tile_template.contains("{y}")
    {
        return Err(anyhow::anyhow!(
            "tile source URL template must contain {{z}}, {{x}}, and {{y}}"
        ));
    }
    Ok(())
}

fn map_tile_url(tile_template: &str, tile: TileCoord) -> String {
    tile_template
        .replace("{z}", &tile.z.to_string())
        .replace("{x}", &tile.x.to_string())
        .replace("{y}", &tile.y.to_string())
}

fn clean_map_view_name(input: Option<&str>, lat: f64, lon: f64) -> String {
    let value = input
        .map(str::trim)
        .filter(|value| !value.is_empty())
        .map(|value| {
            value
                .chars()
                .filter(|ch| !ch.is_control())
                .take(80)
                .collect::<String>()
        })
        .filter(|value| !value.trim().is_empty())
        .unwrap_or_else(|| format!("Map {lat:.5}, {lon:.5}"));
    value.trim().to_string()
}

fn slugify_map_view_name(name: &str) -> String {
    let mut slug = String::new();
    let mut last_was_dash = false;
    for ch in name.chars() {
        if ch.is_ascii_alphanumeric() {
            slug.push(ch.to_ascii_lowercase());
            last_was_dash = false;
        } else if !last_was_dash && !slug.is_empty() {
            slug.push('-');
            last_was_dash = true;
        }
    }
    while slug.ends_with('-') {
        slug.pop();
    }
    if slug.is_empty() {
        "map".to_string()
    } else {
        slug
    }
}

fn valid_tile_coord(z: u8, x: u32, y: u32) -> bool {
    if z > MAX_MAP_ZOOM {
        return false;
    }
    let axis = tile_axis_count(z);
    x < axis && y < axis
}

fn lon_to_tile_x(lon: f64, z: u8) -> u32 {
    let axis = f64::from(tile_axis_count(z));
    let raw = ((lon.clamp(-180.0, 180.0) + 180.0) / 360.0) * axis;
    raw.floor().clamp(0.0, axis - 1.0) as u32
}

fn lat_to_tile_y(lat: f64, z: u8) -> u32 {
    let axis = f64::from(tile_axis_count(z));
    let lat_rad = lat
        .clamp(-WEB_MERCATOR_MAX_LAT, WEB_MERCATOR_MAX_LAT)
        .to_radians();
    let mercator = (lat_rad.tan() + (1.0 / lat_rad.cos())).ln() / std::f64::consts::PI;
    let raw = ((1.0 - mercator) / 2.0) * axis;
    raw.floor().clamp(0.0, axis - 1.0) as u32
}

fn tile_center_lat_lon(tile: TileCoord) -> (f64, f64) {
    let axis = f64::from(tile_axis_count(tile.z));
    let lon = ((f64::from(tile.x) + 0.5) / axis) * 360.0 - 180.0;
    let n = std::f64::consts::PI - 2.0 * std::f64::consts::PI * (f64::from(tile.y) + 0.5) / axis;
    let lat = n.sinh().atan().to_degrees();
    (lat, lon)
}

fn tile_axis_count(z: u8) -> u32 {
    1u32 << z
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
    let Some(worker) = state.uds_workers.get(&capability.name).cloned() else {
        let mut jobs = state.jobs.write().await;
        jobs.mark_failed(
            &job_id,
            "UDS worker unavailable for controller".to_string(),
            None,
            None,
        );
        let _ = state.publish_jobs_if_changed().await;
        return;
    };
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

    let result = worker.enter_diagnostic_session(session).await;

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
    let Some(worker) = state.uds_workers.get(&capability.name).cloned() else {
        let mut jobs = state.jobs.write().await;
        jobs.mark_failed(
            &job_id,
            "UDS worker unavailable for controller".to_string(),
            None,
            None,
        );
        let _ = state.publish_jobs_if_changed().await;
        return;
    };
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

    let result = worker.routine_start(routine_id, payload).await;

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
    let Some(worker) = state.uds_workers.get(&capability.name).cloned() else {
        let mut jobs = state.jobs.write().await;
        jobs.mark_failed(
            &job_id,
            "UDS worker unavailable for controller".to_string(),
            None,
            None,
        );
        let _ = state.publish_jobs_if_changed().await;
        return;
    };
    let reset_label = reset_key_label(&reset_type).to_string();
    info!(
        "starting reset job '{}' for controller='{}' reset='{}' on iface='{}'",
        job_id, capability.name, reset_label, capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, format!("Performing {reset_label} reset"));
    }
    let _ = state.publish_jobs_if_changed().await;

    let result = worker.reset_node(reset_type).await;

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

async fn run_flash_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
    firmware_path: PathBuf,
    firmware_name: String,
) {
    let Some(worker) = state.uds_workers.get(&capability.name).cloned() else {
        let mut jobs = state.jobs.write().await;
        jobs.mark_failed(
            &job_id,
            "UDS worker unavailable for controller".to_string(),
            None,
            None,
        );
        let _ = state.publish_jobs_if_changed().await;
        let _ = fs::remove_file(&firmware_path);
        return;
    };
    info!(
        "starting flash job '{}' for controller='{}' file='{}' on iface='{}'",
        job_id, capability.name, firmware_name, capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, format!("Flashing ECU with {firmware_name}"));
    }
    let _ = state.publish_jobs_if_changed().await;

    let result = worker
        .download_app_to_target(firmware_path.clone(), false)
        .await;

    {
        let mut jobs = state.jobs.write().await;
        match result {
            Ok(result) => match result.result {
                FlashStatus::DownloadSuccess => jobs.mark_succeeded(
                    &job_id,
                    format!(
                        "Flash completed from {firmware_name} in {:.2}s",
                        result.duration.as_secs_f32()
                    ),
                    None,
                    None,
                ),
                FlashStatus::CrcMatch => jobs.mark_succeeded(
                    &job_id,
                    format!("Flash skipped for {firmware_name}: CRC already matches"),
                    None,
                    None,
                ),
                FlashStatus::Skipped => jobs.mark_succeeded(
                    &job_id,
                    format!("Flash skipped for {firmware_name}"),
                    None,
                    None,
                ),
                FlashStatus::Failed(error) => jobs.mark_failed(
                    &job_id,
                    format!("Flash failed for {firmware_name}: {error}"),
                    None,
                    None,
                ),
            },
            Err(error) => jobs.mark_failed(
                &job_id,
                format!("Flash failed for {firmware_name}: {error}"),
                None,
                None,
            ),
        }
    }
    let _ = state.publish_jobs_if_changed().await;
    let _ = fs::remove_file(&firmware_path);
}

async fn run_manual_recovery_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
) {
    let detail = if capability.requires_manual_recovery {
        "Recovering controller from baseline after manual-recovery gate".to_string()
    } else {
        "Recovering controller from baseline".to_string()
    };
    info!(
        "starting manual recovery job '{}' for controller='{}' via ota-agent discovered from mDNS service '{}'",
        job_id, capability.name, OTA_AGENT_SERVICE_NAME
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, detail);
    }
    let _ = state.publish_jobs_if_changed().await;

    let client = reqwest::Client::builder()
        .connect_timeout(Duration::from_secs(1))
        .build()
        .unwrap_or_else(|_| reqwest::Client::new());
    let resolved_base_url = match resolve_ota_agent_base_url().await {
        Ok(url) => url,
        Err(error) => {
            let mut jobs = state.jobs.write().await;
            jobs.mark_failed(
                &job_id,
                format!("Failed to discover ota-agent over mDNS: {error}"),
                None,
                None,
            );
            drop(jobs);
            let _ = state.publish_jobs_if_changed().await;
            return;
        }
    };
    let url = format!("{}/ota/revert", resolved_base_url);
    info!(
        "sending manual recovery request for controller='{}' to {}",
        capability.name, url
    );

    match client
        .post(&url)
        .query(&[("node", capability.name.as_str())])
        .send()
        .await
    {
        Ok(response) => {
            let status = response.status();
            info!(
                "manual recovery response for controller='{}' returned HTTP {}",
                capability.name, status
            );
            if status.is_success() {
                match response.text().await {
                    Ok(body) => match serde_json::from_str::<OtaAgentRecoverReply>(&body) {
                        Ok(payload) => {
                            {
                                let mut jobs = state.jobs.write().await;
                                jobs.mark_succeeded(
                                    &job_id,
                                    format!(
                                        "Recovery completed for {} with status {}",
                                        payload.node, payload.status
                                    ),
                                    None,
                                    None,
                                );
                            }
                            let _ = state.publish_jobs_if_changed().await;
                            return;
                        }
                        Err(error) => {
                            let mut jobs = state.jobs.write().await;
                            jobs.mark_failed(
                                &job_id,
                                format!(
                                    "Recovery succeeded from {} but response parsing failed: {}",
                                    resolved_base_url, error
                                ),
                                None,
                                None,
                            );
                        }
                    },
                    Err(error) => {
                        let mut jobs = state.jobs.write().await;
                        jobs.mark_failed(
                            &job_id,
                            format!(
                                "Recovery succeeded from {} but response read failed: {}",
                                resolved_base_url, error
                            ),
                            None,
                            None,
                        );
                    }
                }
            } else {
                let error = ota_agent_error_message(status, response).await;
                let mut jobs = state.jobs.write().await;
                jobs.mark_failed(&job_id, error, None, None);
            }
        }
        Err(error) => {
            let mut jobs = state.jobs.write().await;
            jobs.mark_failed(
                &job_id,
                format!("error sending request for url ({}): {}", url, error),
                None,
                None,
            );
        }
    }

    let _ = state.publish_jobs_if_changed().await;
}

async fn resolve_ota_agent_base_url() -> Result<String> {
    let discovered = tokio::task::spawn_blocking(|| {
        let client = MdnsClient::new(
            None,
            Some(Duration::from_secs(OTA_AGENT_DISCOVERY_TIMEOUT_SECS)),
        )?;
        client.discover(DiscoveryFilter {
            service_type: Some(OTA_AGENT_SERVICE_NAME.to_string()),
            host_name: None,
        })
    })
    .await
    .context("joining mDNS discovery task")?
    .context("discovering ota-agent service")?;

    let address = discovered
        .addresses
        .iter()
        .find(|addr| addr.is_ipv4())
        .or_else(|| discovered.addresses.first())
        .copied()
        .context("ota-agent mDNS record had no addresses")?;

    info!(
        "resolved ota-agent mDNS service '{}' to {}:{} (host='{}')",
        OTA_AGENT_SERVICE_NAME, address, discovered.port, discovered.host_name
    );
    Ok(format!("http://{}:{}", address, discovered.port))
}

async fn run_tester_present_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
    stop_rx: oneshot::Receiver<String>,
) {
    let Some(worker) = state.uds_workers.get(&capability.name).cloned() else {
        let mut jobs = state.jobs.write().await;
        jobs.mark_failed(
            &job_id,
            "UDS worker unavailable for controller".to_string(),
            None,
            None,
        );
        let _ = state.publish_jobs_if_changed().await;
        let mut tester_present = state.tester_present.lock().await;
        tester_present.remove(&capability.name);
        return;
    };
    info!(
        "starting persistent tester present job '{}' for controller='{}' on iface='{}'",
        job_id, capability.name, capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, "Starting persistent tester present".to_string());
    }
    let _ = state.publish_jobs_if_changed().await;

    match worker.start_persistent_tp().await {
        Ok(()) => {
            {
                let mut jobs = state.jobs.write().await;
                jobs.mark_started(&job_id, "Persistent tester present active".to_string());
            }
            let _ = state.publish_jobs_if_changed().await;

            let stop_job_id = match stop_rx.await {
                Ok(stop_job_id) => stop_job_id,
                Err(_) => {
                    {
                        let mut jobs = state.jobs.write().await;
                        jobs.mark_failed(
                            &job_id,
                            "Persistent tester present ended unexpectedly".to_string(),
                            None,
                            None,
                        );
                    }
                    let _ = state.publish_jobs_if_changed().await;
                    let mut tester_present = state.tester_present.lock().await;
                    tester_present.remove(&capability.name);
                    return;
                }
            };
            {
                let mut jobs = state.jobs.write().await;
                jobs.mark_started(
                    &stop_job_id,
                    "Stopping persistent tester present".to_string(),
                );
            }
            let _ = state.publish_jobs_if_changed().await;

            if let Err(error) = worker.stop_persistent_tp().await {
                {
                    let mut jobs = state.jobs.write().await;
                    jobs.mark_failed(
                        &stop_job_id,
                        format!("failed to stop persistent tester present: {error}"),
                        None,
                        None,
                    );
                    jobs.mark_failed(
                        &job_id,
                        format!("failed to stop persistent tester present: {error}"),
                        None,
                        None,
                    );
                }
                let _ = state.publish_jobs_if_changed().await;
            } else {
                {
                    let mut jobs = state.jobs.write().await;
                    jobs.mark_succeeded(
                        &stop_job_id,
                        "Persistent tester present stop completed".to_string(),
                        None,
                        None,
                    );
                    jobs.mark_succeeded(
                        &job_id,
                        "Persistent tester present stopped".to_string(),
                        None,
                        None,
                    );
                }
                let _ = state.publish_jobs_if_changed().await;
            }
        }
        Err(error) => {
            {
                let mut jobs = state.jobs.write().await;
                jobs.mark_failed(
                    &job_id,
                    format!("failed to start persistent tester present: {error}"),
                    None,
                    None,
                );
            }
            let _ = state.publish_jobs_if_changed().await;
        }
    }

    let mut tester_present = state.tester_present.lock().await;
    tester_present.remove(&capability.name);
    let _ = state.publish_jobs_if_changed().await;
}

async fn run_tester_present_request_job(
    state: AppState,
    capability: ControllerCapability,
    job_id: String,
) {
    let Some(worker) = state.uds_workers.get(&capability.name).cloned() else {
        let mut jobs = state.jobs.write().await;
        jobs.mark_failed(
            &job_id,
            "UDS worker unavailable for controller".to_string(),
            None,
            None,
        );
        let _ = state.publish_jobs_if_changed().await;
        return;
    };
    info!(
        "starting tester present request job '{}' for controller='{}' on iface='{}'",
        job_id, capability.name, capability.uds_iface
    );
    {
        let mut jobs = state.jobs.write().await;
        jobs.mark_started(&job_id, "Sending tester present with response".to_string());
    }
    let _ = state.publish_jobs_if_changed().await;

    match worker.tester_present(true).await {
        Ok(response) => {
            let (detail, payload_hex) = format_tester_present_response(&response);
            let mut jobs = state.jobs.write().await;
            jobs.mark_succeeded(&job_id, detail, None, payload_hex);
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

fn format_tester_present_response(response: &[u8]) -> (String, Option<String>) {
    if response.is_empty() {
        return ("Tester present sent".to_string(), None);
    }

    if response[0] == 0x7F {
        if response.len() >= 3 {
            return (
                format!("Tester present rejected: NRC 0x{:02X}", response[2]),
                Some(hex_string(response)),
            );
        }
        return (
            "Tester present rejected: malformed negative response".to_string(),
            Some(hex_string(response)),
        );
    }

    if response[0] == 0x7E {
        return (
            "Tester present acknowledged".to_string(),
            Some(hex_string(response)),
        );
    }

    (
        format!(
            "Unexpected tester present response SID 0x{:02X}",
            response[0]
        ),
        Some(hex_string(response)),
    )
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

fn format_anyhow_chain(error: &anyhow::Error) -> String {
    let mut parts = Vec::new();
    for cause in error.chain() {
        let text = cause.to_string();
        if parts.last() != Some(&text) {
            parts.push(text);
        }
    }
    parts.join(": ")
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

fn json_map_views_response(body: MapViewsResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_map_debug_response(body: MapDebugResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_map_download_response(body: MapDownloadResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_map_plan_response(body: MapDownloadPlanResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_map_commit_response(body: MapCommitResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_map_delete_response(body: MapDeleteResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_map_tile_upload_response(body: MapTileUploadResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_jobs_response(body: JobsSnapshot) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

fn json_tester_present_state_response(
    body: TesterPresentStateResponse,
    status: warp::http::StatusCode,
) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), status).into_response()
}

fn json_current_session_response(body: CurrentSessionResponse) -> warp::reply::Response {
    warp::reply::with_status(warp::reply::json(&body), warp::http::StatusCode::OK).into_response()
}

async fn ota_agent_error_message(status: HttpStatusCode, response: reqwest::Response) -> String {
    match response.text().await {
        Ok(body) => match serde_json::from_str::<OtaAgentErrorReply>(&body) {
            Ok(payload) => payload
                .error
                .filter(|value| !value.is_empty())
                .map(|error| format!("Manual recovery failed ({status}): {error}"))
                .unwrap_or_else(|| format!("Manual recovery failed with status {status}")),
            Err(_) => format!("Manual recovery failed with status {status}"),
        },
        Err(_) => format!("Manual recovery failed with status {status}"),
    }
}

fn signal_events_reply(state: AppState, selected_ids: BTreeSet<String>) -> impl Reply {
    info!(
        "signal SSE client connected with {} requested signal(s); shared stream subscribers will become {}",
        selected_ids.len(),
        state.signal_events.receiver_count() + 1
    );
    let selected_ids = Arc::new(selected_ids);
    let stream = futures::stream::unfold(
        (state.signal_events.subscribe(), selected_ids),
        |(mut rx, selected_ids)| async move {
            loop {
                let mut events = match rx.recv().await {
                    Ok(batch) => filter_signal_sample_events(batch.as_ref(), &selected_ids),
                    Err(broadcast::error::RecvError::Lagged(skipped)) => {
                        warn!("signal SSE client lagged behind by {skipped} batch(es)");
                        continue;
                    }
                    Err(broadcast::error::RecvError::Closed) => {
                        return None;
                    }
                };

                loop {
                    match rx.try_recv() {
                        Ok(batch) => {
                            events.extend(filter_signal_sample_events(
                                batch.as_ref(),
                                &selected_ids,
                            ));
                        }
                        Err(broadcast::error::TryRecvError::Empty) => {
                            break;
                        }
                        Err(broadcast::error::TryRecvError::Lagged(skipped)) => {
                            warn!("signal SSE client lagged behind by {skipped} batch(es)");
                        }
                        Err(broadcast::error::TryRecvError::Closed) => {
                            return None;
                        }
                    }
                }

                if events.is_empty() {
                    continue;
                }

                let payload = serde_json::to_string(&SignalSampleBatch { events })
                    .unwrap_or_else(|_| "{\"events\":[]}".to_string());
                return Some((
                    Ok::<Event, Infallible>(Event::default().event("signal-sample").data(payload)),
                    (rx, selected_ids),
                ));
            }
        },
    );

    warp::sse::reply(warp::sse::keep_alive().stream(stream))
}

fn filter_signal_sample_events(
    batch: &SignalSampleBatch,
    selected_ids: &BTreeSet<String>,
) -> Vec<SignalSampleEvent> {
    if selected_ids.is_empty() {
        return batch.events.clone();
    }

    batch
        .events
        .iter()
        .filter_map(|event| {
            let samples = event
                .samples
                .iter()
                .filter(|sample| selected_ids.contains(&sample.signal_id))
                .cloned()
                .collect::<Vec<_>>();
            if samples.is_empty() {
                return None;
            }
            Some(SignalSampleEvent {
                timestamp_ms: event.timestamp_ms,
                bus: event.bus.clone(),
                message_name: event.message_name.clone(),
                message_id: event.message_id,
                samples,
            })
        })
        .collect()
}

fn spawn_veh_worker(
    iface: String,
    _tracked_controllers: Arc<BTreeSet<String>>,
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
    _tracked_controllers: Arc<BTreeSet<String>>,
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
    let seen_at_ms = now_ms();
    let members = decoded
        .members
        .into_iter()
        .map(|member| PlainMeasurement {
            name: member.name,
            value: member.value,
            unit: member.unit,
            label: member.label,
        })
        .collect::<Vec<_>>();
    normalize_update(
        &decoded.bus_name,
        decoded.message_name,
        members,
        tracked_controllers,
        seen_at_ms,
    )
}

fn run_veh_worker(opts: &Opts) -> Result<()> {
    let iface = opts.veh_iface.clone();
    let iface_map = [(iface.as_str(), yamcan::Bus::Veh)];
    let binding = yamcan::configure_iface(&iface, &iface_map)
        .map_err(|e| anyhow::anyhow!("failed to configure veh decoder for {iface}: {e}"))?;
    run_can_worker(&iface, "vehicle", &binding)
}

fn run_body_worker(opts: &Opts) -> Result<()> {
    let iface = opts.body_iface.as_deref().unwrap_or("unknown");
    Err(anyhow::anyhow!(
        "body CAN worker is no longer supported (requested iface: {iface})"
    ))
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
        let (frame, id) =
            recv_veh_frame(&socket).with_context(|| format!("read error on {iface}"))?;
        let Some(update) = decode_veh_frame(binding, frame, id, &tracked_controllers) else {
            continue;
        };
        serde_json::to_writer(&mut writer, &update).context("serializing worker update")?;
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

fn build_signal_manifest() -> SignalManifestResponse {
    let mut signals = yamcan::signal_descriptors()
        .iter()
        .map(|descriptor| SignalManifestEntry {
            id: descriptor.fqid.to_string(),
            bus: descriptor.bus.as_str().to_string(),
            message_name: descriptor.message_name.to_string(),
            message_id: descriptor.message_id,
            signal_name: descriptor.signal_name.to_string(),
            unit: descriptor.unit.map(str::to_string),
            kind: map_signal_kind(descriptor.kind),
        })
        .collect::<Vec<_>>();
    signals.sort_by(|a, b| {
        a.bus
            .cmp(&b.bus)
            .then_with(|| a.message_name.cmp(&b.message_name))
            .then_with(|| a.signal_name.cmp(&b.signal_name))
    });
    SignalManifestResponse { signals }
}

fn map_signal_kind(kind: yamcan::SignalKind) -> SignalPlotKind {
    match kind {
        yamcan::SignalKind::Numeric => SignalPlotKind::Numeric,
        yamcan::SignalKind::Boolean => SignalPlotKind::Boolean,
        yamcan::SignalKind::Enum => SignalPlotKind::Enum,
    }
}

fn sample_event_from_members(
    bus_name: &str,
    message_name: &str,
    message_id: u32,
    members: &[PlainMeasurement],
    timestamp_ms: u64,
) -> Option<SignalSampleEvent> {
    let samples = members
        .iter()
        .map(|member| SignalSample {
            signal_id: format!("{bus_name}/{message_name}/{}", member.name),
            signal_name: member.name.clone(),
            value: member.value,
            label: member.label.clone(),
            unit: member.unit.clone(),
        })
        .collect::<Vec<_>>();
    if samples.is_empty() {
        return None;
    }

    Some(SignalSampleEvent {
        timestamp_ms,
        bus: bus_name.to_string(),
        message_name: message_name.to_string(),
        message_id,
        samples,
    })
}

fn spawn_signal_broadcast_worker(
    iface: String,
    bus: yamcan::Bus,
    signal_events: broadcast::Sender<Arc<SignalSampleBatch>>,
) {
    let (tx, mut rx) = mpsc::channel::<SignalSampleEvent>(SIGNAL_EVENT_QUEUE_CAPACITY);
    let signal_events_for_decode = signal_events.clone();
    thread::spawn(move || {
        info!("shared signal stream worker starting for iface='{iface}'");
        let iface_map = [(iface.as_str(), bus)];
        let binding = match yamcan::configure_iface(&iface, &iface_map) {
            Ok(binding) => binding,
            Err(error) => {
                warn!("failed to configure signal stream decoder for {iface}: {error}");
                return;
            }
        };
        let socket = match open_raw_can_socket(&iface) {
            Ok(socket) => socket,
            Err(error) => {
                warn!("failed to open signal stream socket for {iface}: {error}");
                return;
            }
        };
        let mut dropped_frames = 0_u64;
        let mut last_drop_warning = Instant::now();

        loop {
            let (frame, id) = match recv_veh_frame(&socket) {
                Ok(frame) => frame,
                Err(error) => {
                    warn!("signal stream read error on {iface}: {error}");
                    return;
                }
            };
            if signal_events_for_decode.receiver_count() == 0 {
                continue;
            }
            let Some(event) = decode_signal_frame(&binding, frame, id) else {
                continue;
            };
            match tx.try_send(event) {
                Ok(()) => {}
                Err(mpsc::error::TrySendError::Full(_)) => {
                    dropped_frames = dropped_frames.saturating_add(1);
                    if last_drop_warning.elapsed() >= Duration::from_secs(1) {
                        warn!(
                            "signal stream decode queue is full; dropped {dropped_frames} decoded frame(s)"
                        );
                        dropped_frames = 0;
                        last_drop_warning = Instant::now();
                    }
                }
                Err(mpsc::error::TrySendError::Closed(_)) => {
                    return;
                }
            }
        }
    });

    tokio::spawn(async move {
        let mut interval =
            tokio::time::interval(Duration::from_millis(SIGNAL_SAMPLE_BATCH_INTERVAL_MS));
        interval.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
        let mut pending: Vec<SignalSampleEvent> = Vec::new();
        loop {
            tokio::select! {
                maybe_event = rx.recv() => {
                    match maybe_event {
                        Some(event) => pending.push(event),
                        None => {
                            if !pending.is_empty() && signal_events.receiver_count() > 0 {
                                let _ = signal_events.send(Arc::new(SignalSampleBatch {
                                    events: pending,
                                }));
                            }
                            break;
                        }
                    }
                }
                _ = interval.tick() => {
                    while let Ok(event) = rx.try_recv() {
                        pending.push(event);
                    }
                    if pending.is_empty() {
                        continue;
                    }
                    if signal_events.receiver_count() == 0 {
                        pending.clear();
                        continue;
                    }
                    let _ = signal_events.send(Arc::new(SignalSampleBatch {
                        events: std::mem::take(&mut pending),
                    }));
                }
            }
        }
    });
}

fn decode_signal_frame(
    binding: &yamcan::BusBinding<yamcan::Bus>,
    frame: yamcan::CanFrame,
    id: u32,
) -> Option<SignalSampleEvent> {
    let decoded = yamcan::maybe_decode(Some(binding), &frame, id, true, true, &[], &[])?;
    let members = decoded
        .members
        .into_iter()
        .map(|member| PlainMeasurement {
            name: member.name,
            value: member.value,
            unit: member.unit,
            label: member.label,
        })
        .collect::<Vec<_>>();
    let event = sample_event_from_members(
        &decoded.bus_name,
        &decoded.message_name,
        decoded.message_id,
        &members,
        now_ms(),
    )?;
    Some(event)
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
        assert!(is_supported_controller("pm100dx"));
        assert!(!is_supported_controller("stw"));
    }

    #[test]
    fn tracked_controller_names_include_non_uds_pm100dx() {
        let tracked = tracked_controller_names();
        assert!(tracked.iter().any(|controller| controller == "pm100dx"));
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
                    unit: None,
                    label: Some("FAULT".to_string()),
                },
                PlainMeasurement {
                    name: "faulted5vCritical".to_string(),
                    value: 0.0,
                    unit: None,
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
                    unit: Some("V".to_string()),
                    label: None,
                },
                PlainMeasurement {
                    name: "balancingState".to_string(),
                    value: 1.0,
                    unit: None,
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

    #[test]
    fn default_session_options_include_safety_diagnostic() {
        let sessions = default_session_options();
        assert!(sessions.iter().any(|session| {
            session.key == "safety-system" && session.label == "Safety Diagnostic"
        }));
    }

    #[test]
    fn database_redirect_host_uses_dashboard_host_without_port() {
        assert_eq!(
            database_host_from_header(Some("carputer:8091")),
            Some("carputer".to_string())
        );
        assert_eq!(
            database_host_from_header(Some("192.168.1.42:8091")),
            Some("192.168.1.42".to_string())
        );
        assert_eq!(
            database_host_from_header(Some("[::1]:8091")),
            Some("[::1]".to_string())
        );
        assert_eq!(
            database_host_from_header(Some("2001:db8::1")),
            Some("[2001:db8::1]".to_string())
        );
        assert_eq!(database_host_from_header(Some("")), None);
        assert_eq!(database_host_from_header(None), None);
    }

    #[test]
    fn database_redirect_token_is_query_encoded() {
        assert_eq!(
            percent_encode_query_value("abc-_.~=="),
            "abc-_.~%3D%3D".to_string()
        );
    }
}
