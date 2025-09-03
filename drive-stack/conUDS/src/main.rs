use core::time;
use std::fs::File;
use std::io::stdin;
use std::sync::Arc;
use std::sync::Mutex;

use anyhow::anyhow;
use anyhow::Result;
use conUDS::config::Config;
use conUDS::modules::uds::UdsClient;
use ecu_diagnostics::dynamic_diag::DiagProtocol;
use ecu_diagnostics::uds::UDSProtocol;
use log::{debug, error, info};
use simplelog::{CombinedLogger, TermLogger, WriteLogger};
use tokio::sync::mpsc;

use conUDS::SupportedResetTypes;
use conUDS::arguments::{ArgSubCommands, Arguments};
use conUDS::modules::canio::CANIO;
use conUDS::{CanioCmd, PrdCmd};

struct App {
    exit: bool,
}

impl App {
    fn new() -> Self {
        Self { exit: false }
    }
}

#[tokio::main]
async fn main() -> Result<()> {
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
    ])?;

    debug!("App Startup");

    let args: Arguments = argh::from_env();
    debug!("Command-line arguments processed: {:#?}", args);

    let cfg = Config::new(&args.node_manifest)?;
    debug!("Configuration initialized: {:#?}", cfg);

    let uds_node = cfg.nodes.get(&args.node).unwrap_or_else(|| {
        error!("UDS node not defined in ``");
        std::process::exit(1)
    });
    debug!("UDS node identified: {:#?}", uds_node);

    let app = Arc::new(Mutex::new(App::new()));
    let app_canio = Arc::clone(&app);
    let app_10ms = Arc::clone(&app);

    let (uds_queue_tx, uds_queue_rx) = mpsc::channel::<CanioCmd>(100);
    let (cmd_queue_tx, mut cmd_queue_rx) = mpsc::channel::<PrdCmd>(10);

    let mut uds_client = UdsClient::new(cmd_queue_tx.clone(), uds_queue_tx.clone());
    debug!("UDS client initialized: {:#?}", uds_client);

    let canio = {
        match CANIO::new(
            &args.device,
            uds_node.request_id,
            uds_node.response_id,
            uds_queue_rx,
        ) {
            Ok(canio) => {
                debug!("CANIO object initialized: {}", canio);
                canio
            }
            Err(e) => {
                error!("Failed to initialize CANIO object! {:#?}", e);
                return Err(anyhow!("Error initializing CANIO object! {:#?}", e));
            }
        }
    };

    debug!("Sawning threads");
    let t1 = tokio::spawn(async move { tsk_canio(app_canio, canio).await });
    let t2 = tokio::spawn(async move { tsk_10ms(app_10ms, &mut cmd_queue_rx, uds_queue_tx).await });

    match args.subcmd {
        ArgSubCommands::Reset(reset) => {
            debug!(
                "Performing {:#?} reset for node `{}`",
                reset.reset_type, args.node
            );
            uds_client.ecu_reset(reset.reset_type).await;
        }
        ArgSubCommands::Download(dl) => {
            debug!(
                "Downloading binary at '{:#?}' to node `{}`",
                dl.binary, args.node
            );
            let _ = uds_client.ecu_reset(SupportedResetTypes::Hard).await;
            uds_client.start_persistent_tp().await?;

            info!("Waiting for the user to hit enter before continuing with download");
            let mut garbage = String::new();
            while stdin().read_line(&mut garbage).is_err() {
                // wait for user to hit enter
            }
            info!("Enter key detected, proceeding with download");

            if let Err(e) = uds_client.app_download(dl.binary, 0x08002000).await {
                error!("While downloading app: {}", e);
            }
        }
        ArgSubCommands::BootloaderDownload(dl) => {
            debug!(
                "Downloading binary at '{:#?}' to node `{}`",
                dl.binary, args.node
            );
            uds_client.start_persistent_tp().await?;

            info!("Waiting for the user to hit enter before continuing with download");
            let mut garbage = String::new();
            while stdin().read_line(&mut garbage).is_err() {
                // wait for user to hit enter
            }
            info!("Enter key detected, proceeding with download");

            if let Err(e) = uds_client.app_download(dl.binary, 0x08000000).await {
                error!("While downloading app: {}", e);
            }
        }
        ArgSubCommands::ReadDID(did) => {
            info!("Performing DID read on id {:#?} for node `{}`", did.id, args.node);
            let id = u16::from_str_radix(&did.id, 16).unwrap();
            let _ = uds_client.did_read(id).await;
        }
        ArgSubCommands::NVMHardReset(_) => {
            info!(
                "Performing NVM hard reset for node `{}`",
                args.node
            );
            if let Err(e) = uds_client.routine_start(0xf0f0, None).await {
                error!("While downloading app: {}", e);
            }
            else {
                let result = uds_client.routine_get_results(0xf0f0).await;
                /// TODO: Implement error checking
                info!("Successful NVM erase");
            }
        }
    }

    app.lock().unwrap().exit = true;

    debug!("Main app logic done, joining threads");
    let _ = t1.await.unwrap();
    let _ = t2.await.unwrap();
    debug!("Threads finished, exiting");

    // fixme: probably wait for shutdown signal here, then signal threads to exit

    // for handle in handles {
    //     let res = handle.await.unwrap().await;
    //     if !res.is_ok() {
    //         println!("{:?}", res);
    //         app.lock().unwrap().exit = true;
    //     }
    // }

    Ok(())
}

/// CANIO task
///
/// Owns the CAN hardware and handles all transmitting and receiving functions
async fn tsk_canio(app: Arc<Mutex<App>>, mut canio: CANIO<'_>) -> Result<()> {
    debug!("CANIO thread starting");

    while !app.try_lock().is_ok_and(|app| app.exit) {
        if let Err(e) = canio.process().await {
            error!("In CANIO process: {}", e);
            return Err(e);
        }
    }

    debug!("CANIO thread exiting");
    Ok(())
}

/// 10ms(100Hz) periodic task
///
/// This task will run at 100Hz in order to handle periodic actions
async fn tsk_10ms(
    app: Arc<Mutex<App>>,
    cmd_queue: &mut mpsc::Receiver<PrdCmd>,
    uds_queue: mpsc::Sender<CanioCmd>,
) -> Result<()> {
    debug!("10ms thread starting");

    let mut send_tp = false;

    while !app.try_lock().is_ok_and(|app| app.exit) {
        if let Ok(cmd) = cmd_queue.try_recv() {
            match cmd {
                PrdCmd::PersistentTesterPresent(state) => {
                    if state {
                        info!("Enabling persistent TP");
                    } else {
                        info!("Disabling persistent TP");
                    }
                    send_tp = state
                }
            }
        }

        if send_tp {
            if let Err(e) = uds_queue
                .clone()
                .send(CanioCmd::UdsCmdNoResponse(
                    UDSProtocol::create_tp_msg(false).to_bytes(),
                ))
                .await
            {
                error!(target: "10ms thread", "Failed to add to CANIO queue from 10ms task: {}", e);
                return Err(e.into());
            }
        }

        tokio::time::sleep(time::Duration::from_millis(10)).await;
    }

    debug!("10ms thread exiting");
    Ok(())
}
