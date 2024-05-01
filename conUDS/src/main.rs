use core::time;
// use futures::future::BoxFuture;
use std::{sync::Arc, sync::Mutex};
use std::io::stdin;

use anyhow::Result;
use anyhow::anyhow;
use ecu_diagnostics::dynamic_diag::{DiagProtocol, DiagPayload};
use ecu_diagnostics::uds::{UDSProtocol, UdsCommand};
use modules::canio::CANIO;
use tokio::sync::mpsc;
use tokio::sync::oneshot;

pub mod modules;

struct App {
    exit: bool,
}

impl App {
    fn new() -> Self {
        Self { exit: false }
    }
}

// struct CmdResp {
//     cmd: Vec<u8>,
//     resp: oneshot::Sender<Vec<u8>>,
// }

// impl CmdResp {
//     async fn send(cmd: Vec<u8>, send: &mut mpsc::Sender<Self>) -> oneshot::Receiver<Vec<u8>> {
//         let (resp, recv) = oneshot::channel::<Vec<u8>>();

//         let _ = send.send(Self { cmd, resp }).await;
//         recv
//     }
// }

#[derive(Debug)]
pub enum CanioCmd {
    UdsCmdNoResponse(Vec<u8>),
    UdsCmdWithResponse(Vec<u8>, oneshot::Sender<Vec<u8>>),
}

impl CanioCmd {
    #[allow(dead_code)]
    async fn send_with_resp(
        buf: &[u8],
        queue: mpsc::Sender<CanioCmd>,
    ) -> Result<oneshot::Receiver<Vec<u8>>> {
        let (tx, rx) = oneshot::channel();
        match queue.send(Self::UdsCmdWithResponse(buf.to_owned(), tx)).await {
            Ok(_) => Ok(rx),
            Err(e) => Err(anyhow!(e)),
        }
    }
}

enum PrdCmd {
    PersistentTesterPresent(bool),
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

    let _ = cmd_queue_tx
        .send(PrdCmd::PersistentTesterPresent(true))
        .await;

    let mut buf = String::new();
    while !stdin().read_line(&mut buf).is_ok() {
        // wait for user to hit enter
    }

    let buf = DiagPayload::new(
        UdsCommand::RoutineControl.into(),
        &[0x01, 0xf0, 0x0f],
    ).to_bytes();

    let resp = CanioCmd::send_with_resp(&buf, uds_queue_tx);

    if let Ok(resp) = resp.await {
        if let Ok(resp) = resp.await {
            println!("response: {:?}", resp);
        }

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
