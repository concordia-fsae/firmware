use std::{
    collections::HashMap,
    net::{IpAddr, Ipv4Addr, SocketAddr},
    path::{Path, PathBuf},
    io::Read,
    fs::File,
    sync::Arc,
    thread,
    time::Duration,
};

use anyhow::{bail, Context, Result};
use argh::FromArgs;
use bytes::Buf; // for chunk.chunk()
use chrono::Utc;
use futures_util::{StreamExt, TryStreamExt};
use hex;
use hostname::get as get_hostname;
use if_addrs::get_if_addrs;
use reqwest::{Client, Error, multipart};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use thiserror::Error;
use tokio::{fs, io::AsyncWriteExt};
use tracing::{info, error};
use tracing_subscriber::EnvFilter;
use warp::{http::StatusCode, Filter};

#[cfg(target_os = "linux")]
use conUDS::modules::uds::{FlashStatus, UdsSession};
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
    Flash(SubActionFlash),
}

/// Start an OTA agent server
#[derive(Debug, FromArgs)]
#[argh(subcommand, name = "flash")]
pub struct SubActionFlash {
    /// the node to flash
    #[argh(option, short = 'n')]
    pub node: String,
    /// the binary to flash
    #[argh(option, short = 'b')]
    binary: PathBuf,
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

#[derive(Clone, Debug, Deserialize)]
struct Config {
    nodes: HashMap<String, UdsNode>,
}

#[derive(Clone)]
struct AppState {
    cfg: Arc<Config>,
    can_device: String,
    save_dir: PathBuf,
    manifest_path: PathBuf,
    manifest_lock: Arc<tokio::sync::Mutex<()>>,
}

#[derive(Deserialize)]
struct FlashParams {
    node: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, Default)]
struct BinariesManifest {
    nodes: HashMap<String, BinaryEntry>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct BinaryEntry {
    filename: String,
    path: String,
    size: u64,
    hash: String,
    updated_at: String,
}

#[derive(Error, Debug, Clone)]
enum ManifestError {
    #[error("binary already associated with node '{existing}', cannot reassign to '{incoming}'")]
    NodeMismatch { existing: String, incoming: String },
    #[error("filename already associated with node '{existing}', cannot reassign to '{incoming}'")]
    FilenameError { existing: String, incoming: String },
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

            let flash_route = warp::path("update-binary")
                .and(warp::post())
                .and(warp::query::<FlashParams>())
                .and(state_filter)
                .and(warp::multipart::form().max_length(1024 * 1024 * 512)) // 512MB cap
                .and_then(update_handler)
                .recover(recover_json); // make our JSON errors show up

            warp::serve(flash_route).run(addr).await;
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
                SubAction::Flash(flash) => {
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

                    info!("Uploading {} to the agent...", &fname);
                    let rest_client = Client::new();
                    // Send POST request with query parameter
                    let response = rest_client
                        .post(format!("http://{:?}:{}/update-binary", result.addresses[0], result.port))
                        .query(&[("node", flash.node)])
                        .multipart(form)
                        .send().await?;

                    info!("Status: {}", response.status());
                    info!("Body: {}", response.text().await?);
                }
            }
        }
    }
    Ok(())
}

async fn update_handler(
    p: FlashParams,
    state: Arc<AppState>,
    form: warp::multipart::FormData,
) -> Result<impl warp::Reply, warp::Rejection> {
    let uds_node = state
        .cfg
        .nodes
        .get(&p.node)
        .ok_or_else(|| warp::reject::custom(HttpError(StatusCode::BAD_REQUEST, format!("unknown node '{}'", p.node))))?;

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
        if let Err(e) = update_binaries_manifest_exclusive(
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
            return Err(warp::reject::custom(HttpError(
                StatusCode::INTERNAL_SERVER_ERROR,
                e.to_string(),
            )));
        }
    }

    info!(
        "Starting flash on device '{}' with node '{}'",
        &state.can_device, &p.node
    );

    let mut uds =
        UdsSession::new(&state.can_device, uds_node.request_id, uds_node.response_id, false).await;
    let result = uds.download_app_to_target(&bin_path, true).await;
    match result.result {
        FlashStatus::Failed(e) => {
            error!("Flash failed: {e}");
            let body = serde_json::json!({ "error": format!("flash failed: {e}") });
            Ok(warp::reply::with_status(
                warp::reply::json(&body),
                StatusCode::INTERNAL_SERVER_ERROR,
            ))
        }
        _ => {
            info!("Flash status: {:?}", result.result);
            let body = serde_json::json!({
                "status": format!("{:?}", result.result),
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
    }
}

async fn load_manifest(path: &str) -> Result<Config> {
    let data = fs::read(path)
        .await
        .with_context(|| format!("reading {}", path))?;
    let cfg: Config = serde_yaml::from_slice(&data).with_context(|| "parsing YAML")?;
    Ok(cfg)
}

fn start_mdns_advertisement(interface: String, service_type: String, ip: IpAddr, port: u16) {
    let instance_name = get_hostname()
        .ok()
        .and_then(|os| os.into_string().ok())
        .unwrap_or_else(|| "ota-agent".to_string());
    let host_name = format!("{}.local.", instance_name);

    let mut txt: HashMap<String, String> = HashMap::new();
    txt.insert("api".to_string(), "/flash".to_string());
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

async fn update_binaries_manifest_exclusive(
    manifest_path: &Path,
    sha256_hex: &str,
    filename: &str,
    file_path: &Path,
    size: u64,
    node: &str,
) -> anyhow::Result<()> {
    let mut manifest: BinariesManifest = match fs::read(manifest_path).await {
        Ok(bytes) => serde_yaml::from_slice(&bytes).context("parsing binaries manifest")?,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => BinariesManifest::default(),
        Err(e) => return Err(e).context("reading binaries manifest")?,
    };

    for (saved_node, binary) in &manifest.nodes {
        if node != saved_node && sha256_hex == binary.hash {
            return Err(ManifestError::NodeMismatch {
                existing: saved_node.to_string(),
                incoming: node.to_string(),
            }
            .into());
        }
        if node != saved_node && filename == binary.filename {
            return Err(ManifestError::FilenameError {
                existing: saved_node.to_string(),
                incoming: node.to_string(),
            }
            .into());
        }
    }

    manifest.nodes.insert(
        node.to_string(),
        BinaryEntry {
            filename: filename.to_string(),
            path: file_path.to_string_lossy().into_owned(),
            size,
            hash: sha256_hex.to_string(),
            updated_at: Utc::now().to_rfc3339(),
        },
    );

    let tmp_path = manifest_path.with_extension("yaml.tmp");
    let yaml = serde_yaml::to_string(&manifest).context("serializing binaries manifest")?;
    fs::write(&tmp_path, yaml.as_bytes())
        .await
        .context("writing temp manifest")?;
    fs::rename(&tmp_path, manifest_path)
        .await
        .context("renaming temp manifest")?;
    Ok(())
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
    if let Some(HttpError(code, msg)) = err.find::<HttpError>() {
        Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": msg })),
            *code,
        ))
    } else {
        Ok(warp::reply::with_status(
            warp::reply::json(&serde_json::json!({ "error": "unhandled error" })),
            StatusCode::INTERNAL_SERVER_ERROR,
        ))
    }
}
