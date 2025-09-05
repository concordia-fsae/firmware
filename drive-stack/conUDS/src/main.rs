use std::fs::File;
use std::error::Error;

use log::{debug, error, info};
use simplelog::{CombinedLogger, TermLogger, WriteLogger};

use conUDS::SupportedResetTypes;
use conUDS::arguments::{ArgSubCommands, Arguments};
use conUDS::config::Config;
use conUDS::modules::uds::UdsSession;

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

    let uds_node = cfg.nodes.get(&args.node).unwrap_or_else(|| {
        error!("UDS node not defined");
        std::process::exit(1)
    });
    debug!("UDS node identified: {:#?}", uds_node);

    let mut uds = UdsSession::new(
        &args.device,
        uds_node.request_id,
        uds_node.response_id,
        true).await;

    match args.subcmd {
        ArgSubCommands::Reset(reset) => {
            debug!(
                "Performing {:#?} reset for node `{}`",
                reset.reset_type, args.node
            );
            if let Err(e) = uds.reset_node(reset.reset_type).await {
                error!("Error while resetting ecu: {}", e);
            }
        }
        ArgSubCommands::Download(dl) => {
            debug!(
                "Downloading binary at '{:#?}' to node `{}`",
                dl.binary, args.node
            );
            uds.reset_node(SupportedResetTypes::Hard).await;
            if let Err(e) = &uds.file_download(&dl.binary, 0x08002000).await {
                error!("While downloading app: {}", e);
            }
        }
        ArgSubCommands::BootloaderDownload(dl) => {
            debug!(
                "Downloading bootloader at '{:#?}' to node `{}`",
                dl.binary, args.node
            );
            if let Err(e) = uds.file_download(&dl.binary, 0x08000000).await {
                error!("While downloading bootloader: {}", e);
            }
        }
        ArgSubCommands::ReadDID(did) => {
            info!("Performing DID read on id {:#?} for node `{}`", did.id, args.node);
            let id = u16::from_str_radix(&did.id, 16).unwrap();
            let _ = uds.client.did_read(id).await;
        }
        ArgSubCommands::NVMHardReset(_) => {
            info!(
                "Performing NVM hard reset for node `{}`",
                args.node
            );
            if let Err(e) = uds.client.routine_start(0xf0f0, None).await {
                error!("While downloading app: {}", e);
            }
            else {
                let _result = uds.client.routine_get_results(0xf0f0).await;
                // TODO: Implement error checking
                info!("Successful NVM erase");
            }
        }
    }

    uds.teardown().await;
}
