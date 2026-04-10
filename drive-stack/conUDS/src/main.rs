use std::fs::File;
use std::path::PathBuf;
use std::time::{Duration, Instant};

use log::{debug, error, info};
use simplelog::{CombinedLogger, TermLogger, WriteLogger};

use conUDS::SupportedResetTypes;
use conUDS::arguments::{ArgSubCommands, Arguments};
use conUDS::config::Config;
use conUDS::modules::uds::{
    CurrentDiagnosticSession, DiagnosticSessionResponse, RoutineStartResponse, UdsSession,
};
use conUDS::{FlashStatus, UpdateResult};

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
    ])
    .unwrap_or_else(|e| {
        error!("Initialization error: {:?}", e);
        std::process::exit(1)
    });

    debug!("App Startup");

    let args: Arguments = argh::from_env();
    debug!("Command-line arguments processed: {:#?}", args);

    let cfg = Config::new(&args.node_manifest, &args.routine_manifest).unwrap_or_else(|e| {
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
            let mut results: Vec<(String, UpdateResult)> = Vec::with_capacity(batch.targets.len());

            for upd in &batch.targets {
                let Some((node, bin)) = parse_update_pair(upd) else {
                    let msg = format!("Bad -u format: '{}'. Expected node:/path/to/bin", upd);
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

                let Some(uds_node) = cfg.nodes.get(&node) else {
                    let msg = format!("UDS node '{}' not defined in manifest", node);
                    error!("{}", msg);
                    results.push((
                        node.clone(),
                        UpdateResult {
                            bin: bin.clone(),
                            result: FlashStatus::Failed(msg),
                            duration: Duration::from_secs(0),
                        },
                    ));
                    continue;
                };

                info!("Downloading binary {:?} to node '{}'", bin, node);

                let mut uds = UdsSession::new(
                    &args.device,
                    uds_node.request_id,
                    uds_node.response_id,
                    false,
                )
                .await;
                let result = uds.download_app_to_target(&bin, true).await;
                uds.teardown().await;

                results.push((node.clone(), result));
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

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;
            info!(
                "Performing {:?} reset for node '{}'",
                reset.reset_type, node
            );
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

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                dl.no_skip,
            )
            .await;
            info!("Downloading binary {:?} to node '{}'", dl.binary, node);

            if let Err(e) = uds.reset_node(SupportedResetTypes::Hard).await {
                error!("Error while resetting ecu: {}", e);
                if !dl.no_skip {
                    return;
                }
            }
            if let FlashStatus::Failed(e) = uds
                .download_app_to_target(&dl.binary, !dl.no_skip)
                .await
                .result
            {
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

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;
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

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;
            info!(
                "Performing DID read on id {:#?} for node '{}'",
                did.id, node
            );
            let id = u16::from_str_radix(&did.id, 16).unwrap();
            let resp = uds.client.did_read(id).await;
            uds.teardown().await;
            println!("{:?}", resp);
        }

        ArgSubCommands::ReadSession(_) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'read-session'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;
            match uds.read_current_session().await {
                Ok(session) => println!("{}", fmt_current_session(session)),
                Err(e) => error!(
                    "Failed to read diagnostic session for node '{}': {}",
                    node, e
                ),
            }
            uds.teardown().await;
        }

        ArgSubCommands::SetSession(session) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'set-session'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let target_session = session.session.into();
            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;
            match uds.client.enter_diagnostic_session(target_session).await {
                Ok(resp) => println!("{}", fmt_diagnostic_session_response(&resp)),
                Err(e) => error!(
                    "Failed to change diagnostic session for node '{}' to '{}': {}",
                    node,
                    session.session.key(),
                    e
                ),
            }
            uds.teardown().await;
        }

        ArgSubCommands::PersistentTesterPresent(_) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'persistent-tester-present'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;

            if let Err(e) = uds.start_persistent_tp().await {
                error!(
                    "Failed to start persistent tester present for node '{}': {}",
                    node, e
                );
                uds.teardown().await;
                std::process::exit(1);
            }

            println!(
                "Persistent tester present active for node '{}' on '{}'. Press Ctrl+C to stop.",
                node, args.device
            );

            if let Err(e) = tokio::signal::ctrl_c().await {
                error!("Failed while waiting for Ctrl+C: {}", e);
            }

            if let Err(e) = uds.stop_persistent_tp().await {
                error!(
                    "Failed to stop persistent tester present for node '{}': {}",
                    node, e
                );
            }
            uds.teardown().await;
        }

        ArgSubCommands::RoutineStart(routine) => {
            let node = args.node.clone().unwrap_or_else(|| {
                error!("-n <node> is required for 'routineStart'.");
                std::process::exit(1)
            });
            let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                error!("UDS node '{}' not defined", node);
                std::process::exit(1)
            });

            let Some(routine_id) = routine_id_for_node(uds_node, &routine.routine) else {
                let available = fmt_available_routines(uds_node);
                error!(
                    "Routine '{}' not defined for node '{}'. Available: {}",
                    routine.routine, node, available
                );
                std::process::exit(1)
            };

            let mut uds = UdsSession::new(
                &args.device,
                uds_node.request_id,
                uds_node.response_id,
                true,
            )
            .await;
            info!(
                "Starting routine '{}' (0x{:04X}) for node '{}'",
                routine.routine, routine_id, node
            );
            match uds.client.routine_start(routine_id, None).await {
                Ok(resp) => println!("{}", fmt_routine_start_response(&resp)),
                Err(e) => error!("While starting routine '{}': {}", routine.routine, e),
            }
            uds.teardown().await;
        }

        ArgSubCommands::RoutineList(_) => {
            if let Some(node) = args.node.clone() {
                let uds_node = cfg.nodes.get(&node).unwrap_or_else(|| {
                    error!("UDS node '{}' not defined", node);
                    std::process::exit(1)
                });

                let mut routines: Vec<_> = uds_node.routines.iter().collect();
                routines.sort_by_key(|(name, _cfg)| *name);
                if routines.is_empty() {
                    println!("No routines configured for node '{}'", node);
                    return;
                }

                println!("Routines for node '{}':", node);
                for (name, cfg) in routines {
                    println!("  {}: 0x{:04X}", name, cfg.id);
                }
            } else {
                let mut nodes: Vec<_> = cfg.nodes.iter().collect();
                nodes.sort_by_key(|(name, _node)| *name);
                let mut any = false;
                for (node_name, uds_node) in nodes {
                    if uds_node.routines.is_empty() {
                        continue;
                    }
                    any = true;
                    let mut routines: Vec<_> = uds_node.routines.iter().collect();
                    routines.sort_by_key(|(name, _cfg)| *name);
                    println!("Routines for node '{}':", node_name);
                    for (name, cfg) in routines {
                        println!("  {}: 0x{:04X}", name, cfg.id);
                    }
                }
                if !any {
                    println!("No routines configured for any node");
                }
            }
        }
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

