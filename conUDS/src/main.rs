use core::time;
use std::{sync::Arc, sync::Mutex};
use futures::future::BoxFuture;

use anyhow::Result;

pub mod modules;

use crate::modules::canio;

struct App {
    exit: bool,
}

impl App {
    fn new() -> Self {
        Self {
            exit: false,
        }
    }
}

type TskFn = fn(Arc<Mutex<App>>) -> BoxFuture<'static, Result<()>>;


#[tokio::main]
async fn main() {

    let app = Arc::new(Mutex::new(App::new()));
    let mut handles = vec![];

    let tasks: Vec<TskFn> = vec![
        |app| Box::pin(tsk_canio(app)),
        |app| Box::pin(tsk_10ms(app))];
    for tsk in tasks {
        let app_clone = Arc::clone(&app);
        handles.push(tokio::spawn(async move {tsk(app_clone)}));
    }

    // fixme: probably wait for shutdown signal here, then signal threads to exit

    for handle in handles {
        let res = handle.await.unwrap().await;
        if !res.is_ok() {
            println!("{:?}", res);
            app.lock().unwrap().exit = true;
        }
    }
}


/// CANIO task
///
/// Will handle converting between UDS commands and CAN frames, and interfacing with the hardware
async fn tsk_canio(app: Arc<Mutex<App>>) -> Result<()> {
    canio::process()?;
    Ok(())
}

/// 10ms(100Hz) periodic task
///
/// This task will run at 100Hz in order to handle periodic actions
async fn tsk_10ms(app: Arc<Mutex<App>>) -> Result<()>{
    loop {
        if app.lock().unwrap().exit {
            break;
        }

        tokio::time::sleep(time::Duration::from_millis(10)).await;

    }

    #[allow(unreachable_code)]
    Ok(())
}
