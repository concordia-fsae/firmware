use std::{
    collections::HashMap,
    net::{IpAddr, Ipv4Addr, SocketAddr},
    path::{Path, PathBuf},
    io::Read,
    fs::File,
    sync::Arc,
    thread,
    time::{Duration, Instant},
};

use anyhow::{anyhow, bail, Context, Result};
use argh::FromArgs;
use bytes::Buf; // for chunk.chunk()
use chrono::Utc;
use futures_util::{StreamExt, TryStreamExt};
use hex;
use hostname::get as get_hostname;
use if_addrs::get_if_addrs;
use reqwest::{Client, multipart, Url};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use thiserror::Error;
use tokio::{fs, io::AsyncWriteExt};
use tracing::{info, error};
use tracing_subscriber::EnvFilter;
use warp::{http::StatusCode, Filter};

#[cfg(target_os = "linux")]
use conUDS::modules::uds::UdsSession;
use conUDS::{FlashStatus, UpdateResult};
use net_detec::Server as MdnsServer;
use net_detec::Client as MdnsClient;
use net_detec::{DiscoveryFilter, DiscoveredService};

/// Host a Rest API with ability to upload and deploy applications
#[derive(FromArgs, Debug)]
struct Args {
    /// service type like _carputer-uds._tcp.local.
    #[argh(option, short = 's', default = "String::from(\"_ota-agent._tcp.local.\")")]
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
    Batch(SubActionBatch),
}

/// Stage a binary to a node
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "stage")]
pub struct SubActionStage {
    /// the node to flash
    #[argh(option, short = 'n')]
    pub node: String,
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
    /// the binary to flash
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
    /// repeatable node-to-binary pairs in the form `-u node:/path/to/bin`
    /// example: -u mcu:build/app_mcu.bin -u imu:build/app_imu.bin
    #[argh(option, short = 'u')]
    pub targets: Vec<String>,
    /// force flashing even if there is a binary match
    #[argh(switch, short = 'f')]
    force: bool,
}

#[cfg(target_os = "linux")]
/// Start an OTA agent server
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "server")]
pub struct SubArgServer {
    /// the CAN device to use. `can0` is used if this option is not provided
    #[argh(option, short = 't', default = "String::from(\"can0\")")]
    device: String,
    /// the manifest of UDS can nodes on the bus
    #[argh(option, short = 'm', default = "String::from(\"drive-stack/conUDS/nodes.yml\")")]
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
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct UdsNode {
    request_id: u32,
    response_id: u32,
}

#[cfg(target_os = "linux")]
#[derive(Clone, Debug, Deserialize)]
struct Config {
    nodes: HashMap<String, UdsNode>,
}

#[cfg(target_os = "linux")]
#[derive(Clone)]
struct AppState {
    cfg: Arc<Config>,
    can_device: String,
    save_dir: PathBuf,
    manifest_path: PathBuf,
    manifest_lock: Arc<tokio::sync::Mutex<()>>,
}

#[derive(Debug, Deserialize, Serialize)]
struct FlashParams {
    node: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    sha: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    force: Option<bool>,
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

#[derive(Debug, Serialize, Deserialize)]
struct VerifyReply {
    node: String,
    filename: String,
    sha256: String,
    matched: bool,
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

            let state = Arc::new(AppState {
                cfg: Arc::new(cfg),
                can_device: server.device.clone(),
                save_dir: server.save_dir.clone(),
                manifest_path: server.save_dir.join("binaries.yaml"),
                manifest_lock: Arc::new(tokio::sync::Mutex::new(())),
            });

            let ip: IpAddr = match &server.ip {
                Some(s) => s
                    .parse()
                    .with_context(|| format!("parsing ip {}", s))?,
                None => IpAddr::V4(find_interface_ipv4(server.interface.as_deref().unwrap())?),
            };
            let addr = SocketAddr::new(ip, server.port);

            start_mdns_advertisement(
                server.interface.clone().unwrap_or_else(default_iface_name),
                args.service_name.clone(),
                ip,
                server.port,
            );

            let state_filter = warp::any().map(move || Arc::clone(&state));

