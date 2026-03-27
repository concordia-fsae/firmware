use std::{
    collections::HashMap,
    fs::File,
    io::Read,
    net::{IpAddr, Ipv4Addr, SocketAddr},
    os::unix::fs::{PermissionsExt, symlink},
    path::{Path, PathBuf},
    sync::Arc,
    thread,
    time::{Duration, Instant},
};

use anyhow::{Context, Result, anyhow, bail};
use argh::FromArgs;
use bytes::Buf; // for chunk.chunk()
use chrono::Utc;
use futures_util::{StreamExt, TryStreamExt};
use hex;
use hostname::get as get_hostname;
use if_addrs::get_if_addrs;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use reqwest::{Client, Url, multipart};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use thiserror::Error;
use tokio::{fs, io::AsyncWriteExt};
use tracing::{error, info};
use tracing_subscriber::EnvFilter;
use warp::{Filter, http::StatusCode};

#[cfg(target_os = "linux")]
use conUDS::modules::uds::UdsSession;
use conUDS::{FlashStatus, UpdateResult};
use net_detec::Client as MdnsClient;
use net_detec::Server as MdnsServer;
use net_detec::{DiscoveredService, DiscoveryFilter};

/// Host a Rest API with ability to upload and deploy applications
#[derive(FromArgs, Debug)]
struct Args {
    /// service type like _carputer-uds._tcp.local.
    #[argh(
        option,
        short = 's',
        default = "String::from(\"_ota-agent._tcp.local.\")"
    )]
    service_name: String,

    #[argh(subcommand)]
    pub subcmd: ArgSubCommands,
}

#[derive(Debug, FromArgs)]
#[argh(subcommand)]
pub enum ArgSubCommands {
    Client(SubArgClient),
    #[cfg(target_os = "linux")]
    Server(SubArgServer),
}

/// Start an OTA agent client
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "client")]
pub struct SubArgClient {
    #[argh(subcommand)]
    pub action: SubAction,
}

#[derive(Debug, FromArgs)]
#[argh(subcommand)]
pub enum SubAction {
    Stage(SubActionStage),
    Flash(SubActionFlash),
    Ota(SubActionOta),
    Bootstrap(SubActionBootstrap),
    Batch(SubActionBatch),
    Status(SubActionStatus),
    UdsPing(SubActionUdsPing),
    Revert(SubActionRevert),
}

/// Stage a binary to a node
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "stage")]
pub struct SubActionStage {
    /// the node to flash
    #[argh(option, short = 'n')]
    pub node: String,
    /// expected target platform, e.g. cfr25 or cfr26
    #[argh(option, short = 'p')]
    pub platform: Option<String>,
    /// the binary to flash
    #[argh(option, short = 'b')]
    binary: PathBuf,
}

/// Flash a staged binary onto a node
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "flash")]
pub struct SubActionFlash {
    /// the node to flash
    #[argh(option, short = 'n')]
    pub node: String,
    /// expected target platform, e.g. cfr25 or cfr26
    #[argh(option, short = 'p')]
    pub platform: Option<String>,
    /// force flashing even if there is a binary match
    #[argh(switch, short = 'f')]
    force: bool,
}

/// Stage and flash a binary to a node
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "ota")]
pub struct SubActionOta {
    /// the node to flash
    #[argh(option, short = 'n')]
    pub node: String,
    /// expected target platform, e.g. cfr25 or cfr26
    #[argh(option, short = 'p')]
    pub platform: Option<String>,
    /// the binary to flash
    #[argh(option, short = 'b')]
    binary: PathBuf,
    /// force flashing even if there is a binary match
    #[argh(switch, short = 'f')]
    force: bool,
}

/// Stage and bootstrap a full system bundle (carputer + firmware)
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "bootstrap")]
pub struct SubActionBootstrap {
    /// the node to flash (defaults to carputer)
    #[argh(option, short = 'n', default = "String::from(\"carputer\")")]
    pub node: String,
    /// expected target platform, e.g. cfr25 or cfr26
    #[argh(option, short = 'p')]
    pub platform: Option<String>,
    /// the bundle to flash
    #[argh(option, short = 'b')]
    binary: PathBuf,
    /// force flashing even if there is a binary match
    #[argh(switch, short = 'f')]
    force: bool,
}

/// OTA a set of applications to their ECUs
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "batch")]
pub struct SubActionBatch {
    /// expected target platform, e.g. cfr25 or cfr26
    #[argh(option, short = 'p')]
    pub platform: Option<String>,
    /// repeatable node-to-binary pairs in the form `-u node:/path/to/bin`
    /// example: -u mcu:build/app_mcu.bin -u imu:build/app_imu.bin
    #[argh(option, short = 'u')]
    pub targets: Vec<String>,
    /// force flashing even if there is a binary match
    #[argh(switch, short = 'f')]
    force: bool,
}

/// Report staged, production, and declared deployment state
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "status")]
pub struct SubActionStatus {
    /// optional node to report
    #[argh(option, short = 'n')]
    pub nodes: Vec<String>,
}

/// Check UDS node responsiveness and CRCs
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "uds-ping")]
pub struct SubActionUdsPing {}

/// Revert one, many, or all nodes to the baseline artifact
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "revert")]
pub struct SubActionRevert {
    /// repeatable nodes to revert
    #[argh(option, short = 'n')]
    pub nodes: Vec<String>,
    /// revert every node
    #[argh(switch)]
    pub all: bool,
}

#[cfg(target_os = "linux")]
/// Start an OTA agent server
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "server")]
pub struct SubArgServer {
    /// the CAN device to use. `can0` is used if this option is not provided
    #[argh(option, short = 't', default = "String::from(\"can0\")")]
    device: String,
    /// the deployment target manifest for local packages and UDS nodes
    #[argh(
        option,
        short = 'm',
        default = "String::from(\"/application/config/ota-agent/deploy-targets.yaml\")"
    )]
    node_manifest: String,
    /// network interface to use (e.g., eth0, en0)
    #[argh(option, short = 'i')]
    interface: Option<String>,
    /// IPv4 to advertise. If omitted, the server will detect the IPv4 of the chosen interface.
    #[argh(option)]
    ip: Option<String>,
    /// port to advertise
    #[argh(option, default = "10000")]
    port: u16,
    /// directory where received binaries will be saved (and overwritten) before flashing
    #[argh(option)]
    save_dir: PathBuf,
    /// persistent root used for local bundle extraction and activation
    #[argh(option, default = "PathBuf::from(\"/var/lib/ota-agent/local-deploy\")")]
    local_deploy_root: PathBuf,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