fn fmt_routine_start_response(resp: &RoutineStartResponse) -> String {
    match resp {
        RoutineStartResponse::Positive { payload, text, .. } => {
            let text = text
                .as_ref()
                .map(|text| format!(" text=\"{text}\""))
                .unwrap_or_default();
            let data = fmt_payload(payload);
            format!("Routine start positive response:{text}{data}")
        }
        RoutineStartResponse::Negative {
            nrc,
            description,
            payload,
            text,
            ..
        } => {
            let text = text
                .as_ref()
                .map(|text| format!(" text=\"{text}\""))
                .unwrap_or_default();
            let data = fmt_payload(payload);
            format!("Routine start negative response: NRC=0x{nrc:02X} ({description}){text}{data}")
        }
    }
}

fn fmt_current_session(session: CurrentDiagnosticSession) -> String {
    match session {
        CurrentDiagnosticSession::Unknown(raw) => {
            format!("Current diagnostic session: unknown (0x{raw:02X})")
        }
        _ => format!(
            "Current diagnostic session: {} ({}, 0x{:02X})",
            session.label(),
            session.key(),
            session.raw_value()
        ),
    }
}

fn fmt_diagnostic_session_response(resp: &DiagnosticSessionResponse) -> String {
    match resp {
        DiagnosticSessionResponse::Positive {
            session,
            payload,
            raw,
        } => format!(
            "Diagnostic session changed to {:?} raw=[{}]{}",
            session,
            fmt_hex(raw),
            fmt_payload(payload)
        ),
        DiagnosticSessionResponse::Negative {
            session,
            nrc,
            description,
            payload,
            raw,
        } => format!(
            "Diagnostic session change to {:?} rejected: NRC=0x{nrc:02X} ({description}) raw=[{}]{}",
            session,
            fmt_hex(raw),
            fmt_payload(payload)
        ),
    }
}

fn fmt_payload(payload: &[u8]) -> String {
    if payload.is_empty() {
        String::new()
    } else {
        format!(" data=[{}]", fmt_hex(payload))
    }
}

fn fmt_hex(payload: &[u8]) -> String {
    payload
        .iter()
        .map(|byte| format!("{byte:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

fn routine_id_for_node(uds_node: &conUDS::config::Node, routine: &str) -> Option<u16> {
    uds_node.routines.get(routine).map(|cfg| cfg.id)
}

fn fmt_available_routines(uds_node: &conUDS::config::Node) -> String {
    if uds_node.routines.is_empty() {
        "<none>".to_string()
    } else {
        let mut names: Vec<_> = uds_node.routines.keys().cloned().collect();
        names.sort();
        names.join(", ")
    }
}