            // POST /firmware/verify
            let verify_route = warp::path("firmware")
                .and(warp::path("verify"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and_then(verify_handler);

            // POST /firmware/stage  (multipart form: file)
            let stage_route = warp::path("firmware")
                .and(warp::path("stage"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and(warp::multipart::form().max_length(1024 * 1024 * 512)) // 512MB cap
                .and_then(stage_handler);

            // POST /firmware/flash  (no body; uses staged manifest)
            let flash_route = warp::path("firmware")
                .and(warp::path("flash"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and_then(flash_handler);

            // POST /firmware/promote (no body; move staged -> production)
            let promote_route = warp::path("firmware")
                .and(warp::path("promote"))
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter.clone())
                .and_then(promote_handler);

            let routes = stage_route
                .or(verify_route)
                .or(flash_route)
                .or(promote_route)
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
                    // Build multipart form with the file
                    let mut file = File::open(&flash.binary)?;
                    let mut buffer = Vec::new();
                    let fname: String = Path::new(&flash.binary)
                        .file_name()
                        .map(|s| s.to_string_lossy().into_owned()).expect("Invalid filename");
                    file.read_to_end(&mut buffer)?;
                    let part = multipart::Part::bytes(buffer)
                        .file_name(fname.clone())
                        .mime_str("application/octet-stream")?;
                    let form = multipart::Form::new().part("file", part);

                    // Stage
                    info!("Staging {} to the agent...", &fname);
                    let rest_client = Client::new();
                    let url = build_url(result.addresses[0], result.port, "/firmware/stage");
                    let response = rest_client
                        .post(url)
                        .query(&[("node", flash.node.clone())])
                        .multipart(form)
                        .send().await?;

                    let stage_body = response.text().await?;
                    let parsed: Result<FlashReply, _> = serde_json::from_str(&stage_body);
                    info!("Flash status: {}", parsed.unwrap().status);
                }
                SubAction::Flash(flash) => {
                    // Flash
                    info!("Requesting flash of staged binary...");
                    let url_flash = build_url(result.addresses[0], result.port, "/firmware/flash");
                    let resp_flash = Client::new()
                        .post(url_flash)
                        .query(&[("node", flash.node), ("force", if flash.force { "true".into() } else { "false".into() })])
                        .send().await?;
                    let flash_body = resp_flash.text().await?;
                    let parsed: Result<FlashReply, _> = serde_json::from_str(&flash_body);
                    info!("Flash status: {}", parsed.unwrap().status);
                }
                SubAction::Ota(flash) => {
                    // Build multipart form with the file
                    let mut file = File::open(&flash.binary)?;
                    let mut buffer = Vec::new();
                    let fname: String = Path::new(&flash.binary)
                        .file_name()
                        .map(|s| s.to_string_lossy().into_owned()).expect("Invalid filename");
                    file.read_to_end(&mut buffer)?;
                    let part = multipart::Part::bytes(buffer.clone())
                        .file_name(fname.clone())
                        .mime_str("application/octet-stream")?;
                    let form = multipart::Form::new().part("file", part);

                    let rest_client = Client::new();

                    // Verify staged binary differs
                    info!("Verifying {} with the agent...", &fname);
                    let mut sha = Sha256::new();
                    sha.update(&buffer);
                    let sha256_hex = hex::encode(sha.finalize());
                    let url = build_url(result.addresses[0], result.port, "/firmware/verify");
                    let response = rest_client
                        .post(url)
                        .query(&[("node", flash.node.clone()), ("sha", sha256_hex)])
                        .send().await?;
                    let parsed: Result<VerifyReply, _> = serde_json::from_str(&response.text().await?);

                    let mut staged = false;
                    if let Ok(parsed) = parsed {
                        if parsed.matched {
                            info!("Binary verified on ota-agent, skipping staging...");
                            staged = true;
                        }
                    }

                    // Stage
                    if !staged {
                        info!("Staging {} to the agent...", &fname);
                        let url = build_url(result.addresses[0], result.port, "/firmware/stage");
                        let response = rest_client
                            .post(url)
                            .query(&[("node", flash.node.clone())])
                            .multipart(form)
                            .send().await?;

                        let stage_body = response.text().await?;
                    }

                    // Flash
                    info!("Requesting flash of staged binary...");
                    let url_flash = build_url(result.addresses[0], result.port, "/firmware/flash");
                    let resp_flash = Client::new()
                        .post(url_flash)
                        .query(&[("node", flash.node), ("force", if flash.force { "true".into() } else { "false".into() })])
                        .send().await?;
                    let flash_body = resp_flash.text().await?;
                    let parsed: Result<FlashReply, _> = serde_json::from_str(&flash_body);
                    info!("Flash status: {}", parsed.unwrap().status);
                }
                SubAction::Batch(batch) => {
                    if batch.targets.is_empty() {
                        error!("No targets provided. Use -u node:/path/to/bin (repeatable).");
                        std::process::exit(1);
                    }

                    let overall_start = Instant::now();
                    let mut results: Vec<(String, UpdateResult)> = Vec::with_capacity(batch.targets.len());
                    let rest_client = Client::new();

                    for upd in &batch.targets {
                        let node_start = Instant::now();
                        let Some((node, bin)) = parse_update_pair(upd) else {
                            let msg = format!("Bad -u format: '{}'. Expected node:/path/to/bin", upd);
                            error!("{}", msg);
                            results.push(("<unknown>".to_string(), UpdateResult {
                                bin: PathBuf::from("<unknown>".to_string()),
                                result: FlashStatus::Failed(msg),
                                duration: Duration::from_secs(0),
                            }));
                            continue;
                        };

                        info!("OTAing binary {:?} to node '{}'", bin, node);

                        // Build multipart form with the file
                        let mut file = File::open(&bin)?;
                        let mut buffer = Vec::new();
                        let fname: String = Path::new(&bin)
                            .file_name()
                            .map(|s| s.to_string_lossy().into_owned()).expect("Invalid filename");
                        file.read_to_end(&mut buffer)?;
                        let part = multipart::Part::bytes(buffer.clone())
                            .file_name(fname.clone())
                            .mime_str("application/octet-stream")?;
                        let form = multipart::Form::new().part("file", part);

                        // Verify staged binary differs
                        info!("Verifying {} with the agent...", &fname);
                        let mut sha = Sha256::new();
                        sha.update(&buffer);
                        let sha256_hex = hex::encode(sha.finalize());
                        let url = build_url(result.addresses[0], result.port, "/firmware/verify");
                        let response = rest_client
                            .post(url)
                            .query(&[("node", node.clone()), ("sha", sha256_hex)])
                            .send().await?;
                        let parsed: Result<VerifyReply, _> = serde_json::from_str(&response.text().await?);

                        let mut staged = false;
                        if let Ok(parsed) = parsed {
                            if parsed.matched {
                                info!("Binary verified on ota-agent, skipping staging...");
                                staged = true;
                            }
                        }

                        if !staged {
                            // Stage
                            info!("Staging {} to the agent...", &fname);
                            let url = build_url(result.addresses[0], result.port, "/firmware/stage");
                            let response = rest_client
                                .post(url)
                                .query(&[("node", node.clone())])
                                .multipart(form)
                                .send().await?;

                            let stage_body = response.text().await?;
                        }

                        // Flash
                        info!("Requesting flash of staged binary...");
                        let url_flash = build_url(result.addresses[0], result.port, "/firmware/flash");
                        let resp_flash = Client::new()
                            .post(url_flash)
                            .query(&[("node", node.clone()), ("force", if batch.force { "true".into() } else { "false".into() })])
                            .send().await?;

                        // Try to parse JSON regardless of status code
                        let status = resp_flash.status();
                        let flash_body = resp_flash.text().await?;

                        // Parse into FlashReply if possible; if not, mark as failure with the raw body
                        let parsed: Result<FlashReply, _> = serde_json::from_str(&flash_body);

                        let (final_status, bin_for_report, dur_for_report) = match parsed {
                            Ok(fr) => {
                                // Map API status to our reporting enum
                                info!("Flash status: {}", fr.status);
                                let fs = match fr.status.as_str() {
                                    "failed" => {
                                        let msg = fr.error.unwrap_or_else(|| "unknown error".to_string());
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
                                    other => FlashStatus::Failed(format!("unexpected status '{}'", other)),
                                };
                                (fs, PathBuf::from(fr.bin), Duration::from_millis(fr.duration_ms as u64))
                            }
                            Err(e) => {
                                // Non-JSON or unexpected shape
                                let msg = format!("unparseable flash reply: {} | body: {}", e, flash_body);
                                (FlashStatus::Failed(msg), PathBuf::from("<unknown>"), Duration::from_secs(0))
                            }
                        };

                        let is_failed = matches!(final_status, FlashStatus::Failed(_));
                        let duration = if dur_for_report.is_zero() { node_start.elapsed() } else { dur_for_report };

                        // Record the node result using what we parsed
                        results.push((
                            node.clone(),
                            UpdateResult {
                                bin: bin_for_report,
                                result: final_status,
                                duration: duration,
                            },
                        ));
                        thread::sleep(Duration::from_secs(1));
                    }

                    let total_dur = overall_start.elapsed();
                    print_deployment_report(&results, total_dur);
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

#[cfg(target_os = "linux")]
// POST /firmware/stage?node=...   (multipart form with part name "file")
async fn stage_handler(
    p: FlashParams,
    state: Arc<AppState>,
    form: warp::multipart::FormData,
) -> Result<impl warp::Reply, warp::Rejection> {
    let uds_node = state
        .cfg
        .nodes
        .get(&p.node)
        .ok_or_else(|| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, format!("unknown node '{}'", p.node))))?;
    let _ = uds_node; // only verifying existence here

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

        let fname = part
            .filename()
            .map(|s| s.to_string())
            .ok_or_else(|| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, "missing filename".into())))?;
        let safe = sanitize_filename(&fname);
        let full = state.save_dir.join(&safe);

        let mut f = fs::File::create(&full)
            .await
            .map_err(|e| warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string())))?;

        let mut stream = part.stream();
        while let Some(chunk) = stream
            .try_next()
            .await
            .map_err(|e| warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string())))?
        {
            // `chunk` implements `bytes::Buf`
            let bytes = chunk.chunk();
            sha.update(bytes);
            f.write_all(bytes)
                .await
                .map_err(|e| warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string())))?;
            total += bytes.len() as u64;
        }
        f.flush()
            .await
            .map_err(|e| warp::reject::custom(HttpError(StatusCode::INTERNAL_SERVER_ERROR, e.to_string())))?;

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

    {
        let _guard = state.manifest_lock.lock().await;
        if let Err(e) = update_manifest_stage(
            &state.manifest_path,
            &sha256_hex,
            &filename,
            &bin_path,
            total,
            &p.node,
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
// POST /firmware/stage?node=...
async fn verify_handler(
    p: FlashParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    let uds_node = state
        .cfg
        .nodes
        .get(&p.node)
        .ok_or_else(|| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, format!("unknown node '{}'", p.node))))?;

    let mut manifest = match read_manifest_compat(&state.manifest_path).await {
        Ok(m) => m,
        Err(e) => {
            return Err(warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                format!("failed to read manifest: {}", e),
            )))
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
            });
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
    Ok(warp::reply::with_status(
        warp::reply::json(&body),
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
        return UpdateResult{
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
    if !manifest.nodes.get_mut(node).expect("Invalid manifest entry").flashing {
        manifest.nodes.get_mut(node).expect("Invalid manifest entry").flashing = true;
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
    if manifest.nodes.get_mut(node).expect("Invalid manifest entry").flashing {
        manifest.nodes.get_mut(node).expect("Invalid manifest entry").flashing = false;
        return Ok(write_manifest(manifest_path, &manifest).await?);
    } else {
        return Err(anyhow!("Node {} is not being flashed", node));
    }
}

#[cfg(target_os = "linux")]
async fn systemd_service(action: &str, unit: &str) -> anyhow::Result<()> {
    use tokio::process::Command;
    let status = Command::new("systemctl").arg(action).arg(unit).status().await?;
    if status.success() {
        Ok(())
    } else {
        Err(anyhow::anyhow!("systemctl {} {} exited with {:?}", action, unit, status))
    }
}

#[cfg(target_os = "linux")]
// POST /firmware/flash?node=...
async fn flash_handler(
    p: FlashParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    let uds_node = state
        .cfg
        .nodes
        .get(&p.node)
        .ok_or_else(|| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, format!("unknown node '{}'", p.node))))?;

    let entry = {
        let _guard = state.manifest_lock.lock().await; // just to serialize reads/writes
        match read_manifest_compat(&state.manifest_path).await {
            Ok(m) => m.nodes.get(&p.node).and_then(|nb| nb.staged.clone()),
            Err(e) => {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::INTERNAL_SERVER_ERROR,
                    format!("failed to read manifest: {}", e),
                )))
            }
        }
    };

    let staged = entry.ok_or_else(|| warp::reject::custom(HttpError(
        StatusCode::BAD_REQUEST,
        ManifestError::NoStaged(p.node.clone()).to_string(),
    )))?;

    info!(
        "Starting flash on device '{}' with node '{}' (file: {})",
        &state.can_device, &p.node, &staged.filename
    );

    let bridge_unit = "can-bridge.service";
    if let Err(e) = systemd_service("stop", bridge_unit).await {
        error!("Failed to stop {}: {}", bridge_unit, e);
    }
    let processor_unit = "log-processor.service";
    if let Err(e) = systemd_service("stop", processor_unit).await {
        error!("Failed to stop {}: {}", processor_unit, e);
    }
    println!("{:?}", p);
    let result = flash_node(
        &state.can_device,
        &Path::new(&staged.path),
        &p.node,
        uds_node.request_id,
        uds_node.response_id,
        &state.manifest_path,
        &state.manifest_lock,
        if let Some(force) = p.force { force } else { false },
    ).await;
    if let Err(e) = systemd_service("start", bridge_unit).await {
        error!("Failed to start {}: {}", bridge_unit, e);
    }
    if let Err(e) = systemd_service("start", processor_unit).await {
        error!("Failed to stop {}: {}", processor_unit, e);
    }

    let status_str = match &result.result {
        FlashStatus::DownloadSuccess => "download_success",
        FlashStatus::CrcMatch        => "crc_match",
        FlashStatus::Failed(_)       => "failed",
        _                            => "unknown",
    }.to_string();

    let make_body = |error: Option<String>| {
        let duration_ms = result.duration.as_millis();
        serde_json::json!(FlashReply {
            node: p.node.clone(),
            filename: staged.filename.clone(),
            bytes: staged.size,
            sha256: staged.hash.clone(),
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
            Ok(warp::reply::with_status(warp::reply::json(&body), StatusCode::INTERNAL_SERVER_ERROR))
        }
        _ => {
            info!("Flash result: {:?}", result);
            let body = make_body(None);
            Ok(warp::reply::with_status(warp::reply::json(&body), StatusCode::OK))
        }
    }
}