enum DeployTarget {
    LocalBundle {
        artifact: DeclaredArtifact,
    },
    Uds {
        request_id: u32,
        response_id: u32,
        artifact: DeclaredArtifact,
        #[serde(default)]
        stop_services: Vec<String>,
        #[serde(default)]
        start_services: Vec<String>,
    },
    LocalPackage {
        artifact: DeclaredArtifact,
        binary: LocalPackageBinary,
        #[serde(default)]
        service: Option<LocalPackageService>,
        #[serde(default)]
        resources: Vec<LocalPackageResource>,
        #[serde(default)]
        enable_services: Vec<String>,
        #[serde(default)]
        restart_services: Vec<String>,
    },
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct Config {
    #[serde(default)]
    platform: String,
    #[serde(default)]
    uds_manifest: Option<String>,
    targets: HashMap<String, DeployTarget>,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct LocalPackageBinary {
    install_path: String,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct LocalPackageService {
    unit: String,
    install_path: String,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct LocalPackageResource {
    install_path: String,
    #[serde(default = "default_true")]
    tracked: bool,
}

#[cfg(target_os = "linux")]
fn default_true() -> bool {
    true
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct DeclaredArtifact {
    filename: String,
    #[serde(default)]
    sha256: Option<String>,
    #[serde(default)]
    bundle_path: Option<String>,
}

#[cfg(target_os = "linux")]
#[derive(Clone)]
struct AppState {
    cfg: Arc<Config>,
    uds_manifest: Option<UdsManifest>,
    can_device: String,
    save_dir: PathBuf,
    manifest_path: PathBuf,
    manifest_lock: Arc<tokio::sync::Mutex<()>>,
    local_deploy_root: PathBuf,
    service_leases: Arc<tokio::sync::Mutex<ServiceLeaseState>>,
    bootstrap_report: Arc<tokio::sync::Mutex<Option<BootstrapReply>>>,
    bootstrap_report_path: PathBuf,
}

#[derive(Debug, Deserialize, Serialize)]
struct FlashParams {
    node: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    platform: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    sha: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    force: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    lease_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    release_lease: Option<bool>,
}

#[cfg(target_os = "linux")]
#[derive(Default)]
struct ServiceLeaseState {
    service_leases: HashMap<String, std::collections::HashSet<String>>,
    lease_services: HashMap<String, std::collections::HashSet<String>>,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct UdsManifest {
    nodes: HashMap<String, UdsNode>,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct UdsNode {
    request_id: u32,
    response_id: u32,
}

#[derive(Clone, Debug, Serialize, Deserialize, Default)]
struct BinariesManifest {
    /// Per-node staged and production binaries
    nodes: HashMap<String, NodeBinaries>,
}

#[derive(Clone, Debug, Serialize, Deserialize, Default)]
struct NodeBinaries {
    flashing: bool,
    staged: Option<BinaryEntry>,
    production: Option<BinaryEntry>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct BinaryEntry {
    filename: String,
    path: String,
    size: u64,
    hash: String,
    #[serde(default)]
    declared_filename: Option<String>,
    #[serde(default)]
    declared_sha256: Option<String>,
    #[serde(default)]
    filename_matches_declared: Option<bool>,
    #[serde(default)]
    hash_matches_declared: Option<bool>,
    updated_at: String,
}

#[derive(Debug, Serialize, Deserialize)]
struct FlashReply {
    node: String,
    filename: String,
    bytes: u64,
    sha256: String,
    bin: String,
    duration_ms: u128,
    status: String, // "download_success" | "crc_match" | "failed" | "skipped" | etc.
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct BootstrapNodeResult {
    node: String,
    #[serde(default)]
    kind: String,
    #[serde(default)]
    action: String,
    status: String,
    error: Option<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct BootstrapReply {
    status: String,
    #[serde(default)]
    updated: u64,
    #[serde(default)]
    skipped: u64,
    #[serde(default)]
    failed: u64,
    results: Vec<BootstrapNodeResult>,
}

#[derive(Debug, Serialize, Deserialize)]
struct VerifyReply {
    node: String,
    filename: String,
    sha256: String,
    matched: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    declared_filename: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    declared_sha256: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    filename_matches_declared: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    hash_matches_declared: Option<bool>,
}

#[derive(Debug, Deserialize)]
struct StatusParams {
    #[serde(default)]
    node: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct RevertParams {
    node: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct StatusNodeReport {
    kind: String,
    declared_filename: Option<String>,
    declared_sha256: Option<String>,
    staged: Option<BinaryEntry>,
    production: Option<BinaryEntry>,
}

#[derive(Debug, Serialize, Deserialize)]
struct StatusReport {
    platform: String,
    nodes: HashMap<String, StatusNodeReport>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct UdsPingNode {
    node: String,
    status: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    crc: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct UdsPingReport {
    nodes: Vec<UdsPingNode>,
}

#[derive(Error, Debug, Clone)]
enum ManifestError {
    #[error("binary already associated with node '{existing}', cannot reassign to '{incoming}'")]
    NodeMismatch { existing: String, incoming: String },
    #[error("filename already associated with node '{existing}', cannot reassign to '{incoming}'")]
    FilenameError { existing: String, incoming: String },
    #[error("no staged binary for node '{0}'")]
    NoStaged(String),
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env().add_directive("info".parse().unwrap()))
        .init();

    let args: Args = argh::from_env();
    match args.subcmd {
        #[cfg(target_os = "linux")]
        ArgSubCommands::Server(server) => {
            if server.interface.is_none() && server.ip.is_none() {
                bail!("Must provide either --interface or --ip");
            }

            fs::create_dir_all(&server.save_dir)
                .await
                .with_context(|| format!("creating save dir {}", server.save_dir.display()))?;

            let cfg = load_manifest(&server.node_manifest)
                .await
                .with_context(|| format!("loading manifest {}", server.node_manifest))?;
            let uds_manifest = match cfg.uds_manifest.as_deref() {
                Some(path) => Some(
                    load_uds_manifest(path)
                        .await
                        .with_context(|| format!("loading UDS manifest {}", path))?,
                ),
                None => None,
            };
            let mut target_kinds: Vec<(String, &'static str)> = cfg
                .targets
                .iter()
                .map(|(k, v)| (k.clone(), target_kind(v)))
                .collect();
            target_kinds.sort_by(|a, b| a.0.cmp(&b.0));
            info!(
                "Loaded deploy targets from {}: {}",
                server.node_manifest,
                target_kinds
                    .iter()
                    .map(|(k, v)| format!("{}={}", k, v))
                    .collect::<Vec<_>>()
                    .join(", ")
            );
            if let Some(uds_manifest) = &uds_manifest {
                let mut nodes: Vec<String> = uds_manifest.nodes.keys().cloned().collect();
                nodes.sort();
                info!(
                    "Loaded UDS manifest nodes ({}): {}",
                    nodes.len(),
                    nodes.join(", ")
                );
            } else {
                info!("No UDS manifest configured");
            }

            let state = Arc::new(AppState {
                cfg: Arc::new(cfg),
                uds_manifest,
                can_device: server.device.clone(),
                save_dir: server.save_dir.clone(),
                manifest_path: server.save_dir.join("binaries.yaml"),
                manifest_lock: Arc::new(tokio::sync::Mutex::new(())),
                local_deploy_root: server.local_deploy_root.clone(),
                service_leases: Arc::new(tokio::sync::Mutex::new(ServiceLeaseState::default())),
                bootstrap_report: Arc::new(tokio::sync::Mutex::new(None)),
                bootstrap_report_path: server.save_dir.join("bootstrap-report.json"),
            });

            match clear_flashing_flags_on_startup(&state).await {
                Ok(flashing_nodes) => {
                    if let Err(e) =
                        reconcile_flashing_nodes_on_startup(Arc::clone(&state), flashing_nodes).await
                    {
                        error!("Reconcile flashing nodes failed: {}", e);
                    }
                }
                Err(e) => error!("Failed to clear flashing flags: {}", e),
            }
            if let Err(e) = load_bootstrap_report(&state).await {
                error!("Failed to load bootstrap report: {}", e);
            }
            if let Err(e) = ensure_production_entries_on_startup(&state).await {
                error!("Failed to ensure production baselines on startup: {}", e);
            }

            let ip: IpAddr = match &server.ip {
                Some(s) => s.parse().with_context(|| format!("parsing ip {}", s))?,
                None => IpAddr::V4(find_interface_ipv4(server.interface.as_deref().unwrap())?),
            };
            let addr = SocketAddr::new(ip, server.port);

            start_mdns_advertisement(
                server.interface.clone().unwrap_or_else(default_iface_name),
                args.service_name.clone(),
                ip,
                server.port,
                state.cfg.platform.clone(),
            );

            let state_filter = warp::any().map(move || Arc::clone(&state));

            // POST /ota/verify
            let verify_route = warp::path("ota")
                .and(warp::path("verify"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and_then(verify_handler);

            // POST /ota/stage  (multipart form: file)
            let stage_route = warp::path("ota")
                .and(warp::path("stage"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and(warp::multipart::form().max_length(1024 * 1024 * 512)) // 512MB cap
                .and_then(stage_handler);

            // POST /ota/flash  (no body; uses staged manifest)
            let flash_route = warp::path("ota")
                .and(warp::path("flash"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and_then(flash_handler);

            let bootstrap_route = warp::path("ota")
                .and(warp::path("bootstrap"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and_then(bootstrap_handler);

            let bootstrap_status_route = warp::path("ota")
                .and(warp::path("bootstrap"))
                .and(warp::path("status"))
                .and(warp::get())
                .and(state_filter.clone())
                .and_then(bootstrap_status_handler);

            let status_route = warp::path("ota")
                .and(warp::path("status"))
                .and(warp::get())
                .and(warp::query::<StatusParams>())
                .and(state_filter.clone())
                .and_then(status_handler);

            let uds_ping_route = warp::path("ota")
                .and(warp::path("uds"))
                .and(warp::path("ping"))
                .and(warp::get())
                .and(state_filter.clone())
                .and_then(uds_ping_handler);

            let revert_route = warp::path("ota")
                .and(warp::path("revert"))
                .and(warp::post())
                .and(warp::query::<RevertParams>())
                .and(state_filter.clone())
                .and_then(revert_handler);

            let routes = stage_route
                .or(verify_route)
                .or(flash_route)
                .or(bootstrap_route)
                .or(bootstrap_status_route)
                .or(status_route)
                .or(uds_ping_route)
                .or(revert_route)
                .recover(recover_json);

            warp::serve(routes).run(addr).await;
        }
        ArgSubCommands::Client(client) => {
            let mdns_client = MdnsClient::new(None, Some(Duration::from_secs(2)))?;
            let mut result: Option<DiscoveredService> = None;

            let filter = DiscoveryFilter {
                service_type: Some(args.service_name.clone()),
                host_name: None,
            };
            let servers = mdns_client.discover(filter);
            match servers {
                Ok(results) => result = Some(results),
                Err(e) => println!("Error identifying server: {:?}", e),
            }

            let result = match result {
                None => {
                    error!("No agent found.");
                    return Ok(());
                }
                Some(result) => {
                    info!(
                        "Identified ota-agent at {:?}:{}",
                        result.addresses[0], result.port
                    );
                    result
                }
            };

            match client.action {
                SubAction::Stage(flash) => {
                    let mp = MultiProgress::new();
                    let buffer =
                        read_file_with_progress(&mp, &flash.binary, "reading binary")?;
                    let fname: String = Path::new(&flash.binary)
                        .file_name()
                        .map(|s| s.to_string_lossy().into_owned())
                        .expect("Invalid filename");
                    let part = multipart::Part::bytes(buffer)
                        .file_name(fname.clone())
                        .mime_str("application/octet-stream")?;
                    let form = multipart::Form::new().part("file", part);

                    // Stage
                    let stage_start = Instant::now();
                    let stage_pb = progress_step(&mp, &format!("staging {}", fname));
                    let rest_client = Client::new();
                    let url = build_url(result.addresses[0], result.port, "/ota/stage");
                    let response = rest_client
                        .post(url)
                        .query(&FlashParams {
                            node: flash.node.clone(),
                            platform: flash.platform.clone(),
                            sha: None,
                            force: None,
                            lease_id: None,
                            release_lease: None,
                        })
                        .multipart(form)
                        .send()
                        .await?;

                    let stage_body = response.text().await?;
                    stage_pb.finish_with_message(format!(
                        "staged {} ({})",
                        fname,
                        fmt_dur(stage_start.elapsed())
                    ));
                    let parsed: Result<FlashReply, _> = serde_json::from_str(&stage_body);
                    info!("Flash status: {}", parsed.unwrap().status);
                }
                SubAction::Flash(flash) => {
                    // Flash
                    let mp = MultiProgress::new();
                    let flash_start = Instant::now();
                    let flash_pb = progress_step(&mp, "flashing staged binary");
                    let url_flash = build_url(result.addresses[0], result.port, "/ota/flash");
                    let resp_flash = Client::new()
                        .post(url_flash)
                        .query(&FlashParams {
                            node: flash.node,
                            platform: flash.platform,
                            sha: None,
                            force: Some(flash.force),
                            lease_id: None,
                            release_lease: None,
                        })
                        .send()
                        .await?;
                    let flash_body = resp_flash.text().await?;
                    let parsed: Result<FlashReply, _> = serde_json::from_str(&flash_body);
                    flash_pb.finish_with_message(format!(
                        "flash done ({})",
                        fmt_dur(flash_start.elapsed())
                    ));
                    info!("Flash status: {}", parsed.unwrap().status);
                }
                SubAction::Ota(flash) => {
                    // Build multipart form with the file
                    let mp = MultiProgress::new();
                    let mut rows = Vec::new();
                    let buffer =
                        read_file_with_progress(&mp, &flash.binary, "reading binary")?;
                    let fname: String = Path::new(&flash.binary)
                        .file_name()
                        .map(|s| s.to_string_lossy().into_owned())
                        .expect("Invalid filename");
                    let part = multipart::Part::bytes(buffer.clone())
                        .file_name(fname.clone())
                        .mime_str("application/octet-stream")?;
                    let form = multipart::Form::new().part("file", part);

                    let rest_client = Client::new();

                    // Verify staged binary differs
                    let verify_start = Instant::now();
                    let verify_pb = progress_step(&mp, &format!("verifying {}", fname));
                    let mut sha = Sha256::new();
                    sha.update(&buffer);
                    let sha256_hex = hex::encode(sha.finalize());
                    let url = build_url(result.addresses[0], result.port, "/ota/verify");
                    let response = rest_client
                        .post(url)
                        .query(&FlashParams {
                            node: flash.node.clone(),
                            platform: flash.platform.clone(),
                            sha: Some(sha256_hex),
                            force: None,
                            lease_id: None,
                            release_lease: None,
                        })
                        .send()
                        .await?;
                    let parsed: Result<VerifyReply, _> =
                        serde_json::from_str(&response.text().await?);

                    let mut staged = false;
                    if let Ok(parsed) = parsed {
                        if parsed.matched {
                            staged = true;
                        }
                    }
                    let verify_status = if staged {
                        "matched"
                    } else {
                        "needs stage"
                    };
                    verify_pb.finish_with_message(format!(
                        "verify {} ({})",
                        verify_status,
                        fmt_dur(verify_start.elapsed())
                    ));
                    rows.push(vec![
                        "verify".to_string(),
                        verify_status.to_string(),
                        fmt_dur(verify_start.elapsed()),
                    ]);

                    // Stage
                    if !staged {
                        let stage_start = Instant::now();
                        let stage_pb = progress_step(&mp, &format!("staging {}", fname));
                        let url = build_url(result.addresses[0], result.port, "/ota/stage");
                        let response = rest_client
                            .post(url)
                            .query(&FlashParams {
                                node: flash.node.clone(),
                                platform: flash.platform.clone(),
                                sha: None,
                                force: None,
                                lease_id: None,
                                release_lease: None,
                            })
                            .multipart(form)
                            .send()
                            .await?;

                        let stage_body = response.text().await?;
                        stage_pb.finish_with_message(format!(
                            "staged {} ({})",
                            fname,
                            fmt_dur(stage_start.elapsed())
                        ));
                        let stage_status = if stage_body.is_empty() {
                            "ok"
                        } else {
                            "ok"
                        };
                        rows.push(vec![
                            "stage".to_string(),
                            stage_status.to_string(),
                            fmt_dur(stage_start.elapsed()),
                        ]);
                    }

                    // Flash
                    let flash_start = Instant::now();
                    let flash_pb = progress_step(&mp, "flashing staged binary");
                    let url_flash = build_url(result.addresses[0], result.port, "/ota/flash");
                    let resp_flash = Client::new()
                        .post(url_flash)
                        .query(&FlashParams {
                            node: flash.node,
                            platform: flash.platform,
                            sha: None,
                            force: Some(flash.force),
                            lease_id: None,
                            release_lease: None,
                        })
                        .send()
                        .await?;
                    let status = resp_flash.status();
                    let flash_body = resp_flash.text().await?;
                    if !status.is_success() {
                        let msg = serde_json::from_str::<serde_json::Value>(&flash_body)
                            .ok()
                            .and_then(|v| v.get("error").and_then(|e| e.as_str()).map(|s| s.to_string()))
                        .unwrap_or_else(|| flash_body.clone());
                        return Err(anyhow!("flash failed: {}", msg));
                    }
                    let parsed: FlashReply = serde_json::from_str(&flash_body)
                        .with_context(|| format!("parsing flash reply: {}", flash_body))?;
                    flash_pb.finish_with_message(format!(
                        "flash {} ({})",
                        parsed.status,
                        fmt_dur(flash_start.elapsed())
                    ));
                    rows.push(vec![
                        "flash".to_string(),
                        parsed.status.clone(),
                        fmt_dur(flash_start.elapsed()),
                    ]);
                    info!("Flash status: {}", parsed.status);
                    let table = render_table(&["action", "status", "duration"], &rows);
                    println!("{table}");
                }
                SubAction::Bootstrap(bootstrap) => {
                    let mp = MultiProgress::new();
                    let mut rows = Vec::new();
                    let buffer =
                        read_file_with_progress(&mp, &bootstrap.binary, "reading bundle")?;
                    let fname: String = Path::new(&bootstrap.binary)
                        .file_name()
                        .map(|s| s.to_string_lossy().into_owned())
                        .expect("Invalid filename");
                    let part = multipart::Part::bytes(buffer.clone())
                        .file_name(fname.clone())
                        .mime_str("application/octet-stream")?;
                    let form = multipart::Form::new().part("file", part);

                    let rest_client = Client::new();

                    let verify_start = Instant::now();
                    let verify_pb = progress_step(&mp, &format!("verifying {}", fname));
                    let mut sha = Sha256::new();
                    sha.update(&buffer);
                    let sha256_hex = hex::encode(sha.finalize());
                    let url = build_url(result.addresses[0], result.port, "/ota/verify");
                    let response = rest_client
                        .post(url)
                        .query(&FlashParams {
                            node: bootstrap.node.clone(),
                            platform: bootstrap.platform.clone(),
                            sha: Some(sha256_hex),
                            force: None,
                            lease_id: None,
                            release_lease: None,
                        })
                        .send()
                        .await?;
                    let parsed: Result<VerifyReply, _> =
                        serde_json::from_str(&response.text().await?);

                    let mut staged = false;
                    if let Ok(parsed) = parsed {
                        if parsed.matched {
                            staged = true;
                        }
                    }
                    let verify_status = if staged {
                        "matched"
                    } else {
                        "needs stage"
                    };
                    verify_pb.finish_with_message(format!(
                        "verify {} ({})",
                        verify_status,
                        fmt_dur(verify_start.elapsed())
                    ));
                    rows.push(vec![
                        "verify".to_string(),
                        verify_status.to_string(),
                        fmt_dur(verify_start.elapsed()),
                    ]);

                    if !staged {
                        let stage_start = Instant::now();
                        let stage_pb = progress_step(&mp, &format!("staging {}", fname));
                        let url = build_url(result.addresses[0], result.port, "/ota/stage");
                        let response = rest_client
                            .post(url)
                            .query(&FlashParams {
                                node: bootstrap.node.clone(),
                                platform: bootstrap.platform.clone(),
                                sha: None,
                                force: None,
                                lease_id: None,
                                release_lease: None,
                            })
                            .multipart(form)
                            .send()
                            .await?;

                        let _stage_body = response.text().await?;
                        stage_pb.finish_with_message(format!(
                            "staged {} ({})",
                            fname,
                            fmt_dur(stage_start.elapsed())
                        ));
                        rows.push(vec![
                            "stage".to_string(),
                            "ok".to_string(),
                            fmt_dur(stage_start.elapsed()),
                        ]);
                    }

                    let boot_start = Instant::now();
                    let boot_pb = progress_step(&mp, "bootstrapping bundle");
                    let url_bootstrap =
                        build_url(result.addresses[0], result.port, "/ota/bootstrap");
                    let resp = retry_request("bootstrap request", || {
                        let url_bootstrap = url_bootstrap.clone();
                        let node = bootstrap.node.clone();
                        let platform = bootstrap.platform.clone();
                        let force = bootstrap.force;
                        async move {
                            let resp = Client::new()
                                .post(url_bootstrap)
                                .query(&FlashParams {
                                    node,
                                    platform,
                                    sha: None,
                                    force: Some(force),
                                    lease_id: None,
                                    release_lease: None,
                                })
                                .send()
                                .await
                                .map_err(|e| anyhow!("bootstrap request error: {}", e))?;
                            Ok(resp)
                        }
                    })
                    .await?;
                    let status = resp.status();
                    let body = resp.text().await?;
                    let mut parsed: BootstrapReply = serde_json::from_str(&body)
                        .with_context(|| format!("parsing bootstrap reply: {}", body))?;
                    for result in &mut parsed.results {
                        if result.kind.is_empty() {
                            result.kind = "unknown".to_string();
                        }
                        if result.action.is_empty() {
                            result.action = if result.status == "failed" {
                                "failed"
                            } else {
                                "updated"
                            }
                            .to_string();
                        }
                    }
                    if parsed.status == "running" {
                        parsed.status = "scheduled".to_string();
                    }
                    if parsed.failed == 0 && parsed.status == "partial_failed" {
                        parsed.failed = parsed
                            .results
                            .iter()
                            .filter(|r| r.status == "failed")
                            .count() as u64;
                    }
                    if !status.is_success() {
                        return Err(anyhow!(
                            "bootstrap failed: {} ({} failed)",
                            parsed.status,
                            parsed.failed
                        ));
                    }
                    boot_pb.finish_with_message(format!(
                        "bootstrap {} ({})",
                        parsed.status,
                        fmt_dur(boot_start.elapsed())
                    ));
                    rows.push(vec![
                        "bootstrap".to_string(),
                        parsed.status.clone(),
                        fmt_dur(boot_start.elapsed()),
                    ]);
                    let summary_table = render_table(&["action", "status", "duration"], &rows);
                    println!("{summary_table}");

                    let mut result_rows = parsed
                        .results
                        .iter()
                        .map(|r| {
                            vec![
                                r.node.clone(),
                                r.kind.clone(),
                                r.action.clone(),
                                r.status.clone(),
                                r.error.clone().unwrap_or_else(|| "-".to_string()),
                            ]
                        })
                        .collect::<Vec<_>>();
                    result_rows.sort_by(|a, b| a[0].cmp(&b[0]));
                    let result_table = render_table(
                        &["node", "kind", "action", "status", "error"],
                        &result_rows,
                    );
                    println!("{result_table}");
                    if parsed.failed > 0 {
                        let mut reasons = HashMap::new();
                        for result in parsed.results.iter().filter(|r| r.action == "failed") {
                            let reason = result
                                .error
                                .clone()
                                .unwrap_or_else(|| "unknown".to_string());
                            *reasons.entry(reason).or_insert(0u64) += 1;
                        }
                        let mut reason_rows = reasons
                            .iter()
                            .map(|(reason, count)| vec![count.to_string(), reason.clone()])
                            .collect::<Vec<_>>();
                        reason_rows.sort_by(|a, b| b[0].cmp(&a[0]));
                        let reason_table = render_table(&["count", "reason"], &reason_rows);
                        println!("{reason_table}");
                    }

                    if parsed.status == "scheduled" {
                        return Ok(());
                    }
                }
                SubAction::Batch(batch) => {
                    if batch.targets.is_empty() {
                        error!("No targets provided. Use -u node:/path/to/bin (repeatable).");
                        std::process::exit(1);
                    }

                    let overall_start = Instant::now();
                    let mut results: Vec<(String, UpdateResult)> =
                        Vec::with_capacity(batch.targets.len());
                    let rest_client = Client::new();
                    let mp = MultiProgress::new();
                    let lease_id = format!("batch-{}", Utc::now().timestamp_millis());

                    for (idx, upd) in batch.targets.iter().enumerate() {
                        let node_start = Instant::now();
                        let Some((node, bin)) = parse_update_pair(upd) else {
                            let msg =
                                format!("Bad -u format: '{}'. Expected node:/path/to/bin", upd);
                            error!("{}", msg);
                            results.push((
                                "<unknown>".to_string(),
                                UpdateResult {
                                    bin: PathBuf::from("<unknown>".to_string()),
                                    result: FlashStatus::Failed(msg),
                                    duration: Duration::from_secs(0),
                                },
                            ));
                            continue;
                        };

                        let node_pb =
                            progress_step(&mp, &format!("{}: preparing", node));

                        // Build multipart form with the file
                        let buffer =
                            read_file_with_progress(&mp, &bin, &format!("{}: reading", node))?;
                        let fname: String = Path::new(&bin)
                            .file_name()
                            .map(|s| s.to_string_lossy().into_owned())
                            .expect("Invalid filename");
                        let part = multipart::Part::bytes(buffer.clone())
                            .file_name(fname.clone())
                            .mime_str("application/octet-stream")?;
                        let form = multipart::Form::new().part("file", part);

                        // Verify staged binary differs
                        node_pb.set_message(format!("{}: verifying", node));
                        let mut sha = Sha256::new();
                        sha.update(&buffer);
                        let sha256_hex = hex::encode(sha.finalize());
                        let url = build_url(result.addresses[0], result.port, "/ota/verify");
                        let response = rest_client
                            .post(url)
                            .query(&FlashParams {
                                node: node.clone(),
                                platform: batch.platform.clone(),
                                sha: Some(sha256_hex),
                                force: None,
                                lease_id: None,
                                release_lease: None,
                            })
                            .send()
                            .await?;
                        let parsed: Result<VerifyReply, _> =
                            serde_json::from_str(&response.text().await?);

                        let mut staged = false;
                        if let Ok(parsed) = parsed {
                            if parsed.matched {
                                staged = true;
                            }
                        }

                        if !staged {
                            // Stage
                            node_pb.set_message(format!("{}: staging", node));
                            let url =
                                build_url(result.addresses[0], result.port, "/ota/stage");
                            let response = rest_client
                                .post(url)
                                .query(&FlashParams {
                                    node: node.clone(),
                                    platform: batch.platform.clone(),
                                    sha: None,
                                    force: None,
                                    lease_id: None,
                                    release_lease: None,
                                })
                                .multipart(form)
                                .send()
                                .await?;

                            let _stage_body = response.text().await?;
                        }

                        // Flash
                        node_pb.set_message(format!("{}: flashing", node));
                        let url_flash =
                            build_url(result.addresses[0], result.port, "/ota/flash");
                        let resp_flash = Client::new()
                            .post(url_flash)
                            .query(&FlashParams {
                                node: node.clone(),
                                platform: batch.platform.clone(),
                                sha: None,
                                force: Some(batch.force),
                                lease_id: Some(lease_id.clone()),
                                release_lease: Some(idx + 1 == batch.targets.len()),
                            })
                            .send()
                            .await?;

                        // Try to parse JSON regardless of status code
                        let _status = resp_flash.status();
                        let flash_body = resp_flash.text().await?;

                        // Parse into FlashReply if possible; if not, mark as failure with the raw body
                        let parsed: Result<FlashReply, _> = serde_json::from_str(&flash_body);

                        let (final_status, bin_for_report, dur_for_report) = match parsed {
                            Ok(fr) => {
                                // Map API status to our reporting enum
                                info!("Flash status: {}", fr.status);
                                let fs = match fr.status.as_str() {
                                    "failed" => {
                                        let msg =
                                            fr.error.unwrap_or_else(|| "unknown error".to_string());
                                        FlashStatus::Failed(msg)
                                    }
                                    // Treat both "download_success" and "crc_match" as success for overall reporting
                                    "download_success" => FlashStatus::DownloadSuccess,
                                    "crc_match" => {
                                        if !staged {
                                            FlashStatus::CrcMatch
                                        } else {
                                            FlashStatus::Skipped
                                        }
                                    }
                                    other => FlashStatus::Failed(format!(
                                        "unexpected status '{}'",
                                        other
                                    )),
                                };
                                (
                                    fs,
                                    PathBuf::from(fr.bin),
                                    Duration::from_millis(fr.duration_ms as u64),
                                )
                            }
                            Err(e) => {
                                // Non-JSON or unexpected shape
                                let msg = format!(
                                    "unparseable flash reply: {} | body: {}",
                                    e, flash_body
                                );
                                (
                                    FlashStatus::Failed(msg),
                                    PathBuf::from("<unknown>"),
                                    Duration::from_secs(0),
                                )
                            }
                        };

                        let is_failed = matches!(final_status, FlashStatus::Failed(_));
                        let duration = if dur_for_report.is_zero() {
                            node_start.elapsed()
                        } else {
                            dur_for_report
                        };

                        // Record the node result using what we parsed
                        results.push((
                            node.clone(),
                            UpdateResult {
                                bin: bin_for_report,
                                result: final_status,
                                duration: duration,
                            },
                        ));
                        if is_failed {
                            node_pb.finish_with_message(format!("{}: failed", node));
                        } else {
                            node_pb.finish_with_message(format!("{}: ok", node));
                        }
                        thread::sleep(Duration::from_secs(1));
                    }

                    let total_dur = overall_start.elapsed();
                    print_deployment_report(&results, total_dur);
                }
                SubAction::Status(status) => {
                    let url = build_url(result.addresses[0], result.port, "/ota/status");
                    let query = status
                        .nodes
                        .iter()
                        .map(|node| ("node", node.clone()))
                        .collect::<Vec<_>>();
                    info!("Requesting status from ota-agent...");
                    let response = Client::new().get(url).query(&query).send().await?;
                    let body = response.text().await?;
                    let report: StatusReport = serde_json::from_str(&body)
                        .with_context(|| format!("parsing status reply: {}", body))?;
                    let mut nodes: Vec<_> = report.nodes.iter().collect();
                    nodes.sort_by(|a, b| a.0.cmp(b.0));
                    let rows = nodes
                        .into_iter()
                        .map(|(node, entry)| {
                            vec![
                                node.clone(),
                                entry.kind.clone(),
                                entry
                                    .staged
                                    .as_ref()
                                    .map(|b| b.hash.clone())
                                    .unwrap_or_else(|| "-".to_string()),
                                entry
                                    .production
                                    .as_ref()
                                    .map(|b| b.hash.clone())
                                    .unwrap_or_else(|| "-".to_string()),
                            ]
                        })
                        .collect::<Vec<_>>();
                    let table = render_table(
                        &["node", "kind", "staged_sha", "production_sha"],
                        &rows,
                    );
                    println!("{table}");
                }
                SubAction::UdsPing(_) => {
                    let mp = MultiProgress::new();
                    let ping_pb = progress_step(&mp, "requesting uds ping");
                    let url = build_url(result.addresses[0], result.port, "/ota/uds/ping");
                    let response = Client::new().get(url).send().await?;
                    let body = response.text().await?;
                    let report: UdsPingReport = serde_json::from_str(&body)
                        .with_context(|| format!("parsing uds ping reply: {}", body))?;
                    ping_pb.finish_with_message("uds ping complete");

                    let mut rows = report
                        .nodes
                        .iter()
                        .map(|node| {
                            vec![
                                node.node.clone(),
                                node.status.clone(),
                                node.crc.clone().unwrap_or_else(|| "-".to_string()),
                                node.error.clone().unwrap_or_else(|| "-".to_string()),
                            ]
                        })
                        .collect::<Vec<_>>();
                    rows.sort_by(|a, b| a[0].cmp(&b[0]));
                    let table = render_table(&["node", "status", "crc", "error"], &rows);
                    println!("{table}");
                }
                SubAction::Revert(revert) => {
                    let url = build_url(result.addresses[0], result.port, "/ota/status");
                    info!("Requesting status from ota-agent...");
                    let response = Client::new().get(url).send().await?;
                    let body = response.text().await?;
                    let report: StatusReport = serde_json::from_str(&body)
                        .with_context(|| format!("parsing status reply: {}", body))?;

                    let mut nodes = if revert.all || revert.nodes.is_empty() {
                        report
                            .nodes
                            .keys()
                            .filter(|node| node.as_str() != "carputer")
                            .cloned()
                            .collect::<Vec<_>>()
                    } else {
                        revert.nodes
                    };
                    nodes.sort();

                    let mut rows = Vec::new();
                    for node in nodes {
                        let Some(entry) = report.nodes.get(&node) else {
                            rows.push(vec![
                                node.clone(),
                                "skipped".to_string(),
                                "-".to_string(),
                                "-".to_string(),
                                "unknown node".to_string(),
                            ]);
                            continue;
                        };
                        let staged = entry.staged.as_ref().map(|b| b.hash.clone());
                        let production = entry.production.as_ref().map(|b| b.hash.clone());

                        let Some(staged_hash) = staged else {
                            rows.push(vec![
                                node.clone(),
                                "skipped".to_string(),
                                "-".to_string(),
                                production.clone().unwrap_or_else(|| "-".to_string()),
                                "no staged".to_string(),
                            ]);
                            continue;
                        };
                        let Some(prod_hash) = production else {
                            rows.push(vec![
                                node.clone(),
                                "skipped".to_string(),
                                staged_hash.clone(),
                                "-".to_string(),
                                "no production".to_string(),
                            ]);
                            continue;
                        };
                        if staged_hash == prod_hash {
                            rows.push(vec![
                                node.clone(),
                                "skipped".to_string(),
                                staged_hash.clone(),
                                prod_hash.clone(),
                                "staged matches production".to_string(),
                            ]);
                            continue;
                        }

                        info!(
                            "Reverting '{}' (staged {} != production {})",
                            node, staged_hash, prod_hash
                        );
                        let url = build_url(result.addresses[0], result.port, "/ota/revert");
                        let response = Client::new()
                            .post(url)
                            .query(&[("node", node.clone())])
                            .send()
                            .await?;
                        let status = response.status();
                        let resp_body = response.text().await?;
                        if status.is_success() {
                            rows.push(vec![
                                node.clone(),
                                "reverted".to_string(),
                                staged_hash.clone(),
                                prod_hash.clone(),
                                "staged conflicts production".to_string(),
                            ]);
                        } else {
                            rows.push(vec![
                                node.clone(),
                                "failed".to_string(),
                                staged_hash.clone(),
                                prod_hash.clone(),
                                resp_body,
                            ]);
                        }
                    }
                    let table = render_table(
                        &["node", "action", "staged_sha", "production_sha", "reason"],
                        &rows,
                    );
                    println!("{table}");
                }
            }
        }
    }
    Ok(())
}

fn build_url(ip: std::net::IpAddr, port: u16, path: &str) -> Url {
    let host = match ip {
        std::net::IpAddr::V4(v4) => v4.to_string(),
        std::net::IpAddr::V6(v6) => format!("[{}]", v6),
    };
    Url::parse(&format!("http://{}:{}{}", host, port, path)).unwrap()
}

async fn retry_request<F, Fut, T>(label: &str, mut op: F) -> anyhow::Result<T>
where
    F: FnMut() -> Fut,
    Fut: std::future::Future<Output = anyhow::Result<T>>,
{
    let mut last_err: Option<anyhow::Error> = None;
    for attempt in 1..=10 {
        match op().await {
            Ok(value) => return Ok(value),
            Err(e) => {
                last_err = Some(e);
                if attempt < 10 {
                    info!("{label}: retrying in {}s (attempt {}/{})", attempt, attempt + 1, 10);
                    tokio::time::sleep(Duration::from_secs(attempt as u64)).await;
                }
            }
        }
    }
    Err(anyhow!(
        "{}: failed after retries: {}",
        label,
        last_err.unwrap_or_else(|| anyhow!("unknown error"))
    ))
}

fn render_table(headers: &[&str], rows: &[Vec<String>]) -> String {
    let mut widths: Vec<usize> = headers.iter().map(|h| h.len()).collect();
    for row in rows {
        for (idx, value) in row.iter().enumerate() {
            if idx >= widths.len() {
                widths.push(value.len());
            } else if value.len() > widths[idx] {
                widths[idx] = value.len();
            }
        }
    }

    let mut out = String::new();
    for (idx, header) in headers.iter().enumerate() {
        let pad = widths.get(idx).copied().unwrap_or(header.len());
        out.push_str(&format!("{:width$}{}", header, if idx + 1 == headers.len() { "\n" } else { "  " }, width = pad));
    }
    for (idx, width) in widths.iter().enumerate() {
        out.push_str(&format!("{:-<width$}{}", "", if idx + 1 == widths.len() { "\n" } else { "  " }, width = *width));
    }
    for row in rows {
        for (idx, value) in row.iter().enumerate() {
            let pad = widths.get(idx).copied().unwrap_or(value.len());
            out.push_str(&format!("{:width$}{}", value, if idx + 1 == widths.len() { "\n" } else { "  " }, width = pad));
        }
    }
    out
}

fn progress_style() -> ProgressStyle {
    ProgressStyle::with_template("{spinner} {msg}")
        .unwrap()
        .tick_strings(&["|", "/", "-", "\\"])
}

fn progress_bar_style() -> ProgressStyle {
    ProgressStyle::with_template("{spinner} {msg} [{bar:20.cyan/blue}] {bytes}/{total_bytes}")
        .unwrap()
}

fn progress_step(mp: &MultiProgress, label: &str) -> ProgressBar {
    let pb = mp.add(ProgressBar::new_spinner());
    pb.set_style(progress_style());
    pb.set_message(label.to_string());
    pb.enable_steady_tick(Duration::from_millis(120));
    pb
}

fn read_file_with_progress(
    mp: &MultiProgress,
    path: &Path,
    label: &str,
) -> anyhow::Result<Vec<u8>> {
    let mut file = File::open(path)?;
    let total = file.metadata().map(|m| m.len()).unwrap_or(0);
    let pb = if total > 0 {
        let pb = mp.add(ProgressBar::new(total));
        pb.set_style(progress_bar_style());
        pb
    } else {
        mp.add(ProgressBar::new_spinner())
    };
    pb.set_message(label.to_string());
    pb.enable_steady_tick(Duration::from_millis(120));

    let mut buffer = Vec::new();
    let mut chunk = [0u8; 64 * 1024];
    loop {
        let n = file.read(&mut chunk)?;
        if n == 0 {
            break;
        }
        buffer.extend_from_slice(&chunk[..n]);
        pb.inc(n as u64);
    }
    pb.finish_with_message(format!("{label} done"));
    Ok(buffer)
}

#[cfg(target_os = "linux")]
fn validate_platform_param(
    state: &AppState,
    requested_platform: Option<&str>,
) -> std::result::Result<(), warp::Rejection> {
    let Some(requested_platform) = requested_platform else {
        return Ok(());
    };

    if state.cfg.platform.is_empty() {
        return Ok(());
    }

    if state.cfg.platform == requested_platform {
        Ok(())
    } else {
        Err(warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            format!(
                "platform mismatch: server is configured for '{}' but request was for '{}'",
                state.cfg.platform, requested_platform
            ),
        )))
    }
}

#[cfg(target_os = "linux")]
fn validate_target(state: &AppState, node: &str) -> std::result::Result<(), warp::Rejection> {
    let Some(target) = state.cfg.targets.get(node) else {
        return Err(warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            format!("unknown node '{}'", node),
        )));
    };
    if matches!(target, DeployTarget::Uds { .. }) {
        let uds_manifest = state.uds_manifest.as_ref().ok_or_else(|| {
            warp::reject::custom(HttpError(
                StatusCode::BAD_REQUEST,
                "missing UDS manifest; cannot flash UDS targets".to_string(),
            ))
        })?;
        if !uds_manifest.nodes.contains_key(node) {
            return Err(warp::reject::custom(HttpError(
                StatusCode::BAD_REQUEST,
                format!("missing UDS manifest entry for '{}'", node),
            )));
        }
    }
    Ok(())
}

#[cfg(target_os = "linux")]
fn get_target<'a>(state: &'a AppState, node: &str) -> anyhow::Result<&'a DeployTarget> {
    state
        .cfg
        .targets
        .get(node)
        .ok_or_else(|| anyhow!("unknown node '{}'", node))
}

#[cfg(target_os = "linux")]
fn uds_ids_for_node(
    state: &AppState,
    node: &str,
    request_id: u32,
    response_id: u32,
) -> std::result::Result<(u32, u32), warp::Rejection> {
    let uds_manifest = state.uds_manifest.as_ref().ok_or_else(|| {
        warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            "missing UDS manifest; cannot flash UDS targets".to_string(),
        ))
    })?;
    let entry = uds_manifest.nodes.get(node).ok_or_else(|| {
        warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            format!("missing UDS manifest entry for '{}'", node),
        ))
    })?;
    if entry.request_id != request_id || entry.response_id != response_id {
        info!(
            "UDS manifest overrides ids for {}: manifest=({}, {}) target=({}, {})",
            node, entry.request_id, entry.response_id, request_id, response_id
        );
    }
    Ok((entry.request_id, entry.response_id))
}

#[cfg(target_os = "linux")]
fn uploaded_stage_path(save_dir: &Path, node: &str, filename: &str) -> PathBuf {
    save_dir.join("staged").join(node).join(filename)
}

#[cfg(target_os = "linux")]
fn declared_artifact<'a>(target: &'a DeployTarget) -> &'a DeclaredArtifact {
    match target {
        DeployTarget::LocalBundle { artifact } => artifact,
        DeployTarget::Uds { artifact, .. } => artifact,
        DeployTarget::LocalPackage { artifact, .. } => artifact,
    }
}

#[cfg(target_os = "linux")]
fn target_kind(target: &DeployTarget) -> &'static str {
    match target {
        DeployTarget::LocalBundle { .. } => "local_bundle",
        DeployTarget::LocalPackage { .. } => "local_package",
        DeployTarget::Uds { .. } => "uds",
    }
}

#[cfg(target_os = "linux")]
fn baseline_root(state: &AppState) -> PathBuf {
    state.local_deploy_root.join("baseline")
}

#[cfg(target_os = "linux")]
fn flash_status_label(status: &FlashStatus) -> &'static str {
    match status {
        FlashStatus::DownloadSuccess => "download_success",
        FlashStatus::CrcMatch => "crc_match",
        FlashStatus::Skipped => "skipped",
        FlashStatus::Failed(_) => "failed",
    }
}

#[cfg(target_os = "linux")]
fn baseline_current_root(state: &AppState) -> PathBuf {
    baseline_root(state).join("current")
}

#[cfg(target_os = "linux")]
fn baseline_payload_root(state: &AppState) -> PathBuf {
    baseline_current_root(state).join("payload")
}

#[cfg(target_os = "linux")]
fn bundle_relative_path(path: &str) -> anyhow::Result<PathBuf> {
    if path.starts_with('/') {
        bail!("bundle path '{}' must be relative", path);
    }
    Ok(PathBuf::from(path))
}

#[cfg(target_os = "linux")]
const UDS_DID_CRC: u16 = 0x03;

#[cfg(target_os = "linux")]
// POST /ota/stage?node=...   (multipart form with part name "file")
async fn stage_handler(
    p: FlashParams,
    state: Arc<AppState>,
    form: warp::multipart::FormData,
) -> Result<impl warp::Reply, warp::Rejection> {
    validate_platform_param(&state, p.platform.as_deref())?;
    validate_target(&state, &p.node)?;
    info!("Stage request: node='{}' platform={:?}", p.node, p.platform);

    let target = get_target(&state, &p.node)
        .map_err(|e| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, e.to_string())))?;

    let mut saved_path: Option<PathBuf> = None;
    let mut total: u64 = 0;
    let mut sha = Sha256::new();
    let mut safe_name: Option<String> = None;

    // save uploaded file part named "file"
    let mut parts = form;
    while let Some(next) = parts.next().await {
        let part = next
            .map_err(|e| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, e.to_string())))?;

        if part.name() != "file" {
            continue;
        }

        let fname = part.filename().map(|s| s.to_string()).ok_or_else(|| {
            warp::reject::custom(HttpError(
                StatusCode::BAD_REQUEST,
                "missing filename".into(),
            ))
        })?;
        let safe = sanitize_filename(&fname);
        let full = uploaded_stage_path(&state.save_dir, &p.node, &safe);
        if let Some(parent) = full.parent() {
            fs::create_dir_all(parent).await.map_err(|e| {
                warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
            })?;
        }

        let mut f = fs::File::create(&full).await.map_err(|e| {
            warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
        })?;

        let mut stream = part.stream();
        while let Some(chunk) = stream.try_next().await.map_err(|e| {
            warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
        })? {
            // `chunk` implements `bytes::Buf`
            let bytes = chunk.chunk();
            sha.update(bytes);
            f.write_all(bytes).await.map_err(|e| {
                warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
            })?;
            total += bytes.len() as u64;
        }
        f.flush().await.map_err(|e| {
            warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
        })?;

        safe_name = Some(safe);
        saved_path = Some(full);
        break; // only the first `file` part is used
    }
    drop(parts);

    let bin_path = saved_path.ok_or_else(|| {
        warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            "no 'file' part found".into(),
        ))
    })?;
    let filename = safe_name.unwrap();
    let sha256_hex = hex::encode(sha.finalize());
    info!(
        "Staged '{}' for node '{}' ({} bytes, sha256 {})",
        filename, p.node, total, sha256_hex
    );

    {
        let _guard = state.manifest_lock.lock().await;
        if let Err(e) = update_manifest_stage(
            &state.manifest_path,
            &sha256_hex,
            &filename,
            &bin_path,
            total,
            &p.node,
            declared_artifact(target),
        )
        .await
        {
            if let Some(ManifestError::NodeMismatch { existing, incoming }) =
                e.downcast_ref::<ManifestError>().cloned()
            {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::CONFLICT,
                    format!(
                        "binary already associated with node '{existing}', cannot reassign to '{incoming}'"
                    ),
                )));
            }
            if let Some(ManifestError::FilenameError { existing, incoming }) =
                e.downcast_ref::<ManifestError>().cloned()
            {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::CONFLICT,
                    format!(
                        "filename already associated with node '{existing}', cannot reassign to '{incoming}'"
                    ),
                )));
            }
            return Err(warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                e.to_string(),
            )));
        }
    }

    let body = serde_json::json!({
        "status": "staged",
        "node": p.node,
        "filename": filename,
        "bytes_saved": total,
        "sha256": sha256_hex,
    });
    Ok(warp::reply::with_status(
        warp::reply::json(&body),
        StatusCode::OK,
    ))
}

