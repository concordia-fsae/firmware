use core::time;
use std::io::stdin;
use std::path::PathBuf;
use std::{sync::Arc, sync::Mutex};

use anyhow::Result;
use conuds::modules::uds::UdsClient;
use ecu_diagnostics::dynamic_diag::DiagProtocol;
use ecu_diagnostics::uds::UDSProtocol;
use tokio::sync::mpsc;

use conuds::modules::canio::CANIO;
use conuds::{CanioCmd, PrdCmd};

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
    println!("startup");

    let app = Arc::new(Mutex::new(App::new()));
    let app_canio = Arc::clone(&app);
    let app_10ms = Arc::clone(&app);

    let (uds_queue_tx, uds_queue_rx) = mpsc::channel::<CanioCmd>(100);
    let (cmd_queue_tx, mut cmd_queue_rx) = mpsc::channel::<PrdCmd>(10);
    let mut uds_client = UdsClient::new(cmd_queue_tx.clone(), uds_queue_tx.clone());

    let t1 = tokio::spawn(async move { tsk_canio(app_canio, uds_queue_rx).await });
    let t2 = tokio::spawn(async move { tsk_10ms(app_10ms, &mut cmd_queue_rx, uds_queue_tx).await });

    uds_client.start_persistent_tp().await?;

    let mut garbage = String::new();
    while stdin().read_line(&mut garbage).is_err() {
        // wait for user to hit enter
    }

    if let Err(e) = uds_client
        .app_download(PathBuf::from(
            "../../firmware/components/heartbeat/build/heartbeat_crc.bin",
        ))
        .await
    {
        println!("Error downloading app: {}", e);
    }

    let _ = t1.await.unwrap();
    let _ = t2.await.unwrap();

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
/// Will handle converting between UDS commands and CAN frames, and interfacing with the hardware
async fn tsk_canio(app: Arc<Mutex<App>>, mut uds_queue: mpsc::Receiver<CanioCmd>) -> Result<()> {
    // init
    match CANIO::new(0x456, 0x123, &mut uds_queue) {
        Ok(mut canio) => {
            // loop
            while !app.try_lock().is_ok_and(|app| app.exit) {
                if let Err(e) = canio.process().await {
                    println!("Error in CANIO process: {}", e);
                    return Err(e);
                }
            }
        }
        Err(e) => {
            println!("Error creating canio object: {}", e);
            return Err(e);
        }
    }

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
    let mut send_tp = false;

    while !app.try_lock().is_ok_and(|app| app.exit) {
        if let Ok(cmd) = cmd_queue.try_recv() {
            match cmd {
                PrdCmd::PersistentTesterPresent(state) => {
                    if state {
                        println!("Enabling persisting tp");
                    } else {
                        println!("Disabling persisting tp");
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
                println!("Error adding to CANIO queue from 10ms task: {}", e);
                return Err(e.into());
            }
        }

        tokio::time::sleep(time::Duration::from_millis(10)).await;
    }
    Ok(())
}
