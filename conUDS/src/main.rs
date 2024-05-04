use core::time;
// use futures::future::BoxFuture;
use std::{sync::Arc, sync::Mutex};
use std::io::stdin;

use anyhow::Result;
use automotive_diag::uds::{UdsCommand, UdsError, UdsErrorByte, RoutineControlType};
use ecu_diagnostics::dynamic_diag::{DiagProtocol, DiagPayload, EcuNRC};
use ecu_diagnostics::uds::UDSProtocol;
use tokio::sync::mpsc;

use conuds::{CanioCmd, PrdCmd, UdsDownloadStart, start_routine_frame, start_download_frame, DownloadStartResponse, transfer_data_frame, stop_download_frame};
use conuds::modules::canio::CANIO;

struct App {
    exit: bool,
}

impl App {
    fn new() -> Self {
        Self { exit: false }
    }
}

#[tokio::main]
async fn main() {
    println!("startup");

    let app = Arc::new(Mutex::new(App::new()));
    let app_canio = Arc::clone(&app);
    let app_10ms = Arc::clone(&app);

    let (uds_queue_tx, uds_queue_rx) = mpsc::channel::<CanioCmd>(100);
    let uds_queue_tx2 = uds_queue_tx.clone();
    let (cmd_queue_tx, mut cmd_queue_rx) = mpsc::channel::<PrdCmd>(10);

    let t1 = tokio::spawn(async move { tsk_canio(app_canio, uds_queue_rx).await });
    let t2 = tokio::spawn(async move { tsk_10ms(app_10ms, &mut cmd_queue_rx, uds_queue_tx2).await });

    do_uds_things(cmd_queue_tx.clone(), uds_queue_tx.clone()).await;


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
}

async fn do_uds_things(cmd_queue_tx: mpsc::Sender<PrdCmd>, uds_queue_tx: mpsc::Sender<CanioCmd>) {
    let _ = cmd_queue_tx
        .send(PrdCmd::PersistentTesterPresent(true))
        .await;

    let mut garbage = String::new();
    while !stdin().read_line(&mut garbage).is_ok() {
        // wait for user to hit enter
    }

    // start routine 0xf00f
    let buf = start_routine_frame(0xf00f, None);
    println!("Starting routine 0xf00f: {:02x?}", buf);

    if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone()).await {
        if let Ok(resp) = resp.await {
            println!("response: {:02x?}", resp);
        }
    }

    while !stdin().read_line(&mut garbage).is_ok() {
        // wait for user to hit enter
    }

    // start download
    let mut started = false;
    let mut deets: DownloadStartResponse;
    let buf = start_download_frame(UdsDownloadStart::default(0x08002000, 4));
    println!("Starting UDS download: {:02x?}", buf);

    if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone()).await {
        if let Ok(resp) = resp.await {
            println!("response: {:02x?}", resp);

            if resp[0] == 0x7F {
                let nrc = UdsErrorByte::from(*resp.last().unwrap());
                if nrc.is_ecu_busy() {
                    print!("ECU is processing the command");
                } else if nrc.is_repeat_request() {
                    print!("ECU requests retransmission");
                } else {
                    print!("Error: {}", nrc.desc());
                }
            } else {
                deets = DownloadStartResponse {
                    chunksize_len: resp[1],
                    chunksize: resp[2] as u16,
                };
                started = true;
            }
        }
    }

    // actually transfer some data
    while !stdin().read_line(&mut garbage).is_ok() {
        // wait for user to hit enter
    }
    if started {
        let buf = transfer_data_frame();
        println!("Transferring data over UDS: {:02x?}", buf);

        if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone()).await {
            if let Ok(resp) = resp.await {
                println!("response: {:02x?}", resp);
            }
        }

        if let Ok(resp) = CanioCmd::send_recv(&stop_download_frame(), uds_queue_tx.clone()).await {
            if let Ok(resp) = resp.await {
                println!("response: {:02x?}", resp);
            }
        }
    }
}

/// CANIO task
///
/// Will handle converting between UDS commands and CAN frames, and interfacing with the hardware
async fn tsk_canio(app: Arc<Mutex<App>>, mut uds_queue: mpsc::Receiver<CanioCmd>) -> Result<()> {
    // init
    let mut canio = CANIO::new(0x456, 0x123, &mut uds_queue)?;

    // loop
    while !app.try_lock().is_ok_and(|app| app.exit) {
        canio.process().await?;
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
            // println!("sending tp to channel");
            uds_queue
                .clone()
                .send(CanioCmd::UdsCmdNoResponse(
                    UDSProtocol::create_tp_msg(false).to_bytes(),
                ))
                .await?;
        }

        tokio::time::sleep(time::Duration::from_millis(10)).await;
    }
    Ok(())
}