#[cfg(target_os = "linux")]
// POST /ota/stage?node=...
async fn verify_handler(
    p: FlashParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    validate_platform_param(&state, p.platform.as_deref())?;
    validate_target(&state, &p.node)?;

    let manifest = match read_manifest_compat(&state.manifest_path).await {
        Ok(m) => m,
        Err(e) => {
            return Err(warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("failed to read manifest: {}", e),
            )));
        }
    };
    if let Some(node) = manifest.nodes.get(&p.node) {
        if let Some(binary) = &node.staged {
            let matched = binary.hash == p.sha.unwrap_or_default();
            let body = serde_json::json!({
                "node": p.node,
                "filename": binary.filename,
                "sha256": binary.hash,
                "matched": matched,
                "declared_filename": binary.declared_filename,
                "declared_sha256": binary.declared_sha256,
                "filename_matches_declared": binary.filename_matches_declared,
                "hash_matches_declared": binary.hash_matches_declared,
            });
            info!(
                "Verify request: node='{}' matched={} sha256={}",
                p.node, matched, binary.hash
            );
            return Ok(warp::reply::with_status(
                warp::reply::json(&body),
                StatusCode::OK,
            ));
        }
    }

    let body = serde_json::json!({
        "node": p.node,
        "filename": "",
        "sha256": "",
        "matched": false,
    });
    info!("Verify request: node='{}' matched=false", p.node);
    Ok(warp::reply::with_status(
        warp::reply::json(&body),
        StatusCode::OK,
    ))
}

