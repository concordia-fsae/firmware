use std::fs::File;
use std::io::Seek;
use std::io::SeekFrom;
use std::io::Read;
use std::error::Error;
use std::path::PathBuf;

use log::{debug, error, info};
use simplelog::{CombinedLogger, TermLogger, WriteLogger};

use conUDS::SupportedResetTypes;
use conUDS::arguments::{ArgSubCommands, Arguments};
use conUDS::config::Config;
use conUDS::modules::uds::UdsSession;

const UDS_DID_CRC: u16 = 0x03;

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

            for upd in &batch.targets {
                let Some((node, bin)) = parse_update_pair(upd) else {
                    error!("Bad -u format: '{}'. Expected node:/path/to/bin", upd);
                    continue;
                };

                let Some(uds_node) = cfg.nodes.get(&node) else {
                    error!("UDS node '{}' not defined in manifest", node);
                    continue;
                };

                info!("Batch update: node='{}', binary='{:?}'", node, bin);

                let mut uds = UdsSession::new(
                    &args.device,
                    uds_node.request_id,
                    uds_node.response_id,
                    false
                ).await;

                // Optional CRC check, same behavior as single-node download
                let mut file = match File::open(&bin) {
                    Ok(f) => f,
                    Err(e) => {
                        error!("Cannot open binary {:?} for node {}: {}", bin, node, e);
                        uds.teardown().await;
                        continue;
                    }
                };
                if let Err(e) = file.seek(SeekFrom::End(-4)) {
                    error!("Seek end(-4) failed for {:?}: {}", bin, e);
                    uds.teardown().await;
                    continue;
                }
                let mut buffer = [0u8; 4];
                if let Err(e) = file.read_exact(&mut buffer) {
                    error!("Reading CRC from {:?} failed: {}", bin, e);
                    uds.teardown().await;
                    continue;
                }
                let app_crc = u32::from_le_bytes(buffer);
                info!("Application CRC ({}): 0x{:08X}", node, app_crc);

                match uds.client.did_read(UDS_DID_CRC).await {
                    Ok(node_crc_bytes) => {
                        if node_crc_bytes.len() == 4 {
                            if let Ok(arr) = <[u8; 4]>::try_from(node_crc_bytes.as_slice()) {
                                let node_crc = u32::from_le_bytes(arr);
                                if app_crc == node_crc {
                                    info!("CRC match for '{}', skipping download.", node);
                                    uds.teardown().await;
                                    continue;
                                } else {
                                    info!(
                                        "CRC mismatch for '{}': node=0x{:08X}, app=0x{:08X}. Downloading...",
                                        node, node_crc, app_crc
                                    );
                                }
                            } else {
                                error!("Invalid CRC length from node '{}'", node);
                            }
                        } else {
                            error!("Unexpected DID length from node '{}': {:?}", node, node_crc_bytes);
                        }
                    }
                    Err(e) => {
                        error!("DID read (CRC) failed on node '{}': {:?}", node, e);
                        // proceed with download anyway
                    }
                }

                uds.client.start_persistent_tp().await;
                let _ = uds.reset_node(SupportedResetTypes::Hard).await;

                if let Err(e) = uds.file_download(&bin, 0x08002000).await {
                    error!("Download failed for node '{}': {}", node, e);
                }
                else
                {
                    info!("Download successful...");
                }

                uds.teardown().await;
            }
        }

        // ===== single-node commands remain the same, but require -n =====
        ArgSubCommands::Reset(reset) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'reset' (omit only when using 'batch').");
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
                error!("-n <node> is required for 'download' (omit only when using 'batch').");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(&args.device, uds_node.request_id, uds_node.response_id, !dl.no_skip).await;
            info!("Downloading binary {:?} to node '{}'", dl.binary, node);

            if !dl.no_skip {
                let mut file = File::open(dl.binary.to_path_buf()).expect("Binary does not exist!");
                file.seek(SeekFrom::End(-4)).expect("Seek failed");
                let mut buffer = [0u8; 4];
                file.read_exact(&mut buffer).expect("CRC read failed");
                let app_crc = u32::from_le_bytes(buffer);
                println!("Application CRC to download: 0x{:08X}", app_crc);

                let resp = uds.client.did_read(UDS_DID_CRC).await;
                if let Ok(node_crc) = resp {
                    if let Ok(arr) = <[u8; 4]>::try_from(node_crc.as_slice()) {
                        if app_crc == u32::from_le_bytes(arr) {
                            info!("Application CRC match for '{}', skipping download.", node);
                            uds.teardown().await;
                            return;
                        } else {
                            info!("Node '{}' had a CRC mismatch, downloading...", node);
                        }
                    } else {
                        error!("Invalid response length for CRC on '{}'", node);
                    }
                }
            }

            uds.client.start_persistent_tp().await;
            let _ = uds.reset_node(SupportedResetTypes::Hard).await;
            if let Err(e) = uds.file_download(&dl.binary, 0x08002000).await {
                error!("While downloading app: {}", e);
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

fn parse_update_pair(s: &str) -> Option<(String, PathBuf)> {
    // expects "node:/path/to/bin"
    let (node, path) = s.split_once(':')?;
    Some((node.to_string(), PathBuf::from(path)))
}
