use core::time;
use std::fs::read;
use std::io::stdin;
use std::{sync::Arc, sync::Mutex};

use anyhow::Result;
use automotive_diag::uds::{UdsCommand, UdsErrorByte};
use ecu_diagnostics::dynamic_diag::{DiagProtocol, EcuNRC};
use ecu_diagnostics::uds::UDSProtocol;
use tokio::sync::mpsc;

use conuds::modules::canio::CANIO;
use conuds::{
    get_routine_results_frame, start_download_frame, start_routine_frame, stop_download_frame,
    transfer_data_frame, CanioCmd, DownloadParams, PrdCmd, UdsDownloadStart,
};

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
    let t2 =
        tokio::spawn(async move { tsk_10ms(app_10ms, &mut cmd_queue_rx, uds_queue_tx2).await });

    let _ = cmd_queue_tx
        .send(PrdCmd::PersistentTesterPresent(true))
        .await;

    let mut garbage = String::new();
    while stdin().read_line(&mut garbage).is_err() {
        // wait for user to hit enter
    }

    uds_app_download(cmd_queue_tx.clone(), uds_queue_tx.clone()).await;

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

/// Trigger an app download process
///
/// Starts by telling the device to erase the current app, then starts the app download, and lastly
/// starts actually transferring the app
async fn uds_app_download(
    cmd_queue_tx: mpsc::Sender<PrdCmd>,
    uds_queue_tx: mpsc::Sender<CanioCmd>,
) {
    // disable tester present. bootloader is not currently robust to having tester present enabled
    // during an app download
    let _ = cmd_queue_tx
        .send(PrdCmd::PersistentTesterPresent(false))
        .await;

    // tokio::time::sleep(time::Duration::from_millis(5)).await;

    // start routine 0xf00f, erase app
    let buf = start_routine_frame(0xf00f, None);
    println!("Starting routine 0xf00f: {:02x?}", buf);

    if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone(), 20).await {
        if let Ok(resp) = resp.await {
            println!("response: {:02x?}", resp);
        }
    }

    // give it some time to finish and then check results
    loop {
        tokio::time::sleep(time::Duration::from_millis(10)).await;

        let buf = get_routine_results_frame(0xf00f);
        println!("Getting routine results for 0xf00f: {:02x?}", buf);

        if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone(), 20).await {
            if let Ok(resp) = resp.await {
                println!("f00f status: {:02x?}", resp);

                if resp[0] == 0x7F {
                    println!("erase failed, not started");
                    panic!()
                }

                // let rout: u8 = ;
                let sid: u8 = UdsCommand::RoutineControl.into();
                if resp[0] == sid + 0x40 {
                    if resp.len() == 3 && resp[2] == 2 {
                        match resp[2] {
                            0 => {
                                println!("erase failed");
                                panic!("erase failed")
                            }
                            1 => {
                                println!("erase in progress");
                                continue;
                            }
                            2 => {
                                println!("erase completed");
                                break;
                            }
                            _ => {}
                        }
                        break;
                    }
                }
            }
        }
    }

    // start download
    let mut started = false;
    let mut dl_params = DownloadParams::default();

    let buf = start_download_frame(UdsDownloadStart::default(0x08002000, 4));
    println!("Starting UDS download: {:02x?}", buf);

    if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone(), 50).await {
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
                dl_params = DownloadParams {
                    counter: 0,
                    chunksize_len: resp[1] >> 4,
                    chunksize: resp[2] as u16,
                };
                started = true;
            }
        }
    }

    // actually transfer some data
    if started {
        // load the binary to download
        if let Ok(file) = read("../../firmware/components/heartbeat/build/heartbeat_crc.bin") {
            // iterate over it in chunks, the size of which were specified by the ECUs response to
            // the "start transfer" command
            for chunk in file.chunks(dl_params.chunksize as usize - 1) {
                let buf = transfer_data_frame(&mut dl_params, chunk);
                // done reading the file once the frame doesn't contain any data bytes
                if buf.len() == 2 {
                    println!("File read in its entirety");
                    break;
                }
                println!("Transferring data over UDS: {:02x?}", buf);

                // send the frame
                if let Ok(resp) = CanioCmd::send_recv(&buf, uds_queue_tx.clone(), 50).await {
                    match resp.await {
                        Ok(resp) => {
                            println!("response: {:02x?}", resp);
                            if resp[0] == 0x7f {
                                let nrc = UdsErrorByte::from(*resp.last().unwrap());
                                println!("ECU reports failure: {}", nrc.desc());
                                break;
                            }
                        }
                        Err(e) => {
                            println!("error in response: {:?}", e);
                            break;
                        }
                    }
                }
            }

            // finish the download, either because it completed successfully or because it failed
            // early
            if let Ok(resp) =
                CanioCmd::send_recv(&stop_download_frame(), uds_queue_tx.clone(), 20).await
            {
                if let Ok(resp) = resp.await {
                    println!("response: {:02x?}", resp);
                }
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