#[cfg(target_os = "linux")]
async fn status_handler(
    p: StatusParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    let manifest = read_manifest_compat(&state.manifest_path)
        .await
        .map_err(|e| {
            warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("failed to read manifest: {}", e),
            ))
        })?;

    let requested = if p.node.is_empty() {
        state.cfg.targets.keys().cloned().collect::<Vec<_>>()
    } else {
        p.node
    };

    let mut nodes = HashMap::new();
    for node in requested {
        let target = get_target(&state, &node)
            .map_err(|e| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, e.to_string())))?;
        let artifact = declared_artifact(target);
        let binaries = manifest.nodes.get(&node);
        nodes.insert(
            node,
            StatusNodeReport {
                kind: target_kind(target).to_string(),
                declared_filename: Some(artifact.filename.clone()),
                declared_sha256: artifact.sha256.clone(),
                staged: binaries.and_then(|b| b.staged.clone()),
                production: binaries.and_then(|b| b.production.clone()),
            },
        );
    }

    Ok(warp::reply::with_status(
        warp::reply::json(&StatusReport {
            platform: state.cfg.platform.clone(),
            nodes,
        }),
        StatusCode::OK,
    ))
}

#[cfg(target_os = "linux")]
async fn uds_ping_handler(state: Arc<AppState>) -> Result<impl warp::Reply, warp::Rejection> {
    let uds_manifest = state.uds_manifest.as_ref().ok_or_else(|| {
        warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            "missing UDS manifest; cannot ping UDS targets".to_string(),
        ))
    })?;

    let manifest = read_manifest_compat(&state.manifest_path)
        .await
        .map_err(|e| {
            warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("failed to read manifest: {}", e),
            ))
        })?;

    let mut nodes: Vec<_> = uds_manifest
        .nodes
        .iter()
        .map(|(node, uds_entry)| {
            let flashing = manifest
                .nodes
                .get(node)
                .map(|entry| entry.flashing)
                .unwrap_or(false);
            let ids = match state.cfg.targets.get(node) {
                Some(DeployTarget::Uds {
                    request_id,
                    response_id,
                    ..
                }) => match uds_ids_for_node_anyhow(&state, node, *request_id, *response_id) {
                    Ok(ids) => ids,
                    Err(_) => (uds_entry.request_id, uds_entry.response_id),
                },
                _ => (uds_entry.request_id, uds_entry.response_id),
            };
            (node.clone(), ids.0, ids.1, flashing)
        })
        .collect();
    nodes.sort_by(|a, b| a.0.cmp(&b.0));

    let mut results = Vec::new();
    for (node, request_id, response_id, flashing) in nodes {
        if flashing {
            results.push(UdsPingNode {
                node,
                status: "flashing".to_string(),
                crc: None,
                error: None,
            });
            continue;
        }

        let mut uds = UdsSession::new(&state.can_device, request_id, response_id, false).await;
        let resp = uds.client.did_read(UDS_DID_CRC).await;
        uds.teardown().await;

        match resp {
            Ok(bytes) => {
                if let Ok(arr) = <[u8; 4]>::try_from(bytes.as_slice()) {
                    let crc = u32::from_le_bytes(arr);
                    if crc == 0xffffffff {
                        results.push(UdsPingNode {
                            node,
                            status: "bootloader".to_string(),
                            crc: Some("0xffffffff".to_string()),
                            error: None,
                        });
                    } else {
                        results.push(UdsPingNode {
                            node,
                            status: "online".to_string(),
                            crc: Some(format!("0x{:08x}", crc)),
                            error: None,
                        });
                    }
                } else {
                    results.push(UdsPingNode {
                        node,
                        status: "online".to_string(),
                        crc: None,
                        error: Some("invalid crc response".to_string()),
                    });
                }
            }
            Err(_) => results.push(UdsPingNode {
                node,
                status: "offline".to_string(),
                crc: None,
                error: Some("no response".to_string()),
            }),
        }
    }

    Ok(warp::reply::with_status(
        warp::reply::json(&UdsPingReport { nodes: results }),
        StatusCode::OK,
    ))
}

#[cfg(target_os = "linux")]
async fn flash_node(
    bus: &str,
    binary: &Path,
    node: &str,
    request_id: u32,
    response_id: u32,
    manifest_path: &Path,
    lock: &Arc<tokio::sync::Mutex<()>>,
    force: bool,
) -> UpdateResult {
    if let Err(e) = lock_manifest_node(&manifest_path, &node, &lock).await {
        return UpdateResult {
            bin: binary.into(),
            result: FlashStatus::Failed(e.to_string()),
            duration: Duration::from_secs(0),
        };
    }
    let mut uds = UdsSession::new(bus, request_id, response_id, false).await;
    let result = uds.download_app_to_target(&binary.into(), !force).await;
    unlock_manifest_node(&manifest_path, &node, &lock).await;

    result
}

#[cfg(target_os = "linux")]
async fn lock_manifest_node(
    manifest_path: &Path,
    node: &str,
    lock: &Arc<tokio::sync::Mutex<()>>,
) -> anyhow::Result<()> {
    let _guard = lock.lock().await;
    let mut manifest = read_manifest_compat(manifest_path).await?;
    if !manifest
        .nodes
        .get_mut(node)
        .expect("Invalid manifest entry")
        .flashing
    {
        manifest
            .nodes
            .get_mut(node)
            .expect("Invalid manifest entry")
            .flashing = true;
        return Ok(write_manifest(manifest_path, &manifest).await?);
    } else {
        return Err(anyhow!("Node {} is already being flashed", node));
    }
}

#[cfg(target_os = "linux")]
async fn unlock_manifest_node(
    manifest_path: &Path,
    node: &str,
    lock: &Arc<tokio::sync::Mutex<()>>,
) -> anyhow::Result<()> {
    let _guard = lock.lock().await;
    let mut manifest = read_manifest_compat(manifest_path).await?;
    if manifest
        .nodes
        .get_mut(node)
        .expect("Invalid manifest entry")
        .flashing
    {
        manifest
            .nodes
            .get_mut(node)
            .expect("Invalid manifest entry")
            .flashing = false;
        return Ok(write_manifest(manifest_path, &manifest).await?);
    } else {
        return Err(anyhow!("Node {} is not being flashed", node));
    }
}

#[cfg(target_os = "linux")]
async fn systemd_service(action: &str, unit: &str) -> anyhow::Result<()> {
    use tokio::process::Command;
    let status = Command::new("systemctl")
        .arg(action)
        .arg(unit)
        .status()
        .await?;
    if status.success() {
        Ok(())
    } else {
        Err(anyhow::anyhow!(
            "systemctl {} {} exited with {:?}",
            action,
            unit,
            status
        ))
    }
}

#[cfg(target_os = "linux")]
async fn log_systemd_status(unit: &str) {
    use tokio::process::Command;
    let active = Command::new("systemctl")
        .arg("is-active")
        .arg(unit)
        .output()
        .await;
    let enabled = Command::new("systemctl")
        .arg("is-enabled")
        .arg(unit)
        .output()
        .await;

    let active_msg = match active {
        Ok(out) => String::from_utf8_lossy(&out.stdout).trim().to_string(),
        Err(e) => format!("error: {}", e),
    };
    let enabled_msg = match enabled {
        Ok(out) => String::from_utf8_lossy(&out.stdout).trim().to_string(),
        Err(e) => format!("error: {}", e),
    };

    info!(
        "systemd status for {}: active='{}' enabled='{}'",
        unit, active_msg, enabled_msg
    );
}

