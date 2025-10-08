use std::fs::File;
use std::io::Seek;
use std::io::SeekFrom;
use std::io::Read;
use std::error::Error;
use std::path::PathBuf;
use std::time::{Duration, Instant};

use log::{debug, error, info};
use simplelog::{CombinedLogger, TermLogger, WriteLogger};

use conUDS::SupportedResetTypes;
use conUDS::arguments::{ArgSubCommands, Arguments};
use conUDS::config::Config;
use conUDS::modules::uds::UdsSession;

const UDS_DID_CRC: u16 = 0x03;

#[derive(Debug)]
struct NodeUpdateResult {
    node: String,
    bin: PathBuf,
    success: bool,
    error: Option<String>,
    duration: Duration,
}

#[tokio::main]
async fn main() {
    // initialize logging
    CombinedLogger::init(vec![
        TermLogger::new(
            simplelog::LevelFilter::Info,
            simplelog::ConfigBuilder::new()
                .set_max_level(log::LevelFilter::Debug)
                .set_time_level(log::LevelFilter::Debug)
                .build(),
            simplelog::TerminalMode::Mixed,
            simplelog::ColorChoice::Auto,
        ),
        WriteLogger::new(
            simplelog::LevelFilter::Debug,
            simplelog::ConfigBuilder::new()
                .set_target_level(log::LevelFilter::Info)
                .set_thread_mode(simplelog::ThreadLogMode::Names)
                .build(),
            File::create("conUDS.log").unwrap(),
        ),
    ]).unwrap_or_else(|e| {
        error!("Initialization error: {:?}", e);
        std::process::exit(1)
    });

    debug!("App Startup");

    let args: Arguments = argh::from_env();
    debug!("Command-line arguments processed: {:#?}", args);

    let cfg = Config::new(&args.node_manifest).unwrap_or_else(|e| {
        error!("Configuration error: {:?}", e);
        std::process::exit(1)
    });
    debug!("Configuration initialized: {:#?}", cfg);

    match args.subcmd {
        ArgSubCommands::Batch(batch) => {
            if batch.targets.is_empty() {
                error!("No targets provided. Use -u node:/path/to/bin (repeatable).");
                std::process::exit(1);
            }

            let overall_start = Instant::now();
            let mut results: Vec<NodeUpdateResult> = Vec::with_capacity(batch.targets.len());

            for upd in &batch.targets {
                let Some((node, bin)) = parse_update_pair(upd) else {
                    let msg = format!("Bad -u format: '{}'. Expected node:/path/to/bin", upd);
                    error!("{}", msg);
                    results.push(NodeUpdateResult {
                        node: "<unknown>".into(),
                        bin: PathBuf::from("<unknown>"),
                        success: false,
                        error: Some(msg),
                        duration: Duration::from_secs(0),
                    });
                    continue;
                };

                let Some(uds_node) = cfg.nodes.get(&node) else {
                    let msg = format!("UDS node '{}' not defined in manifest", node);
                    error!("{}", msg);
                    results.push(NodeUpdateResult {
                        node: node.clone(),
                        bin: bin.clone(),
                        success: false,
                        error: Some(msg),
                        duration: Duration::from_secs(0),
                    });
                    continue;
                };

                info!("Batch update: node='{}', binary='{:?}'", node, bin);

                let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                    error!("UDS node '{}' not defined", node);
                    std::process::exit(1)
                });

                let node_start = Instant::now();
                let mut success = false;
                let mut err_msg: Option<String> = None;

                let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, false).await;
                info!("Downloading binary {:?} to node '{}'", bin, node);

                match download_app_to_target(&bin, &mut uds, false).await {
                    Ok(_) => {
                        info!("Download successful for node '{}'...", node);
                        success = true;
                    }
                    Err(e) => {
                        error!("Download failed for node '{}': {}", node, e);
                        err_msg = Some(e.to_string());
                    }
                }

                uds.teardown().await;

                let node_dur = node_start.elapsed();
                results.push(NodeUpdateResult {
                    node: node.clone(),
                    bin: bin.clone(),
                    success,
                    error: err_msg,
                    duration: node_dur,
                });
            }

            let total_dur = overall_start.elapsed();
            print_deployment_report(&results, total_dur);
        }

        ArgSubCommands::Reset(reset) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'reset'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, true).await;
            info!("Performing {:?} reset for node '{}'", reset.reset_type, node);
            if let Err(e) = uds.reset_node(reset.reset_type).await {
                error!("Error while resetting ecu: {}", e);
            }
            uds.teardown().await;
        }

        ArgSubCommands::Download(dl) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'download'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, !dl.no_skip).await;
            info!("Downloading binary {:?} to node '{}'", dl.binary, node);

            if let Err(e) = download_app_to_target(&dl.binary, &mut uds, dl.no_skip).await {
                error!("Download failed for node '{}': {}", node, e);
            } else {
                info!("Download successful for node '{}'...", node);
            }
            uds.teardown().await;
        }

        ArgSubCommands::BootloaderDownload(dl) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'bootloader-download'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, true).await;
            info!("Downloading bootloader {:?} to node '{}'", dl.binary, node);
            if let Err(e) = uds.file_download(&dl.binary, 0x08000000).await {
                error!("While downloading bootloader: {}", e);
            }
            uds.teardown().await;
        }

        ArgSubCommands::ReadDID(did) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'readDID'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, true).await;
            info!("Performing DID read on id {:#?} for node '{}'", did.id, node);
            let id = u16::from_str_radix(&did.id, 16).unwrap();
            let _ = uds.client.did_read(id).await;
            uds.teardown().await;
        }

        ArgSubCommands::NVMHardReset(_) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'nvmHardReset'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, true).await;
            info!("Performing NVM hard reset for node '{}'", node);
            if let Err(e) = uds.client.routine_start(0xf0f0, None).await {
                error!("While starting NVM erase: {}", e);
            } else {
                let _result = uds.client.routine_get_results(0xf0f0).await;
                // TODO: Implement error checking
                info!("Successful NVM erase");
            }
            uds.teardown().await;
        }
    }
}