#[cfg(target_os = "linux")]
// POST /firmware/promote?node=...
async fn promote_handler(
    p: FlashParams,
    state: Arc<AppState>,
) -> Result<impl warp::Reply, warp::Rejection> {
    let _ = state
        .cfg
        .nodes
        .get(&p.node)
        .ok_or_else(|| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, format!("unknown node '{}'", p.node))))?;

    {
        let _guard = state.manifest_lock.lock().await;
        if let Err(e) = promote_staged_to_production(&state.manifest_path, &p.node).await {
            if let Some(ManifestError::NoStaged(_)) = e.downcast_ref::<ManifestError>() {
                return Err(warp::reject::custom(HttpError(
                    StatusCode::BAD_REQUEST,
                    e.to_string(),
                )));
            }
            return Err(warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                e.to_string(),
            )));
        }
    }

    let body = serde_json::json!({
        "status": "promoted",
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
        updated_at: Utc::now().to_rfc3339(),
    };

    manifest
        .nodes
        .entry(node.to_string())
        .and_modify(|nb| nb.staged = Some(entry.clone()))
        .or_insert_with(|| NodeBinaries { flashing: false, staged: Some(entry), production: None });

    write_manifest(manifest_path, &manifest).await
}

#[cfg(target_os = "linux")]
async fn promote_staged_to_production(manifest_path: &Path, node: &str) -> anyhow::Result<()> {
    let mut manifest = read_manifest_compat(manifest_path).await?;
    let nb = manifest.nodes.get_mut(node).ok_or_else(|| anyhow::anyhow!(ManifestError::NoStaged(node.to_string())))?;
    let staged = nb.staged.clone().ok_or_else(|| anyhow::anyhow!(ManifestError::NoStaged(node.to_string())))?;
    nb.production = Some(staged);
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
fn start_mdns_advertisement(interface: String, service_type: String, ip: IpAddr, port: u16) {
    let instance_name = get_hostname()
        .ok()
        .and_then(|os| os.into_string().ok())
        .unwrap_or_else(|| "ota-agent".to_string());
    let host_name = format!("{}.local.", instance_name);

    let mut txt: HashMap<String, String> = HashMap::new();
    txt.insert("api".to_string(), "/firmware".to_string());
    txt.insert("proto".to_string(), "http".to_string());

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
        .map(|c| if c.is_ascii_alphanumeric() || "-._".contains(c) { c } else { '_' })
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
async fn recover_json(
    err: warp::Rejection,
) -> Result<impl warp::Reply, std::convert::Infallible> {
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
            warp::reply::json(&serde_json::json!({ "error": "unsupported media type (expect multipart/form-data)" })),
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
    let successes = results.iter().filter(|(_node, r)| !matches!(r.result, FlashStatus::Failed(_))).count() as u64;
    let failures = total - successes;
    let avg = average_duration(&results.iter().map(|(_node, r)| r.duration).collect::<Vec<_>>());
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
    info!("{:<18}  {:<28}  {:<10}  {}", "Node", "Binary", "Elapsed", "Status");
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
        for (node, r) in results.iter().filter(|(_node, r)| matches!(r.result, FlashStatus::Failed(_))) {
            info!("node='{}' bin='{}' error='{:?}'",
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