#[cfg(target_os = "linux")]
async fn acquire_service_lease(
    state: &AppState,
    lease_id: Option<&str>,
    services: &[String],
) -> anyhow::Result<()> {
    let Some(lease_id) = lease_id else {
        for unit in services {
            if let Err(e) = systemd_service("stop", unit).await {
                error!("Failed to stop {}: {}", unit, e);
            }
        }
        return Ok(());
    };

    let mut to_stop = Vec::new();
    {
        let mut leases = state.service_leases.lock().await;
        for unit in services {
            let owners = leases.service_leases.entry(unit.clone()).or_default();
            if owners.insert(lease_id.to_string()) && owners.len() == 1 {
                to_stop.push(unit.clone());
            }
            leases
                .lease_services
                .entry(lease_id.to_string())
                .or_default()
                .insert(unit.clone());
        }
    }

    for unit in to_stop {
        if let Err(e) = systemd_service("stop", &unit).await {
            error!("Failed to stop {}: {}", unit, e);
        }
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn release_service_lease(
    state: &AppState,
    lease_id: Option<&str>,
    fallback_services: &[String],
) -> anyhow::Result<()> {
    let Some(lease_id) = lease_id else {
        for unit in fallback_services {
            if let Err(e) = systemd_service("start", unit).await {
                error!("Failed to start {}: {}", unit, e);
            }
        }
        return Ok(());
    };

    let mut to_start = Vec::new();
    {
        let mut leases = state.service_leases.lock().await;
        let services = leases.lease_services.remove(lease_id).unwrap_or_default();

        for unit in services {
            if let Some(owners) = leases.service_leases.get_mut(&unit) {
                owners.remove(lease_id);
                if owners.is_empty() {
                    leases.service_leases.remove(&unit);
                    to_start.push(unit);
                }
            }
        }
    }

    for unit in to_start {
        if let Err(e) = systemd_service("start", &unit).await {
            error!("Failed to start {}: {}", unit, e);
        }
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn run_command(program: &str, args: &[&str]) -> anyhow::Result<()> {
    use tokio::process::Command;

    let status = Command::new(program).args(args).status().await?;
    if status.success() {
        Ok(())
    } else {
        Err(anyhow!("{program} {:?} exited with {:?}", args, status))
    }
}

#[cfg(target_os = "linux")]
async fn install_local_support_files(release_root: &Path) -> anyhow::Result<PathBuf> {
    let bootstrap_dir = release_root.join("bootstrap");
    let service_src = bootstrap_dir.join("ota-agent-drive-stack.service");
    let script_src = bootstrap_dir.join("ota-agent-drive-stack-activate.sh");
    let bootstrap_src = bootstrap_dir.join("bootstrap-carputer.sh");
    let bootstrap_startup_src = bootstrap_dir.join("bootstrap-startup.sh");
    let bootstrap_service_src = bootstrap_dir.join("bootstrap-carputer.service");
    let service_dst = PathBuf::from("/etc/systemd/system/ota-agent-drive-stack.service");
    let script_dst = PathBuf::from("/usr/local/libexec/ota-agent/drive-stack-activate.sh");
    let bootstrap_dst = PathBuf::from("/usr/local/bin/bootstrap-carputer");
    let bootstrap_startup_dst = PathBuf::from("/usr/local/libexec/ota-agent/bootstrap-startup.sh");
    let bootstrap_service_dst = PathBuf::from("/etc/systemd/system/bootstrap-carputer.service");

    fs::create_dir_all("/usr/local/libexec/ota-agent")
        .await
        .context("creating bootstrap script directory")?;
    fs::copy(&service_src, &service_dst)
        .await
        .with_context(|| format!("installing {}", service_dst.display()))?;
    fs::copy(&script_src, &script_dst)
        .await
        .with_context(|| format!("installing {}", script_dst.display()))?;
    if fs::try_exists(&bootstrap_src).await? {
        fs::create_dir_all("/usr/local/bin")
            .await
            .context("creating /usr/local/bin")?;
        fs::copy(&bootstrap_src, &bootstrap_dst)
            .await
            .with_context(|| format!("installing {}", bootstrap_dst.display()))?;
        fs::set_permissions(&bootstrap_dst, std::fs::Permissions::from_mode(0o755))
            .await
            .with_context(|| format!("setting executable bit on {}", bootstrap_dst.display()))?;
    }
    if fs::try_exists(&bootstrap_startup_src).await? {
        fs::copy(&bootstrap_startup_src, &bootstrap_startup_dst)
            .await
            .with_context(|| format!("installing {}", bootstrap_startup_dst.display()))?;
        fs::set_permissions(&bootstrap_startup_dst, std::fs::Permissions::from_mode(0o755))
            .await
            .with_context(|| format!("setting executable bit on {}", bootstrap_startup_dst.display()))?;
    }
    if fs::try_exists(&bootstrap_service_src).await? {
        fs::copy(&bootstrap_service_src, &bootstrap_service_dst)
            .await
            .with_context(|| format!("installing {}", bootstrap_service_dst.display()))?;
    }
    fs::set_permissions(&script_dst, std::fs::Permissions::from_mode(0o755))
        .await
        .with_context(|| format!("setting executable bit on {}", script_dst.display()))?;

    Ok(script_dst)
}

#[cfg(target_os = "linux")]
async fn seed_base_bin_from_payload(release_root: &Path, state: &AppState) -> anyhow::Result<()> {
    let payload_bin = release_root.join("payload/bin/cfr");
    let base_root = state.local_deploy_root.join("base/bin-cfr");
    if !fs::try_exists(&payload_bin).await? {
        info!(
            "No payload bin/cfr found at {}, skipping base seed",
            payload_bin.display()
        );
        return Ok(());
    }
    if fs::try_exists(&base_root).await? {
        fs::remove_dir_all(&base_root)
            .await
            .with_context(|| format!("removing {}", base_root.display()))?;
    }
    fs::create_dir_all(&base_root)
        .await
        .with_context(|| format!("creating {}", base_root.display()))?;
    let src_str = payload_bin
        .to_str()
        .ok_or_else(|| anyhow!("invalid payload path {}", payload_bin.display()))?;
    let dst_str = base_root
        .to_str()
        .ok_or_else(|| anyhow!("invalid base path {}", base_root.display()))?;
    run_command(
        "bash",
        &[
            "-lc",
            &format!("cp -a \"{src}/.\" \"{dst}/\"", src = src_str, dst = dst_str),
        ],
    )
    .await
    .with_context(|| format!("seeding base bin from {}", payload_bin.display()))?;
    Ok(())
}

#[cfg(target_os = "linux")]
fn payload_path(release_root: &Path, install_path: &str) -> anyhow::Result<PathBuf> {
    let relative = install_path
        .strip_prefix('/')
        .ok_or_else(|| anyhow!("install path '{}' must be absolute", install_path))?;
    Ok(release_root.join("payload").join(relative))
}

#[cfg(target_os = "linux")]
async fn validate_local_package_contents(
    release_root: &Path,
    node: &str,
    binary: &LocalPackageBinary,
    service: Option<&LocalPackageService>,
    resources: &[LocalPackageResource],
) -> anyhow::Result<()> {
    let binary_path = payload_path(release_root, &binary.install_path)?;
    if !fs::try_exists(&binary_path).await? {
        bail!(
            "package for '{}' is missing binary at {}",
            node,
            binary_path.display()
        );
    }

    if let Some(service) = service {
        let service_path = payload_path(release_root, &service.install_path)?;
        if !fs::try_exists(&service_path).await? {
            bail!(
                "package for '{}' is missing service unit '{}' at {}",
                node,
                service.unit,
                service_path.display()
            );
        }
    }

    for resource in resources {
        let resource_path = payload_path(release_root, &resource.install_path)?;
        if !fs::try_exists(&resource_path).await? {
            bail!(
                "package for '{}' is missing resource at {}",
                node,
                resource_path.display()
            );
        }
    }

    Ok(())
}

#[cfg(target_os = "linux")]
async fn sha256_file_hex(path: &Path) -> anyhow::Result<String> {
    let bytes = fs::read(path)
        .await
        .with_context(|| format!("reading {}", path.display()))?;
    let mut sha = Sha256::new();
    sha.update(&bytes);
    Ok(hex::encode(sha.finalize()))
}

#[cfg(target_os = "linux")]
async fn local_payload_sha256(
    release_root: &Path,
    binary: &LocalPackageBinary,
    service: Option<&LocalPackageService>,
    resources: &[LocalPackageResource],
) -> anyhow::Result<String> {
    let mut sha = Sha256::new();

    let binary_path = payload_path(release_root, &binary.install_path)?;
    sha.update(binary.install_path.as_bytes());
    sha.update(sha256_file_hex(&binary_path).await?.as_bytes());

    if let Some(service) = service {
        let service_path = payload_path(release_root, &service.install_path)?;
        sha.update(service.install_path.as_bytes());
        sha.update(service.unit.as_bytes());
        sha.update(sha256_file_hex(&service_path).await?.as_bytes());
    }

    for resource in resources {
        if !resource.tracked {
            continue;
        }
        let resource_path = payload_path(release_root, &resource.install_path)?;
        sha.update(resource.install_path.as_bytes());
        sha.update(sha256_file_hex(&resource_path).await?.as_bytes());
    }

    Ok(hex::encode(sha.finalize()))
}

#[cfg(target_os = "linux")]
async fn copy_tree_contents(src: &Path, dst: &Path) -> anyhow::Result<()> {
    fs::create_dir_all(dst)
        .await
        .with_context(|| format!("creating {}", dst.display()))?;
    let src_str = src
        .to_str()
        .ok_or_else(|| anyhow!("invalid source path {}", src.display()))?;
    let dst_str = dst
        .to_str()
        .ok_or_else(|| anyhow!("invalid destination path {}", dst.display()))?;
    run_command(
        "bash",
        &[
            "-lc",
            &format!("cp -a \"{src}/.\" \"{dst}/\"", src = src_str, dst = dst_str),
        ],
    )
    .await
}

#[cfg(target_os = "linux")]
async fn remove_payload_path(active_payload: &Path, install_path: &str) -> anyhow::Result<()> {
    let relative = install_path
        .strip_prefix('/')
        .ok_or_else(|| anyhow!("install path '{}' must be absolute", install_path))?;
    let dst = active_payload.join(relative);
    if !fs::try_exists(&dst).await? {
        return Ok(());
    }
    let metadata = fs::symlink_metadata(&dst).await?;
    if metadata.file_type().is_dir() && !metadata.file_type().is_symlink() {
        fs::remove_dir_all(&dst).await?;
    } else {
        fs::remove_file(&dst).await?;
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn remove_dir_if_exists(path: &Path) -> anyhow::Result<()> {
    if fs::try_exists(path).await? {
        fs::remove_dir_all(path)
            .await
            .with_context(|| format!("removing {}", path.display()))?;
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn clear_bootstrap_state(state: &AppState) -> anyhow::Result<()> {
    remove_dir_if_exists(&state.local_deploy_root.join("packages")).await?;
    remove_dir_if_exists(&state.local_deploy_root.join("staged-local")).await?;
    Ok(())
}

#[cfg(target_os = "linux")]
async fn clear_staged_uploads(state: &AppState) -> anyhow::Result<()> {
    remove_dir_if_exists(&state.save_dir.join("staged")).await?;
    Ok(())
}

#[cfg(target_os = "linux")]
async fn collect_and_clear_flashing_nodes(
    manifest_path: &Path,
) -> anyhow::Result<Vec<String>> {
    let mut manifest = read_manifest_compat(manifest_path).await?;
    let mut flashing_nodes = Vec::new();
    for (node, entry) in manifest.nodes.iter_mut() {
        if entry.flashing {
            flashing_nodes.push(node.clone());
            entry.flashing = false;
        }
    }
    if !flashing_nodes.is_empty() {
        write_manifest(manifest_path, &manifest).await?;
    }
    Ok(flashing_nodes)
}

#[cfg(target_os = "linux")]
async fn clear_flashing_flags_on_startup(state: &AppState) -> anyhow::Result<Vec<String>> {
    let _guard = state.manifest_lock.lock().await;
    let flashing_nodes = collect_and_clear_flashing_nodes(&state.manifest_path).await?;
    if !flashing_nodes.is_empty() {
        info!(
            "Cleared stale flashing flags on startup for {} nodes",
            flashing_nodes.len()
        );
    }
    Ok(flashing_nodes)
}

#[cfg(target_os = "linux")]
fn uds_ids_for_node_anyhow(
    state: &AppState,
    node: &str,
    request_id: u32,
    response_id: u32,
) -> anyhow::Result<(u32, u32)> {
    let uds_manifest = state
        .uds_manifest
        .as_ref()
        .ok_or_else(|| anyhow!("missing UDS manifest; cannot flash UDS targets"))?;
    let entry = uds_manifest
        .nodes
        .get(node)
        .ok_or_else(|| anyhow!("missing UDS manifest entry for '{}'", node))?;
    if entry.request_id != request_id || entry.response_id != response_id {
        info!(
            "UDS manifest overrides ids for {}: manifest=({}, {}) target=({}, {})",
            node, entry.request_id, entry.response_id, request_id, response_id
        );
    }
    Ok((entry.request_id, entry.response_id))
}

#[cfg(target_os = "linux")]
async fn reconcile_flashing_nodes_on_startup(
    state: Arc<AppState>,
    flashing_nodes: Vec<String>,
) -> anyhow::Result<()> {
    if flashing_nodes.is_empty() {
        return Ok(());
    }
    let carputer_flashing = flashing_nodes.iter().any(|n| n == "carputer");
    let mut uds_nodes = Vec::new();
    if carputer_flashing {
        for (node, target) in state.cfg.targets.iter() {
            if matches!(target, DeployTarget::Uds { .. }) {
                uds_nodes.push(node.clone());
            }
        }
    } else {
        for node in flashing_nodes {
            if let Some(DeployTarget::Uds { .. }) = state.cfg.targets.get(&node) {
                uds_nodes.push(node.clone());
            }
        }
    }
    if uds_nodes.is_empty() {
        return Ok(());
    }
    info!(
        "Reconciling {} UDS nodes after restart (carputer_flashing={})",
        uds_nodes.len(),
        carputer_flashing
    );
    let mut results: Vec<BootstrapNodeResult> = Vec::new();
    for node in uds_nodes {
        let DeployTarget::Uds {
            request_id,
            response_id,
            artifact,
            ..
        } = state
            .cfg
            .targets
            .get(&node)
            .ok_or_else(|| anyhow!("missing target for '{}'", node))?
        else {
            continue;
        };
        let (request_id, response_id) =
            uds_ids_for_node_anyhow(&state, &node, *request_id, *response_id)?;
        let bundle_path = artifact
            .bundle_path
            .as_ref()
            .ok_or_else(|| anyhow!("firmware target '{}' missing bundle path", node))?;
        let baseline_path = baseline_bundle_entry(&state, bundle_path)?;
        if !fs::try_exists(&baseline_path).await? {
            error!(
                "Reconcile: missing baseline firmware for '{}' at {}",
                node,
                baseline_path.display()
            );
            results.push(BootstrapNodeResult {
                node: node.clone(),
                kind: "uds".to_string(),
                action: "failed".to_string(),
                status: "missing_baseline".to_string(),
                error: Some("missing baseline firmware".to_string()),
            });
            continue;
        }
        let result = flash_node(
            &state.can_device,
            &baseline_path,
            &node,
            request_id,
            response_id,
            &state.manifest_path,
            &state.manifest_lock,
            false,
        )
        .await;
        info!("Reconcile: flashed '{}' status={:?}", node, result.result);
        let error = match &result.result {
            FlashStatus::Failed(e) => Some(e.clone()),
            _ => None,
        };
        results.push(BootstrapNodeResult {
            node: node.clone(),
            kind: "uds".to_string(),
            action: if error.is_some() { "failed" } else { "updated" }.to_string(),
            status: flash_status_label(&result.result).to_string(),
            error,
        });
    }
    if !results.is_empty() {
        let mut updated = 0;
        let mut failed = 0;
        for result in &results {
            if result.action == "updated" {
                updated += 1;
            } else {
                failed += 1;
            }
        }
        let mut report_guard = state.bootstrap_report.lock().await;
        let mut report = report_guard.clone().unwrap_or(BootstrapReply {
            status: "running".to_string(),
            updated: 0,
            skipped: 0,
            failed: 0,
            results: Vec::new(),
        });
        for update in results.iter() {
            if let Some(existing) = report.results.iter_mut().find(|r| r.node == update.node) {
                *existing = update.clone();
            } else {
                report.results.push(update.clone());
            }
        }
        report.updated = report.results.iter().filter(|r| r.action == "updated").count() as u64;
        report.failed = report.results.iter().filter(|r| r.action == "failed").count() as u64;
        report.skipped = report.results.iter().filter(|r| r.action == "skipped").count() as u64;
        report.status = if report.failed > 0 {
            "partial_failed".to_string()
        } else {
            "ok".to_string()
        };
        *report_guard = Some(report.clone());
        drop(report_guard);
        let _ = save_bootstrap_report(&state, &report).await;
        info!(
            "Reconcile report updated: status={} updated={} failed={}",
            report.status, report.updated, report.failed
        );
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn save_bootstrap_report(state: &AppState, report: &BootstrapReply) -> anyhow::Result<()> {
    let json = serde_json::to_vec_pretty(report)?;
    fs::write(&state.bootstrap_report_path, json).await?;
    Ok(())
}

#[cfg(target_os = "linux")]
async fn load_bootstrap_report(state: &AppState) -> anyhow::Result<()> {
    if !fs::try_exists(&state.bootstrap_report_path).await? {
        return Ok(());
    }
    let data = fs::read(&state.bootstrap_report_path).await?;
    let report: BootstrapReply = serde_json::from_slice(&data)?;
    let mut stored = state.bootstrap_report.lock().await;
    *stored = Some(report);
    Ok(())
}

#[cfg(target_os = "linux")]
async fn prune_release_dirs(
    root: &Path,
    keep_name: Option<&str>,
) -> anyhow::Result<()> {
    if !fs::try_exists(root).await? {
        return Ok(());
    }
    let mut entries = fs::read_dir(root).await?;
    while let Some(entry) = entries.next_entry().await? {
        let path = entry.path();
        let name = entry
            .file_name()
            .to_str()
            .map(|s| s.to_string())
            .unwrap_or_default();
        if keep_name.is_some() && keep_name == Some(name.as_str()) {
            continue;
        }
        if entry.file_type().await?.is_dir() {
            fs::remove_dir_all(&path)
                .await
                .with_context(|| format!("removing {}", path.display()))?;
        } else {
            fs::remove_file(&path)
                .await
                .with_context(|| format!("removing {}", path.display()))?;
        }
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn cleanup_bootstrap_releases(state: &AppState) -> anyhow::Result<()> {
    let baseline_releases = baseline_root(state).join("releases");
    let keep_name = if fs::try_exists(&baseline_current_root(state)).await? {
        fs::read_link(&baseline_current_root(state))
            .await
            .ok()
            .and_then(|path| path.file_name().map(|s| s.to_string_lossy().into_owned()))
    } else {
        None
    };
    prune_release_dirs(&baseline_releases, keep_name.as_deref()).await?;
    let keep_name = if fs::try_exists(&state.local_deploy_root.join("current")).await? {
        fs::read_link(&state.local_deploy_root.join("current"))
            .await
            .ok()
            .and_then(|path| path.file_name().map(|s| s.to_string_lossy().into_owned()))
    } else {
        None
    };
    prune_release_dirs(&state.local_deploy_root.join("releases"), keep_name.as_deref()).await?;
    Ok(())
}

#[cfg(target_os = "linux")]
async fn clear_local_target_paths(
    active_payload: &Path,
    binary: &LocalPackageBinary,
    service: Option<&LocalPackageService>,
    resources: &[LocalPackageResource],
) -> anyhow::Result<()> {
    remove_payload_path(active_payload, &binary.install_path).await?;
    if let Some(service) = service {
        remove_payload_path(active_payload, &service.install_path).await?;
    }
    for resource in resources {
        remove_payload_path(active_payload, &resource.install_path).await?;
    }
    Ok(())
}

#[cfg(target_os = "linux")]
fn baseline_bundle_entry(state: &AppState, bundle_path: &str) -> anyhow::Result<PathBuf> {
    Ok(baseline_payload_root(state).join(bundle_relative_path(bundle_path)?))
}

#[cfg(target_os = "linux")]
fn baseline_local_root(state: &AppState, bundle_path: &str) -> anyhow::Result<PathBuf> {
    Ok(baseline_payload_root(state).join(bundle_relative_path(bundle_path)?))
}

#[cfg(target_os = "linux")]
async fn install_global_bundle(
    state: &AppState,
    staged: &BinaryEntry,
) -> anyhow::Result<UpdateResult> {
    let started = Instant::now();
    info!("Installing global bundle from {}", staged.path);
    lock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await?;

    let baseline_root = baseline_root(state);
    let releases_root = baseline_root.join("releases");
    fs::create_dir_all(&releases_root).await?;
    let release_root = releases_root.join(staged.hash.clone());
    if fs::try_exists(&release_root).await? {
        fs::remove_dir_all(&release_root).await?;
    }
    fs::create_dir_all(&release_root).await?;

    let release_root_str = release_root
        .to_str()
        .ok_or_else(|| anyhow!("invalid release path {}", release_root.display()))?;
    if let Err(e) = run_command("tar", &["-xzf", &staged.path, "-C", release_root_str]).await {
        let _ = unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
        return Err(e);
    }

    let script_path = match install_local_support_files(&release_root).await {
        Ok(path) => path,
        Err(e) => {
            let _ =
                unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
            return Err(e);
        }
    };
    if let Err(e) = seed_base_bin_from_payload(&release_root, state).await {
        let _ = unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
        return Err(e);
    }

    let active_root = state.local_deploy_root.join("active");
    let active_payload = active_root.join("payload");
    if fs::try_exists(&active_payload).await? {
        fs::remove_dir_all(&active_payload).await?;
    }
    fs::create_dir_all(&active_payload).await?;
    let release_payload = release_root.join("payload");
    if let Err(e) = copy_tree_contents(&release_payload, &active_payload).await {
        let _ = unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
        return Err(e);
    }

    let script_path_str = script_path
        .to_str()
        .ok_or_else(|| anyhow!("invalid script path {}", script_path.display()))?;
    let active_root_arg = active_root
        .to_str()
        .ok_or_else(|| anyhow!("invalid active path {}", active_root.display()))?;
    if let Err(e) = run_command(script_path_str, &[active_root_arg]).await {
        let _ = unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
        return Err(e);
    }
    if let Err(e) = run_command("systemctl", &["daemon-reload"]).await {
        let _ = unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
        return Err(e);
    }
    let bootstrap_startup = PathBuf::from("/usr/local/libexec/ota-agent/bootstrap-startup.sh");
    if fs::try_exists(&bootstrap_startup).await? {
        let bootstrap_startup_str = bootstrap_startup
            .to_str()
            .ok_or_else(|| anyhow!("invalid startup script path {}", bootstrap_startup.display()))?;
        info!("Running bootstrap startup script");
        if let Err(e) = run_command(bootstrap_startup_str, &[]).await {
            let _ = unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await;
            return Err(e);
        }
    }
    let _ = systemd_service("disable", "bootstrap-carputer.service").await;

    let mut enable_units = Vec::new();
    let mut restart_units = Vec::new();
    let mut start_units = Vec::new();
    for target in state.cfg.targets.values() {
        match target {
            DeployTarget::LocalPackage {
                enable_services,
                restart_services,
                ..
            } => {
                enable_units.extend(enable_services.iter().cloned());
                restart_units.extend(restart_services.iter().cloned());
            }
            DeployTarget::Uds { start_services, .. } => {
                start_units.extend(start_services.iter().cloned());
            }
            DeployTarget::LocalBundle { .. } => {}
        }
    }
    enable_units.sort();
    enable_units.dedup();
    restart_units.sort();
    restart_units.dedup();
    start_units.sort();
    start_units.dedup();

    info!(
        "Service actions: enable={} start={} restart={}",
        enable_units.len(),
        start_units.len(),
        restart_units.len()
    );
    if !enable_units.is_empty() {
        info!("Enable services: {:?}", enable_units);
    }
    if !start_units.is_empty() {
        info!("Start services: {:?}", start_units);
    }
    if !restart_units.is_empty() {
        info!("Restart services: {:?}", restart_units);
    }
    for unit in &enable_units {
        let _ = systemd_service("enable", unit).await;
    }
    for unit in &start_units {
        let _ = systemd_service("start", unit).await;
    }
    for unit in &restart_units {
        let _ = systemd_service("restart", unit).await;
    }
    for unit in enable_units
        .iter()
        .chain(start_units.iter())
        .chain(restart_units.iter())
    {
        log_systemd_status(unit).await;
    }

    let current_link = baseline_root.join("current");
    if fs::try_exists(&current_link).await? {
        let metadata = fs::symlink_metadata(&current_link).await?;
        if metadata.file_type().is_dir() && !metadata.file_type().is_symlink() {
            fs::remove_dir_all(&current_link).await?;
        } else {
            fs::remove_file(&current_link).await?;
        }
    }
    symlink(&release_root, &current_link)?;
    unlock_manifest_node(&state.manifest_path, "carputer", &state.manifest_lock).await?;

    Ok(UpdateResult {
        bin: PathBuf::from(staged.path.clone()),
        result: FlashStatus::DownloadSuccess,
        duration: started.elapsed(),
    })
}

#[cfg(target_os = "linux")]
async fn apply_local_target(
    state: &AppState,
    node: &str,
    staged: Option<&BinaryEntry>,
    artifact: &DeclaredArtifact,
    binary: &LocalPackageBinary,
    service: Option<&LocalPackageService>,
    resources: &[LocalPackageResource],
    enable_services: &[String],
    restart_services: &[String],
    lease_id: Option<&str>,
    release_lease: bool,
) -> anyhow::Result<UpdateResult> {
    let started = Instant::now();
    lock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await?;

    let bundle_path = artifact
        .bundle_path
        .as_ref()
        .ok_or_else(|| anyhow!("local target '{}' missing bundle_path", node))?;
    let baseline_node_root = baseline_local_root(state, bundle_path)?;
    if !fs::try_exists(&baseline_node_root).await? {
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(anyhow!(
            "baseline bundle content for '{}' not installed at {}",
            node,
            baseline_node_root.display()
        ));
    }

    let active_root = state.local_deploy_root.join("active");
    let active_payload = active_root.join("payload");
    acquire_service_lease(state, lease_id, restart_services).await?;
    if let Err(e) = clear_local_target_paths(&active_payload, binary, service, resources).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }

    let applied_root = if let Some(staged) = staged {
        let override_root = state.local_deploy_root.join("staged-local").join(node);
        if fs::try_exists(&override_root).await? {
            fs::remove_dir_all(&override_root).await?;
        }
        fs::create_dir_all(&override_root).await?;
        let override_root_str = override_root
            .to_str()
            .ok_or_else(|| anyhow!("invalid override path {}", override_root.display()))?;
        if let Err(e) = run_command("tar", &["-xzf", &staged.path, "-C", override_root_str]).await {
            let _ = release_service_lease(state, lease_id, restart_services).await;
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
        if let Err(e) =
            validate_local_package_contents(&override_root, node, binary, service, resources).await
        {
            let _ = release_service_lease(state, lease_id, restart_services).await;
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
        override_root.join("payload")
    } else {
        baseline_node_root.clone()
    };

    if let Err(e) = copy_tree_contents(&applied_root, &active_payload).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }
    let script_path = install_local_support_files(&baseline_current_root(state)).await?;
    let script_path_str = script_path
        .to_str()
        .ok_or_else(|| anyhow!("invalid script path {}", script_path.display()))?;
    let active_root_arg = active_root
        .to_str()
        .ok_or_else(|| anyhow!("invalid active path {}", active_root.display()))?;
    if let Err(e) = run_command(script_path_str, &[active_root_arg]).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }
    if let Err(e) = run_command("systemctl", &["daemon-reload"]).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }
    for unit in enable_services {
        if let Err(e) = systemd_service("enable", unit).await {
            let _ = release_service_lease(state, lease_id, restart_services).await;
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
    }
    if lease_id.is_none() {
        for unit in restart_services {
            if let Err(e) = systemd_service("restart", unit).await {
                let _ = release_service_lease(state, lease_id, restart_services).await;
                let _ =
                    unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
                return Err(e);
            }
        }
    } else if release_lease {
        if let Err(e) = release_service_lease(state, lease_id, restart_services).await {
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
    }
    unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await?;

    Ok(UpdateResult {
        bin: PathBuf::from(
            staged
                .map(|entry| entry.path.clone())
                .unwrap_or_else(|| applied_root.display().to_string()),
        ),
        result: FlashStatus::DownloadSuccess,
        duration: started.elapsed(),
    })
}

#[cfg(target_os = "linux")]
async fn record_local_package_baseline(
    state: &AppState,
    node: &str,
    artifact: &DeclaredArtifact,
) -> anyhow::Result<()> {
    let entry = BinaryEntry {
        filename: artifact.filename.clone(),
        path: artifact.bundle_path.clone().unwrap_or_default(),
        size: 0,
        hash: artifact.sha256.clone().unwrap_or_default(),
        declared_filename: Some(artifact.filename.clone()),
        declared_sha256: artifact.sha256.clone(),
        filename_matches_declared: Some(true),
        hash_matches_declared: artifact.sha256.as_ref().map(|_| true),
        updated_at: Utc::now().to_rfc3339(),
    };
    set_production_entry(&state.manifest_path, node, entry).await?;
    Ok(())
}

#[cfg(target_os = "linux")]
async fn apply_local_package(
    state: &AppState,
    node: &str,
    staged: &BinaryEntry,
    force: bool,
    artifact: &DeclaredArtifact,
    binary: &LocalPackageBinary,
    service: Option<&LocalPackageService>,
    resources: &[LocalPackageResource],
    enable_services: &[String],
    restart_services: &[String],
    lease_id: Option<&str>,
    release_lease: bool,
) -> anyhow::Result<UpdateResult> {
    let started = Instant::now();
    lock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await?;

    let packages_root = state.local_deploy_root.join("packages");
    let package_root = packages_root.join(node);
    let releases_root = package_root.join("releases");
    fs::create_dir_all(&releases_root).await?;

    let release_name = if force {
        format!(
            "{}-{}",
            Utc::now().format("%Y%m%dT%H%M%SZ"),
            &staged.hash[..12]
        )
    } else {
        staged.hash.clone()
    };
    let release_root = releases_root.join(release_name);
    if fs::try_exists(&release_root).await? {
        fs::remove_dir_all(&release_root)
            .await
            .with_context(|| format!("removing {}", release_root.display()))?;
    }
    fs::create_dir_all(&release_root)
        .await
        .with_context(|| format!("creating {}", release_root.display()))?;

    let archive_path = staged.path.as_str();
    let release_root_str = release_root
        .to_str()
        .ok_or_else(|| anyhow!("invalid release path {}", release_root.display()))?;
    if let Err(e) = run_command("tar", &["-xzf", archive_path, "-C", release_root_str]).await {
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }

    if let Err(e) =
        validate_local_package_contents(&release_root, node, binary, service, resources).await
    {
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }
    let _ = artifact;

    let script_path = match install_local_support_files(&release_root).await {
        Ok(path) => path,
        Err(e) => {
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
    };

    acquire_service_lease(state, lease_id, restart_services).await?;

    let active_root = state.local_deploy_root.join("active");
    let active_payload = active_root.join("payload");
    if let Err(e) = copy_tree_contents(&release_root.join("payload"), &active_payload).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }

    let script_path_str = script_path
        .to_str()
        .ok_or_else(|| anyhow!("invalid script path {}", script_path.display()))?;
    let active_root_arg = active_root
        .to_str()
        .ok_or_else(|| anyhow!("invalid active path {}", active_root.display()))?;
    if let Err(e) = run_command(script_path_str, &[active_root_arg]).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }

    if let Err(e) = run_command("systemctl", &["daemon-reload"]).await {
        let _ = release_service_lease(state, lease_id, restart_services).await;
        let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
        return Err(e);
    }

    for unit in enable_services {
        if let Err(e) = systemd_service("enable", unit).await {
            let _ = release_service_lease(state, lease_id, restart_services).await;
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
    }
    if lease_id.is_none() {
        for unit in restart_services {
            if let Err(e) = systemd_service("restart", unit).await {
                let _ = release_service_lease(state, lease_id, restart_services).await;
                let _ =
                    unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
                return Err(e);
            }
        }
    } else if release_lease {
        if let Err(e) = release_service_lease(state, lease_id, restart_services).await {
            let _ = unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await;
            return Err(e);
        }
    }

    let current_link = package_root.join("current");
    if fs::try_exists(&current_link).await? {
        let metadata = fs::symlink_metadata(&current_link).await?;
        if metadata.file_type().is_dir() && !metadata.file_type().is_symlink() {
            fs::remove_dir_all(&current_link).await?;
        } else {
            fs::remove_file(&current_link).await?;
        }
    }
    symlink(&release_root, &current_link)
        .with_context(|| format!("linking {}", current_link.display()))?;

    unlock_manifest_node(&state.manifest_path, node, &state.manifest_lock).await?;

    Ok(UpdateResult {
        bin: PathBuf::from(staged.path.clone()),
        result: FlashStatus::DownloadSuccess,
        duration: started.elapsed(),
    })
}

#[cfg(target_os = "linux")]
// POST /ota/flash?node=...
async fn flash_handler(
    p: FlashParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    validate_platform_param(&state, p.platform.as_deref())?;
    validate_target(&state, &p.node)?;
    info!("Flash request: node='{}' platform={:?}", p.node, p.platform);

    let entry = {
        let _guard = state.manifest_lock.lock().await; // just to serialize reads/writes
        match read_manifest_compat(&state.manifest_path).await {
            Ok(m) => m.nodes.get(&p.node).and_then(|nb| nb.staged.clone()),
            Err(e) => {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("failed to read manifest: {}", e),
                )));
            }
        }
    };

    let force = p.force.unwrap_or(false);
    let lease_id = p.lease_id.as_deref();
    let release_lease = p.release_lease.unwrap_or(true);
    let result = match get_target(&state, &p.node) {
        Ok(DeployTarget::LocalBundle { .. }) => {
            let staged = entry.clone().ok_or_else(|| {
                warp::reject::custom(HttpError(
                    StatusCode::BAD_REQUEST,
                    ManifestError::NoStaged(p.node.clone()).to_string(),
                ))
            })?;
            info!(
                "Installing global bundle for node '{}' (file: {})",
                &p.node, &staged.filename
            );
            install_global_bundle(&state, &staged)
                .await
                .map(|result| (result, None::<String>, staged))
        }
        Ok(DeployTarget::LocalPackage {
            artifact,
            binary,
            service,
            resources,
            enable_services,
            restart_services,
        }) => {
            let staged = entry.clone();
            let report_entry = match staged.clone() {
                Some(entry) => entry,
                None => BinaryEntry {
                    filename: artifact.filename.clone(),
                    path: artifact
                        .bundle_path
                        .clone()
                        .unwrap_or_else(|| "<baseline>".to_string()),
                    size: 0,
                    hash: String::new(),
                    declared_filename: Some(artifact.filename.clone()),
                    declared_sha256: artifact.sha256.clone(),
                    filename_matches_declared: Some(true),
                    hash_matches_declared: None,
                    updated_at: Utc::now().to_rfc3339(),
                },
            };
            info!(
                "Applying local target '{}' using {}",
                &p.node,
                if staged.is_some() {
                    "staged override"
                } else {
                    "baseline bundle"
                }
            );
            apply_local_target(
                &state,
                &p.node,
                staged.as_ref(),
                artifact,
                binary,
                service.as_ref(),
                resources,
                enable_services,
                restart_services,
                lease_id,
                release_lease,
            )
            .await
            .map(|result| (result, None::<String>, report_entry))
        }
        Ok(DeployTarget::Uds {
            request_id,
            response_id,
            artifact,
            stop_services,
            start_services,
        }) => {
            let (request_id, response_id) =
                uds_ids_for_node(&state, &p.node, *request_id, *response_id)?;
            let flash_entry = match entry.clone() {
                Some(entry) => entry,
                None => {
                    let bundle_path = artifact.bundle_path.as_ref().ok_or_else(|| {
                        warp::reject::custom(HttpError(
                            StatusCode::INTERNAL_SERVER_ERROR,
                            format!("firmware target '{}' missing bundle path", p.node),
                        ))
                    })?;
                    let baseline_path = baseline_bundle_entry(&state, bundle_path).map_err(|e| {
                        warp::reject::custom(HttpError(
                            StatusCode::INTERNAL_SERVER_ERROR,
                            e.to_string(),
                        ))
                    })?;
                    if !fs::try_exists(&baseline_path).await.map_err(|e| {
                        warp::reject::custom(HttpError(
                            StatusCode::INTERNAL_SERVER_ERROR,
                            e.to_string(),
                        ))
                    })? {
                        return Err(warp::reject::custom(HttpError(
                            StatusCode::BAD_REQUEST,
                            format!(
                                "no staged artifact and no baseline firmware for '{}'",
                                p.node
                            ),
                        )));
                    }
                    BinaryEntry {
                        filename: artifact.filename.clone(),
                        path: baseline_path.display().to_string(),
                        size: 0,
                        hash: artifact.sha256.clone().unwrap_or_default(),
                        declared_filename: Some(artifact.filename.clone()),
                        declared_sha256: artifact.sha256.clone(),
                        filename_matches_declared: Some(true),
                        hash_matches_declared: artifact.sha256.as_ref().map(|_| true),
                        updated_at: Utc::now().to_rfc3339(),
                    }
                }
            };
            if let Err(e) = acquire_service_lease(&state, lease_id, stop_services).await {
                error!("Failed to acquire service lease: {}", e);
            }

            let result = flash_node(
                &state.can_device,
                &Path::new(&flash_entry.path),
                &p.node,
                request_id,
                response_id,
                &state.manifest_path,
                &state.manifest_lock,
                force,
            )
            .await;

            if release_lease {
                if let Err(e) = release_service_lease(&state, lease_id, start_services).await {
                    error!("Failed to release service lease: {}", e);
                }
            }

            Ok((result, None::<String>, flash_entry))
        }
        Err(e) => Err(e),
    };

    let (result, _deployed_sha256, report_entry) = match result {
        Ok(result) => result,
        Err(e) => (
            UpdateResult {
                bin: PathBuf::from(
                    entry
                        .as_ref()
                        .map(|value| value.path.clone())
                        .unwrap_or_else(|| "<none>".to_string()),
                ),
                result: FlashStatus::Failed(e.to_string()),
                duration: Duration::from_secs(0),
            },
            None,
            entry.clone().unwrap_or(BinaryEntry {
                filename: String::new(),
                path: String::new(),
                size: 0,
                hash: String::new(),
                declared_filename: None,
                declared_sha256: None,
                filename_matches_declared: None,
                hash_matches_declared: None,
                updated_at: Utc::now().to_rfc3339(),
            }),
        ),
    };

    if !matches!(result.result, FlashStatus::Failed(_)) {
        info!(
            "Skipping production tracking update for '{}'; use bootstrap to set production",
            p.node
        );
    }

    let status_str = match &result.result {
        FlashStatus::DownloadSuccess => "download_success",
        FlashStatus::CrcMatch => "crc_match",
        FlashStatus::Failed(_) => "failed",
        _ => "unknown",
    }
    .to_string();

    let make_body = |error: Option<String>| {
        let duration_ms = result.duration.as_millis();
        serde_json::json!(FlashReply {
            node: p.node.clone(),
            filename: report_entry.filename.clone(),
            bytes: report_entry.size,
            sha256: report_entry.hash.clone(),
            bin: result.bin.to_string_lossy().into_owned(),
            duration_ms,
            status: status_str.clone(),
            error
        })
    };

    match result.result {
        FlashStatus::Failed(ref e) => {
            error!("Flash failed: {e}");
            let body = make_body(Some(format!("flash failed: {e}")));
            Ok(warp::reply::with_status(
                warp::reply::json(&body),
                StatusCode::INTERNAL_SERVER_ERROR,
            ))
        }
        _ => {
            info!("Flash result: node='{}' status={}", p.node, status_str);
            let body = make_body(None);
            Ok(warp::reply::with_status(
                warp::reply::json(&body),
                StatusCode::OK,
            ))
        }
    }
}

#[cfg(target_os = "linux")]
// POST /ota/bootstrap?node=...
async fn bootstrap_handler(
    p: FlashParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    validate_platform_param(&state, p.platform.as_deref())?;
    validate_target(&state, &p.node)?;
    info!("Bootstrap request: node='{}' platform={:?}", p.node, p.platform);

    let entry = {
        let _guard = state.manifest_lock.lock().await;
        match read_manifest_compat(&state.manifest_path).await {
            Ok(m) => m.nodes.get(&p.node).and_then(|nb| nb.staged.clone()),
            Err(e) => {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("failed to read manifest: {}", e),
                )));
            }
        }
    };

    let target = get_target(&state, &p.node)
        .map_err(|e| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, e.to_string())))?;
    if !matches!(target, DeployTarget::LocalBundle { .. }) {
        return Err(warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            format!(
                "bootstrap requires local_bundle target; '{}' is {}",
                p.node,
                target_kind(target)
            ),
        )));
    }

    let staged = entry.clone().ok_or_else(|| {
        warp::reject::custom(HttpError(
            StatusCode::BAD_REQUEST,
            ManifestError::NoStaged(p.node.clone()).to_string(),
        ))
    })?;

    let mut results: Vec<BootstrapNodeResult> = Vec::new();
    results.push(BootstrapNodeResult {
        node: p.node.clone(),
        kind: "bundle".to_string(),
        action: "scheduled".to_string(),
        status: "scheduled".to_string(),
        error: None,
    });
    for (node, target) in state.cfg.targets.iter() {
        if matches!(target, DeployTarget::LocalPackage { .. }) {
            results.push(BootstrapNodeResult {
                node: node.clone(),
                kind: "local_package".to_string(),
                action: "scheduled".to_string(),
                status: "scheduled".to_string(),
                error: None,
            });
        }
        if matches!(target, DeployTarget::Uds { .. }) {
            results.push(BootstrapNodeResult {
                node: node.clone(),
                kind: "uds".to_string(),
                action: "scheduled".to_string(),
                status: "scheduled".to_string(),
                error: None,
            });
        }
    }

    {
        let mut report = state.bootstrap_report.lock().await;
        let report_value = BootstrapReply {
            status: "running".to_string(),
            updated: 0,
            skipped: 0,
            failed: 0,
            results: results.clone(),
        };
        *report = Some(report_value.clone());
        drop(report);
        if let Err(e) = save_bootstrap_report(&state, &report_value).await {
            error!("Failed to save bootstrap report: {}", e);
        }
    }

    let state_for_task = Arc::clone(&state);
    let staged_for_task = staged.clone();
    let node_for_task = p.node.clone();
    tokio::spawn(async move {
        if let Err(e) = apply_bootstrap(state_for_task, staged_for_task, node_for_task).await {
            error!("Bootstrap apply failed: {}", e);
        }
    });

    let body = BootstrapReply {
        status: "scheduled".to_string(),
        updated: 0,
        skipped: 0,
        failed: 0,
        results,
    };
    Ok(warp::reply::with_status(
        warp::reply::json(&body),
        StatusCode::ACCEPTED,
    ))
}

#[cfg(target_os = "linux")]
async fn bootstrap_status_handler(
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    let report = state.bootstrap_report.lock().await;
    let Some(report) = report.as_ref() else {
        return Err(warp::reject::custom(HttpError(
            StatusCode::NOT_FOUND,
            "no bootstrap report available".to_string(),
        )));
    };
    Ok(warp::reply::with_status(
        warp::reply::json(&report),
        StatusCode::OK,
    ))
}

#[cfg(target_os = "linux")]
async fn apply_bootstrap(
    state: Arc<AppState>,
    staged: BinaryEntry,
    node: String,
) -> anyhow::Result<()> {
    info!("Bootstrap apply: starting");
    let mut results: Vec<BootstrapNodeResult> = Vec::new();

    if let Err(e) = clear_bootstrap_state(&state).await {
        let mut report = state.bootstrap_report.lock().await;
        let report_value = BootstrapReply {
            status: "failed".to_string(),
            updated: 0,
            skipped: 0,
            failed: 1,
            results: vec![BootstrapNodeResult {
                node: node.clone(),
                kind: "bundle".to_string(),
                action: "failed".to_string(),
                status: "failed".to_string(),
                error: Some(e.to_string()),
            }],
        };
        *report = Some(report_value.clone());
        drop(report);
        let _ = save_bootstrap_report(&state, &report_value).await;
        return Err(e);
    }
    info!("Bootstrap apply: cleared local packages/staged-local");

    {
        let _guard = state.manifest_lock.lock().await;
        let _ = collect_and_clear_flashing_nodes(&state.manifest_path).await;
    }

    let mut bundle_result = None;
    for attempt in 1..=5 {
        match install_global_bundle(&state, &staged).await {
            Ok(result) => {
                bundle_result = Some(result);
                break;
            }
            Err(e) => {
                let msg = e.to_string();
                if msg.contains("already being flashed") && attempt < 5 {
                    info!(
                        "Bootstrap apply: bundle busy, retrying {}/5",
                        attempt + 1
                    );
                    tokio::time::sleep(Duration::from_secs(2)).await;
                    continue;
                }
                let mut report = state.bootstrap_report.lock().await;
                let report_value = BootstrapReply {
                    status: "failed".to_string(),
                    updated: 0,
                    skipped: 0,
                    failed: 1,
                    results: vec![BootstrapNodeResult {
                        node: node.clone(),
                        kind: "bundle".to_string(),
                        action: "failed".to_string(),
                        status: "failed".to_string(),
                        error: Some(msg),
                    }],
                };
                *report = Some(report_value.clone());
                drop(report);
                let _ = save_bootstrap_report(&state, &report_value).await;
                return Err(e);
            }
        }
    }
    let result = bundle_result.ok_or_else(|| anyhow!("bundle install failed"))?;
    info!(
        "Bootstrap apply: installed bundle status={}",
        flash_status_label(&result.result)
    );
    results.push(BootstrapNodeResult {
        node: node.clone(),
        kind: "bundle".to_string(),
        action: if matches!(result.result, FlashStatus::Failed(_)) {
            "failed"
        } else {
            "updated"
        }
        .to_string(),
        status: flash_status_label(&result.result).to_string(),
        error: match result.result {
            FlashStatus::Failed(ref e) => Some(e.clone()),
            _ => None,
        },
    });

    let _ = systemd_service("stop", "bootstrap-carputer.service").await;
    let _ = systemd_service("disable", "bootstrap-carputer.service").await;

    cleanup_bootstrap_releases(&state).await?;
    info!("Bootstrap apply: cleaned old releases");

    clear_staged_uploads(&state).await?;
    info!("Bootstrap apply: cleared staged uploads");

    {
        let _guard = state.manifest_lock.lock().await;
        let bundle_entry = BinaryEntry {
            filename: staged.filename.clone(),
            path: staged.path.clone(),
            size: staged.size,
            hash: staged.hash.clone(),
            declared_filename: staged.declared_filename.clone(),
            declared_sha256: staged.declared_sha256.clone(),
            filename_matches_declared: staged.filename_matches_declared,
            hash_matches_declared: staged.hash_matches_declared,
            updated_at: Utc::now().to_rfc3339(),
        };
        set_production_entry(&state.manifest_path, &node, bundle_entry).await?;
    }
    info!("Bootstrap apply: recorded production bundle");

    {
        let _guard = state.manifest_lock.lock().await;
        clear_all_staged_entries(&state.manifest_path).await?;
    }
    info!("Bootstrap apply: cleared staged manifest entries");

    {
        let _guard = state.manifest_lock.lock().await;
        for (node, target) in state.cfg.targets.iter() {
            if let DeployTarget::LocalPackage { artifact, .. } = target {
                match record_local_package_baseline(&state, node, artifact).await {
                    Ok(()) => results.push(BootstrapNodeResult {
                        node: node.clone(),
                        kind: "local_package".to_string(),
                        action: "updated".to_string(),
                        status: "updated".to_string(),
                        error: None,
                    }),
                    Err(e) => results.push(BootstrapNodeResult {
                        node: node.clone(),
                        kind: "local_package".to_string(),
                        action: "failed".to_string(),
                        status: "failed".to_string(),
                        error: Some(e.to_string()),
                    }),
                }
            }
        }
    }
    info!("Bootstrap apply: recorded production baselines for local packages");

    let mut uds_targets = Vec::new();
    let mut stop_services = Vec::new();
    let mut start_services = Vec::new();
    for (node, target) in state.cfg.targets.iter() {
        if let DeployTarget::Uds {
            request_id,
            response_id,
            artifact,
            stop_services: target_stop,
            start_services: target_start,
        } = target
        {
            uds_targets.push((
                node.clone(),
                *request_id,
                *response_id,
                artifact.clone(),
            ));
            stop_services.extend(target_stop.clone());
            start_services.extend(target_start.clone());
        }
    }

    stop_services.sort();
    stop_services.dedup();
    start_services.sort();
    start_services.dedup();

    let lease_id = format!("bootstrap-{}", Utc::now().timestamp_millis());
    acquire_service_lease(&state, Some(&lease_id), &stop_services).await?;

    let force = false;
    info!("Bootstrap apply: flashing {} UDS targets", uds_targets.len());
    for (node, request_id, response_id, artifact) in uds_targets {
        let (request_id, response_id) = uds_ids_for_node(&state, &node, request_id, response_id)
            .map_err(|e| anyhow!(format!("uds ids for {}: {:?}", node, e)))?;
        let bundle_path = artifact
            .bundle_path
            .as_ref()
            .ok_or_else(|| anyhow!("firmware target '{}' missing bundle path", node))?;
        let baseline_path = baseline_bundle_entry(&state, bundle_path)?;
        if !fs::try_exists(&baseline_path).await? {
            error!(
                "Bootstrap apply: missing baseline firmware for '{}' at {}",
                node,
                baseline_path.display()
            );
            results.push(BootstrapNodeResult {
                node: node.clone(),
                kind: "uds".to_string(),
                action: "failed".to_string(),
                status: "missing_baseline".to_string(),
                error: Some("missing baseline firmware".to_string()),
            });
            continue;
        }

        let result = flash_node(
            &state.can_device,
            &baseline_path,
            &node,
            request_id,
            response_id,
            &state.manifest_path,
            &state.manifest_lock,
            force,
        )
        .await;

        info!("Bootstrap apply: flashed '{}' status={:?}", node, result.result);
        let error = match &result.result {
            FlashStatus::Failed(e) => Some(e.clone()),
            _ => None,
        };
        results.push(BootstrapNodeResult {
            node: node.clone(),
            kind: "uds".to_string(),
            action: if error.is_some() { "failed" } else { "updated" }.to_string(),
            status: flash_status_label(&result.result).to_string(),
            error: error.clone(),
        });
        if error.is_none() {
            let baseline_entry = BinaryEntry {
                filename: artifact.filename.clone(),
                path: baseline_path.display().to_string(),
                size: 0,
                hash: artifact.sha256.clone().unwrap_or_default(),
                declared_filename: Some(artifact.filename.clone()),
                declared_sha256: artifact.sha256.clone(),
                filename_matches_declared: Some(true),
                hash_matches_declared: artifact.sha256.as_ref().map(|_| true),
                updated_at: Utc::now().to_rfc3339(),
            };
            let _guard = state.manifest_lock.lock().await;
            set_production_entry(&state.manifest_path, &node, baseline_entry).await?;
        }
    }

    release_service_lease(&state, Some(&lease_id), &start_services).await?;

    let mut updated = 0;
    let mut skipped = 0;
    let mut failed = 0;
    for result in &results {
        match result.action.as_str() {
            "updated" => updated += 1,
            "skipped" => skipped += 1,
            _ => failed += 1,
        }
    }
    let any_failed = failed > 0;
    let status = if any_failed {
        "partial_failed".to_string()
    } else {
        "ok".to_string()
    };
    {
        let mut report = state.bootstrap_report.lock().await;
        let report_value = BootstrapReply {
            status: status.clone(),
            updated,
            skipped,
            failed,
            results: results.clone(),
        };
        *report = Some(report_value.clone());
        drop(report);
        let _ = save_bootstrap_report(&state, &report_value).await;
    }

    if any_failed {
        let mut reasons = HashMap::new();
        for result in results.iter().filter(|r| r.action == "failed") {
            let reason = result
                .error
                .clone()
                .unwrap_or_else(|| "unknown".to_string());
            *reasons.entry(reason).or_insert(0u64) += 1;
        }
        let mut reason_rows = reasons
            .iter()
            .map(|(reason, count)| vec![count.to_string(), reason.clone()])
            .collect::<Vec<_>>();
        reason_rows.sort_by(|a, b| b[0].cmp(&a[0]));
        info!(
            "Bootstrap partial_failed: {} failed, {} updated, {} skipped",
            failed, updated, skipped
        );
        info!("Failure reasons:");
        info!("{}", render_table(&["count", "reason"], &reason_rows));
    }

    let mut result_rows = results
        .iter()
        .map(|r| {
            vec![
                r.node.clone(),
                r.kind.clone(),
                r.action.clone(),
                r.status.clone(),
                r.error.clone().unwrap_or_else(|| "-".to_string()),
            ]
        })
        .collect::<Vec<_>>();
    result_rows.sort_by(|a, b| a[0].cmp(&b[0]));
    info!("Bootstrap results:");
    info!(
        "{}",
        render_table(&["node", "kind", "action", "status", "error"], &result_rows)
    );

    if any_failed {
        info!("Bootstrap completed with failures; restarting ota-agent anyway");
    }
    tokio::spawn(async move {
        tokio::time::sleep(Duration::from_secs(10)).await;
        if let Err(e) = run_command(
            "systemd-run",
            &[
                "--on-active=5s",
                "--unit",
                "ota-agent-bootstrap-restart",
                "/bin/bash",
                "-lc",
                "systemctl restart ota-agent-drive-stack.service && systemctl restart ota-agent.service",
            ],
        )
        .await
        {
            error!("Failed to schedule ota-agent restart: {}", e);
        } else {
            info!("Scheduled ota-agent restart via systemd-run");
        }
    });

    Ok(())
}

#[cfg(target_os = "linux")]
async fn revert_handler(
    p: RevertParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    validate_target(&state, &p.node)?;

    let target = get_target(&state, &p.node)
        .map_err(|e| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, e.to_string())))?;

    {
        let _guard = state.manifest_lock.lock().await;
        clear_staged_entry(&state.manifest_path, &p.node)
            .await
            .map_err(|e| {
                warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("failed to clear staged entry: {}", e),
                ))
            })?;
    }

    match target {
        DeployTarget::LocalBundle { .. } => {}
        DeployTarget::LocalPackage {
            artifact,
            binary,
            service,
            resources,
            enable_services,
            restart_services,
        } => {
            apply_local_target(
                &state,
                &p.node,
                None,
                artifact,
                binary,
                service.as_ref(),
                resources,
                enable_services,
                restart_services,
                None,
                true,
            )
            .await
            .map_err(|e| {
                warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("revert failed: {}", e),
                ))
            })?;
        }
        DeployTarget::Uds {
            request_id,
            response_id,
            artifact,
            stop_services,
            start_services,
        } => {
            let (request_id, response_id) =
                uds_ids_for_node(&state, &p.node, *request_id, *response_id)?;
            let bundle_path = artifact.bundle_path.as_ref().ok_or_else(|| {
                warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("firmware target '{}' missing bundle path", p.node),
                ))
            })?;
            let baseline_path = baseline_bundle_entry(&state, bundle_path).map_err(|e| {
                warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
            })?;
            if !fs::try_exists(&baseline_path).await.map_err(|e| {
                warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string()))
            })? {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::BAD_REQUEST,
                    format!("no baseline artifact available for '{}'", p.node),
                )));
            }
            let _ = acquire_service_lease(&state, None, stop_services).await;
            let result = flash_node(
                &state.can_device,
                &baseline_path,
                &p.node,
                request_id,
                response_id,
                &state.manifest_path,
                &state.manifest_lock,
                false,
            )
            .await;
            let _ = release_service_lease(&state, None, start_services).await;
            if matches!(result.result, FlashStatus::Failed(_)) {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("revert failed: {:?}", result.result),
                )));
            }
        }
    }

    let body = serde_json::json!({
        "status": "reverted",
        "node": p.node,
    });
    Ok(warp::reply::with_status(
        warp::reply::json(&body),
        StatusCode::OK,
    ))
}