async fn verify_app_crc(uds_session: &mut UdsSession) -> Result<u32, String> {
    let resp = uds_session.client.did_read(UDS_DID_CRC).await;
    if let Ok(node_crc) = resp {
        if let Ok(arr) = <[u8; 4]>::try_from(node_crc.as_slice()) {
            return Ok(u32::from_le_bytes(arr));
        } else {
            return Err(format!("Invalid response length for CRC"));
        }
    }

    return Err("CRC read failure".to_string());
}

async fn download_app_to_target(
        binary_path: &PathBuf,
        uds_session: &mut UdsSession,
        no_skip: bool
    ) -> Result<(), String> {
    if !no_skip {
        let mut file = File::open(binary_path).expect("Binary does not exist!");
        file.seek(SeekFrom::End(-4)).expect("Seek failed");
        let mut buffer = [0u8; 4];
        file.read_exact(&mut buffer).expect("CRC read failed");
        let app_crc = u32::from_le_bytes(buffer);
        println!("Application CRC to download: 0x{:08X}", app_crc);

        match verify_app_crc(uds_session).await {
            Ok(crc) => {
                if crc == app_crc {
                    info!("Application CRC match, skipping download.");
                    return Ok(());
                } else {
                    info!(
                        "CRC mismatch: node=0x{:08X}, app=0x{:08X}. Downloading...",
                        crc, app_crc
                    );
                }
            },
            Err(e) => error!("{}", e),
        }
    }

    uds_session.client.start_persistent_tp().await;
    let _ = uds_session.reset_node(SupportedResetTypes::Hard).await;
    match uds_session.file_download(&binary_path, 0x08002000).await {
        Ok(()) => return Ok(()),
        Err(e) => return Err(format!("Error downloading binary: '{}'", e)),
    }
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

fn print_deployment_report(results: &[NodeUpdateResult], total_dur: Duration) {
    let total = results.len() as u64;
    let successes = results.iter().filter(|r| r.success).count() as u64;
    let failures = total - successes;
    let avg = average_duration(&results.iter().map(|r| r.duration).collect::<Vec<_>>());
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
    info!("{:<18}  {:<28}  {:<10}  {:>10}  {}", "Node", "Binary", "Status", "Elapsed", "Error");
    info!("{}", "-".repeat(18 + 2 + 28 + 2 + 10 + 2 + 10 + 2 + 5 + 16));

    for r in results {
        let status = if r.success { "OK" } else { "FAIL" };
        let err = r.error.as_deref().unwrap_or("");
        let bin_str = r.bin.to_string_lossy();
        info!(
            "{:<18}  {:<28}  {:<10}  {:>10}  {}",
            r.node,
            truncate(&bin_str, 28),
            status,
            fmt_dur(r.duration),
            err
        );
    }

    if failures > 0 {
        info!("");
        info!("--- Failure details ---");
        for r in results.iter().filter(|r| !r.success) {
            info!("node='{}' bin='{}' error='{}'",
                r.node,
                r.bin.to_string_lossy(),
                r.error.as_deref().unwrap_or("<unknown error>")
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