#[cfg(target_os = "linux")]
async fn read_manifest_compat(path: &Path) -> anyhow::Result<BinariesManifest> {
    match fs::read(path).await {
        Ok(bytes) => {
            if let Ok(new) = serde_yaml::from_slice::<BinariesManifest>(&bytes) {
                return Ok(new);
            } else {
                Err(anyhow!("Invalid binary manifest"))
            }
        }
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(BinariesManifest::default()),
        Err(e) => Err(e).context("reading binaries manifest"),
    }
}

#[cfg(target_os = "linux")]
async fn production_entry_for_target(
    state: &AppState,
    target: &DeployTarget,
) -> anyhow::Result<Option<BinaryEntry>> {
    let artifact = declared_artifact(target);
    let declared_sha = artifact.sha256.clone();
    let mut hash = declared_sha.clone();
    let mut path = artifact.bundle_path.clone().unwrap_or_default();

    match target {
        DeployTarget::LocalBundle { .. } => {
            let current_link = baseline_current_root(state);
            if fs::try_exists(&current_link).await? {
                if let Ok(dest) = fs::read_link(&current_link).await {
                    if let Some(name) = dest.file_name().map(|s| s.to_string_lossy().into_owned())
                    {
                        hash = Some(name);
                    }
                }
                path = current_link.display().to_string();
            }
        }
        DeployTarget::LocalPackage { .. } => {
            if let Some(bundle_path) = artifact.bundle_path.as_ref() {
                let baseline_path = baseline_local_root(state, bundle_path)?;
                if fs::try_exists(&baseline_path).await? {
                    path = baseline_path.display().to_string();
                }
            }
        }
        DeployTarget::Uds { .. } => {
            if let Some(bundle_path) = artifact.bundle_path.as_ref() {
                let baseline_path = baseline_bundle_entry(state, bundle_path)?;
                if fs::try_exists(&baseline_path).await? {
                    path = baseline_path.display().to_string();
                }
            }
        }
    }

    if hash.as_ref().map(|v| v.is_empty()).unwrap_or(true) && !path.is_empty() {
        let hash_path = Path::new(&path);
        if let Ok(metadata) = fs::metadata(hash_path).await {
            if metadata.is_file() {
                let bytes = fs::read(hash_path).await?;
                let mut sha = Sha256::new();
                sha.update(&bytes);
                hash = Some(hex::encode(sha.finalize()));
            }
        }
    }

    let Some(hash) = hash else {
        return Ok(None);
    };

    let hash_matches_declared = artifact
        .sha256
        .as_ref()
        .map(|declared| declared == &hash);

    Ok(Some(BinaryEntry {
        filename: artifact.filename.clone(),
        path,
        size: 0,
        hash,
        declared_filename: Some(artifact.filename.clone()),
        declared_sha256: artifact.sha256.clone(),
        filename_matches_declared: Some(true),
        hash_matches_declared,
        updated_at: Utc::now().to_rfc3339(),
    }))
}

#[cfg(target_os = "linux")]
async fn ensure_production_entries_on_startup(state: &AppState) -> anyhow::Result<()> {
    let _guard = state.manifest_lock.lock().await;
    let mut manifest = read_manifest_compat(&state.manifest_path).await?;
    let mut updated = 0usize;
    let mut missing = Vec::new();

    for (node, target) in state.cfg.targets.iter() {
        let needs_production = match manifest.nodes.get(node).and_then(|entry| entry.production.as_ref()) {
            Some(prod) => prod.hash.is_empty(),
            None => true,
        };
        if !needs_production {
            continue;
        }
        if let Some(entry) = production_entry_for_target(state, target).await? {
            manifest
                .nodes
                .entry(node.clone())
                .or_default()
                .production = Some(entry);
            updated += 1;
        } else {
            missing.push(node.clone());
        }
    }

    if updated > 0 {
        write_manifest(&state.manifest_path, &manifest).await?;
        info!(
            "Recorded production baselines on startup for {} nodes",
            updated
        );
    }
    if !missing.is_empty() {
        missing.sort();
        info!(
            "Missing production sha on startup for {} nodes: {}",
            missing.len(),
            missing.join(", ")
        );
    }
    Ok(())
}

#[cfg(target_os = "linux")]
async fn write_manifest(path: &Path, manifest: &BinariesManifest) -> anyhow::Result<()> {
    let tmp_path = path.with_extension("yaml.tmp");
    let yaml = serde_yaml::to_string(&manifest).context("serializing binaries manifest")?;
    fs::write(&tmp_path, yaml.as_bytes())
        .await
        .context("writing temp manifest")?;
    fs::rename(&tmp_path, path)
        .await
        .context("renaming temp manifest")?;
    Ok(())
}

#[cfg(target_os = "linux")]
async fn update_manifest_stage(
    manifest_path: &Path,
    sha256_hex: &str,
    filename: &str,
    file_path: &Path,
    size: u64,
    node: &str,
    declared_artifact: &DeclaredArtifact,
) -> anyhow::Result<()> {
    let mut manifest = read_manifest_compat(manifest_path).await?;

    // Cross-node uniqueness checks for staged+production
    for (saved_node, bins) in &manifest.nodes {
        if node != saved_node {
            if let Some(ref b) = bins.staged {
                if sha256_hex == b.hash {
                    return Err(ManifestError::NodeMismatch {
                        existing: saved_node.to_string(),
                        incoming: node.to_string(),
                    }
                    .into());
                }
                if filename == b.filename {
                    return Err(ManifestError::FilenameError {
                        existing: saved_node.to_string(),
                        incoming: node.to_string(),
                    }
                    .into());
                }
            }
            if let Some(ref b) = bins.production {
                if sha256_hex == b.hash {
                    return Err(ManifestError::NodeMismatch {
                        existing: saved_node.to_string(),
                        incoming: node.to_string(),
                    }
                    .into());
                }
                if filename == b.filename {
                    return Err(ManifestError::FilenameError {
                        existing: saved_node.to_string(),
                        incoming: node.to_string(),
                    }
                    .into());
                }
            }
        }
    }

    let entry = BinaryEntry {
        filename: filename.to_string(),
        path: file_path.to_string_lossy().into_owned(),
        size,
        hash: sha256_hex.to_string(),
        declared_filename: Some(declared_artifact.filename.clone()),
        declared_sha256: declared_artifact.sha256.clone(),
        filename_matches_declared: Some(filename == declared_artifact.filename),
        hash_matches_declared: Some(
            declared_artifact
                .sha256
                .as_ref()
                .map(|declared| declared == sha256_hex)
                .unwrap_or(false),
        ),
        updated_at: Utc::now().to_rfc3339(),
    };

    manifest
        .nodes
        .entry(node.to_string())
        .and_modify(|nb| nb.staged = Some(entry.clone()))
        .or_insert_with(|| NodeBinaries {
            flashing: false,
            staged: Some(entry),
            production: None,
        });

    write_manifest(manifest_path, &manifest).await
}

#[cfg(target_os = "linux")]
async fn clear_staged_entry(
    manifest_path: &Path,
    node: &str,
) -> anyhow::Result<Option<BinaryEntry>> {
    let mut manifest = read_manifest_compat(manifest_path).await?;
    let cleared = manifest
        .nodes
        .get_mut(node)
        .and_then(|entry| entry.staged.take());
    write_manifest(manifest_path, &manifest).await?;
    Ok(cleared)
}

#[cfg(target_os = "linux")]
async fn clear_all_staged_entries(manifest_path: &Path) -> anyhow::Result<()> {
    let mut manifest = read_manifest_compat(manifest_path).await?;
    for entry in manifest.nodes.values_mut() {
        entry.staged = None;
    }
    write_manifest(manifest_path, &manifest).await
}

#[cfg(target_os = "linux")]
async fn set_production_entry(
    manifest_path: &Path,
    node: &str,
    entry: BinaryEntry,
) -> anyhow::Result<()> {
    let mut manifest = read_manifest_compat(manifest_path).await?;
    let nb = manifest
        .nodes
        .entry(node.to_string())
        .or_insert_with(NodeBinaries::default);
    nb.production = Some(entry);
    write_manifest(manifest_path, &manifest).await
}

#[cfg(target_os = "linux")]
async fn load_manifest(path: &str) -> Result<Config> {
    let data = fs::read(path)
        .await
        .with_context(|| format!("reading {}", path))?;
    let cfg: Config = serde_yaml::from_slice(&data).with_context(|| "parsing YAML")?;
    Ok(cfg)
}

#[cfg(target_os = "linux")]
async fn load_uds_manifest(path: &str) -> Result<UdsManifest> {
    let data = fs::read(path)
        .await
        .with_context(|| format!("reading {}", path))?;
    let manifest: UdsManifest = serde_yaml::from_slice(&data).with_context(|| "parsing YAML")?;
    Ok(manifest)
}

#[cfg(target_os = "linux")]
fn start_mdns_advertisement(
    interface: String,
    service_type: String,
    ip: IpAddr,
    port: u16,
    platform: String,
) {
    let instance_name = get_hostname()
        .ok()
        .and_then(|os| os.into_string().ok())
        .unwrap_or_else(|| "ota-agent".to_string());
    let host_name = format!("{}.local.", instance_name);

    let mut txt: HashMap<String, String> = HashMap::new();
    txt.insert("api".to_string(), "/ota".to_string());
    txt.insert("proto".to_string(), "http".to_string());
    if !platform.is_empty() {
        txt.insert("platform".to_string(), platform);
    }

    let mdns = MdnsServer {
        interface: interface.clone(),
        service_type: service_type.clone(),
        instance_name: instance_name.clone(),
        host_name: host_name.clone(),
        ip: Some(ip.to_string()),
        port,
        txt,
    };

    thread::spawn(move || {
        if let Ok(dns_server) = mdns.start() {
            while dns_server.is_alive() {
                thread::sleep(Duration::from_secs(60));
            }
            let _ = dns_server.try_exit();
        }
    });
}

fn find_interface_ipv4(ifname: &str) -> Result<Ipv4Addr> {
    for iface in get_if_addrs()? {
        if iface.name == ifname {
            if let IpAddr::V4(v4) = iface.ip() {
                return Ok(v4);
            }
        }
    }
    bail!("No IPv4 found on interface '{ifname}'");
}

fn default_iface_name() -> String {
    if let Ok(addrs) = if_addrs::get_if_addrs() {
        for a in addrs {
            if !a.is_loopback() {
                return a.name;
            }
        }
    }
    "lo".to_string()
}

fn sanitize_filename(raw: &str) -> String {
    let just = std::path::Path::new(raw)
        .file_name()
        .unwrap_or_default()
        .to_string_lossy();
    let s = just
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || "-._".contains(c) {
                c
            } else {
                '_'
            }
        })
        .collect::<String>();
    if s.is_empty() {
        "firmware.bin".into()
    } else {
        s
    }
}

fn int_err<E: std::fmt::Display>(e: E) -> (StatusCode, String) {
    (StatusCode::INTERNAL_SERVER_ERROR, e.to_string())
}

// warp error mapping
#[derive(Debug)]
struct HttpError(StatusCode, String);
impl warp::reject::Reject for HttpError {}

impl From<HttpError> for warp::reply::WithStatus<warp::reply::Json> {
    fn from(e: HttpError) -> Self {
        let body = serde_json::json!({ "error": e.1 });
        warp::reply::with_status(warp::reply::json(&body), e.0)
    }
}

// global recover to turn rejections into JSON
async fn recover_json(err: warp::Rejection) -> Result<impl warp::Reply, std::convert::Infallible> {
    use warp::reject;

    // Known custom error
    if let Some(HttpError(code, msg)) = err.find::<HttpError>() {
        return Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": msg })),
            *code,
        ));
    }

    // Common Warp rejections we want to surface with clearer messages
    if err.is_not_found() {
        return Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": "not found" })),
            StatusCode::NOT_FOUND,
        ));
    }
    if let Some(_) = err.find::<reject::MethodNotAllowed>() {
        return Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": "method not allowed" })),
            StatusCode::METHOD_NOT_ALLOWED,
        ));
    }
    if let Some(_) = err.find::<reject::PayloadTooLarge>() {
        return Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": "payload too large" })),
            StatusCode::PAYLOAD_TOO_LARGE,
        ));
    }
    if let Some(_) = err.find::<reject::LengthRequired>() {
        return Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": "content-length required" })),
            StatusCode::LENGTH_REQUIRED,
        ));
    }
    if let Some(_) = err.find::<reject::UnsupportedMediaType>() {
        return Ok(warp::reply::with_status(
            warp::reply::json(
                &serde_json::json!({ "error": "unsupported media type (expect multipart/form-data)" }),
            ),
            StatusCode::UNSUPPORTED_MEDIA_TYPE,
        ));
    }
    if let Some(e) = err.find::<warp::reject::InvalidQuery>() {
        return Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": format!("invalid query: {}", e) })),
            StatusCode::BAD_REQUEST,
        ));
    }

    // Fallback: dump a debug-ish string to help diagnose during development
    let msg = format!("unhandled rejection: {:?}", err);
    Ok(warp::reply::with_status(
        warp::reply::json(&serde_json::json!({ "error": msg })),
        StatusCode::INTERNAL_SERVER_ERROR,
    ))
}

fn print_deployment_report(results: &[(String, UpdateResult)], total_dur: Duration) {
    let total = results.len() as u64;
    let successes = results
        .iter()
        .filter(|(_node, r)| !matches!(r.result, FlashStatus::Failed(_)))
        .count() as u64;
    let failures = total - successes;
    let avg = average_duration(
        &results
            .iter()
            .map(|(_node, r)| r.duration)
            .collect::<Vec<_>>(),
    );
    let success_rate = if total > 0 {
        (successes as f64) * 100.0 / (total as f64)
    } else {
        0.0
    };

    info!("");
    info!("===================== Deployment Report =====================");
    info!("Total nodes      : {}", total);
    info!("Succeeded        : {}", successes);
    info!("Failed           : {}", failures);
    info!("Success rate     : {:.1}%", success_rate);
    info!("Total time       : {}", fmt_dur(total_dur));
    info!("Avg per-node time: {}", fmt_dur(avg));
    info!("=============================================================");
    info!(
        "{:<18}  {:<28}  {:<10}  {}",
        "Node", "Binary", "Elapsed", "Status"
    );
    info!("{}", "-".repeat(18 + 2 + 28 + 2 + 10 + 2 + 10 + 2 + 5 + 16));

    for (node, r) in results {
        let status = format!("{:?}", r.result);
        let bin_str = r.bin.to_string_lossy();
        info!(
            "{:<18}  {:<28}  {:<10}  {}",
            node,
            truncate(&bin_str, 28),
            fmt_dur(r.duration),
            status,
        );
    }

    if failures > 0 {
        info!("");
        info!("--- Failure details ---");
        for (node, r) in results
            .iter()
            .filter(|(_node, r)| matches!(r.result, FlashStatus::Failed(_)))
        {
            info!(
                "node='{}' bin='{}' error='{:?}'",
                node,
                r.bin.to_string_lossy(),
                r.result
            );
        }
    }
}

/// Helper to keep table columns tidy without extra crates
fn truncate(s: &str, max: usize) -> String {
    if s.len() <= max {
        s.to_string()
    } else if max > 1 {
        let mut t = s.chars().take(max - 1).collect::<String>();
        t.push('…');
        t
    } else {
        "…".to_string()
    }
}

fn average_duration(durations: &[Duration]) -> Duration {
    if durations.is_empty() {
        return Duration::from_secs(0);
    }
    let total_nanos: u128 = durations.iter().map(|d| d.as_nanos()).sum();
    let avg_nanos = total_nanos / (durations.len() as u128);
    Duration::from_nanos(avg_nanos as u64)
}

fn parse_update_pair(s: &str) -> Option<(String, PathBuf)> {
    // expects "node:/path/to/bin"
    let (node, path) = s.split_once(':')?;
    Some((node.to_string(), PathBuf::from(path)))
}

fn fmt_dur(d: Duration) -> String {
    let secs = d.as_secs();
    let ms = d.subsec_millis();
    format!("{secs}.{ms:03}s")
}
