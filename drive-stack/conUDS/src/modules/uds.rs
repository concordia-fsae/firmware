use core::time;
use std::borrow::Cow;
use std::io::stdin;
use std::fs::read;
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::Mutex;

use anyhow::anyhow;
use anyhow::Result;
use automotive_diag::uds::UdsCommand;
use automotive_diag::uds::UdsErrorByte;
use automotive_diag::uds::UdsError;
use ecu_diagnostics::uds::UDSProtocol;
use automotive_diag::uds::{ResetType, RoutineControlType};
use ecu_diagnostics::dynamic_diag::EcuNRC;
use ecu_diagnostics::dynamic_diag::DiagProtocol;
use log::{debug, error, info};
use indicatif::{ProgressBar, ProgressStyle};
use tokio::sync::mpsc;
use tokio::task::JoinHandle;

use crate::DownloadParams;
use crate::SupportedResetTypes;
use crate::UdsDownloadStart;
use crate::CRC8;
use crate::{CanioCmd, PrdCmd};
use crate::modules::canio::CANIO;

#[derive(Debug)]
pub struct UdsClient {
    cmd_queue_tx: mpsc::Sender<PrdCmd>,
    uds_queue_tx: mpsc::Sender<CanioCmd>,
}

pub struct UdsSession {
    pub client: UdsClient,
    exit: Arc::<Mutex::<bool>>,
    threads: [JoinHandle<Result<()>>; 2],
    interactive_session: bool,
}

impl UdsSession {
    pub async fn new(
        device: &str,
        request_id: u32,
        response_id: u32,
        is_interactive: bool,
    ) -> Self {
        let exit = Arc::new(Mutex::new(bool::default()));
        let app_canio = Arc::clone(&exit);
        let app_10ms = Arc::clone(&exit);

        let (uds_queue_tx, uds_queue_rx) = mpsc::channel::<CanioCmd>(100);
        let (cmd_queue_tx, mut cmd_queue_rx) = mpsc::channel::<PrdCmd>(10);

        let uds_client = UdsClient::new(cmd_queue_tx.clone(), uds_queue_tx.clone());
        debug!("UDS client initialized: {:#?}", uds_client);

        let canio = {
            match CANIO::new(
                &device,
                request_id,
                response_id,
                uds_queue_rx,
            ) {
                Ok(canio) => {
                    debug!("CANIO object initialized: {}", canio);
                    canio
                }
                Err(e) => {
                    panic!("New CANIO connection failed due to {:?}", e);
                }
            }
        };

        debug!("Spawning threads");
        let t1 = tokio::spawn(async move { tsk_canio(app_canio, canio).await });
        let t2 = tokio::spawn(async move { tsk_10ms(app_10ms, &mut cmd_queue_rx, uds_queue_tx).await });
        Self {
            exit: exit,
            client: uds_client,
            threads: [t1, t2],
            interactive_session: is_interactive,
        }
    }

    pub async fn teardown(self) {
        *self.exit.lock().unwrap() = true;

        debug!("Main app logic done, joining threads");
        for thread in self.threads {
            if let Ok(thandle) = thread.await {
                thandle.unwrap();
            }
        }
        debug!("Threads finished, exiting");

        // fixme: probably wait for shutdown signal here, then signal threads to exit

        // for handle in handles {
        //     let res = handle.await.unwrap().await;
        //     if !res.is_ok() {
        //         println!("{:?}", res);
        //         app.lock().unwrap().exit = true;
        //     } tk
        // }
    }

    pub async fn reset_node(&mut self, reset_type: SupportedResetTypes) -> Result<()> {
        self.client.ecu_reset(reset_type).await
    }

    pub async fn file_download(&mut self, path: &PathBuf, address: u32) -> Result<()> {
        self.client.start_persistent_tp();

        if self.interactive_session {
            info!("Waiting for the user to hit enter before continuing with download");
            let mut garbage = String::new();
            while stdin().read_line(&mut garbage).is_err() {
                // wait for user to hit enter
            }
            info!("Enter key detected, proceeding with download");
        }


        self.client.app_download(path.to_path_buf(), address).await
    }
}

/// CANIO task
///
/// Owns the CAN hardware and handles all transmitting and receiving functions
async fn tsk_canio(exit: Arc<Mutex<bool>>, mut canio: CANIO<'_>) -> Result<()> {
    debug!("CANIO thread starting");

    while !exit.try_lock().is_ok_and(|exit| *exit) {
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
    exit: Arc<Mutex<bool>>,
    cmd_queue: &mut mpsc::Receiver<PrdCmd>,
    uds_queue: mpsc::Sender<CanioCmd>,
) -> Result<()> {
    debug!("10ms thread starting");

    let mut send_tp = false;

    while !exit.try_lock().is_ok_and(|exit| *exit) {
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

impl UdsClient {
    /// Create a UDS client
    pub fn new(cmd_queue_tx: mpsc::Sender<PrdCmd>, uds_queue_tx: mpsc::Sender<CanioCmd>) -> Self {
        Self {
            cmd_queue_tx,
            uds_queue_tx,
        }
    }

    /// Send a cmd to the 10ms periodic task
    async fn send_cmd(&mut self, cmd: PrdCmd) -> Result<(), mpsc::error::SendError<PrdCmd>> {
        self.cmd_queue_tx.send(cmd).await
    }

    pub async fn start_persistent_tp(&mut self) -> Result<()> {
        info!("Starting persistent TP");
        Ok(self.send_cmd(PrdCmd::PersistentTesterPresent(true)).await?)
    }

    pub async fn stop_persistent_tp(&mut self) -> Result<()> {
        info!("Stopping persistent TP");
        Ok(self
            .send_cmd(PrdCmd::PersistentTesterPresent(false))
            .await?)
    }

    pub async fn did_read(&mut self, did: u16) -> Result<()> {
        let id: [u8; 2] = [
            (did & 0xff).try_into().unwrap(),
            (did >> 8).try_into().unwrap(),
        ];
        let buf: [u8; 3] = [
            UdsCommand::ReadDataByIdentifier.into(),
            id[0],
            id[1],
        ];

        info!("Sending ECU read DID command: {:02x?}", buf);

        match CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await
        {
            Ok(resp) => {
                if resp.len() == 2 {
                    let nrc = UdsErrorByte::from(*resp.last().unwrap());
                    if nrc == ecu_diagnostics::Standard(UdsError::ServiceNotSupported) {
                        error!("Read DID not supported by ECU.");
                    }
                    else {
                        info!("ECU Read DID results: {:02x?}", resp);
                    }
                }
                else {
                    let app_id = u8::from(*resp.last().unwrap());
                    info!("ECU Read DID results: {:02x?}", resp);
                    info!("App ID: {:01x?}", app_id);
                }
            },
            Err(e) => {
                error!("When waiting for response from ECU: {}", e);
                return Err(e.into());
            }
        }
        Ok(())
    }

    pub async fn ecu_reset(&mut self, reset_type: SupportedResetTypes) -> Result<()> {
        let buf = [
            UdsCommand::ECUReset.into(),
            <SupportedResetTypes as Into<ResetType>>::into(reset_type).into(),
        ];

        info!("Sending ECU reset command: {:02x?}", buf);

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        info!("ECU Reset results: {:02x?}", resp);
        let nrc = UdsErrorByte::from(*resp.last().unwrap());
        if nrc == ecu_diagnostics::Standard(UdsError::ConditionsNotCorrect) {
            error!("ECU Reset conditions have not been met. If this is a bootloader updater, reflash with a correct hex.");
        }
        Ok(())
    }

    /// Start the given routine
    pub async fn routine_start(&mut self, routine_id: u16, data: Option<Vec<u8>>) -> Result<()> {
        let mut buf = vec![
            UdsCommand::RoutineControl.into(),
            RoutineControlType::StartRoutine.into(),
        ];

        buf.extend_from_slice(&routine_id.to_le_bytes());

        if let Some(data) = data {
            buf.extend(data)
        }

        debug!("Starting routine 0x{:02x}: {:02x?}", routine_id, buf);

        match CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await
        {
            Ok(resp) => info!("Start routine response: {:02x?}", resp),
            Err(e) => {
                error!("When waiting for response from ECU: {}", e);
                return Err(e.into());
            }
        }

        Ok(())
    }

    /// Request results from the ECU for the given routine
    pub async fn routine_get_results(&mut self, routine_id: u16) -> Result<Vec<u8>> {
        let mut buf = vec![
            UdsCommand::RoutineControl.into(),
            RoutineControlType::RequestRoutineResult.into(),
        ];

        buf.extend_from_slice(&routine_id.to_le_bytes());

        debug!(
            "Getting routine results for routine 0x{:02x}: {:02x?}",
            routine_id, buf
        );

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;

        debug!(
            "Routine results response for 0x{:02x}: {:02x?}",
            routine_id, resp
        );

        Ok(resp)
    }

    /// Tell the ECU to erase the app
    pub async fn app_erase(&mut self) -> Result<()> {
        info!("Starting App Erase");
        self.routine_start(0xf00f, None).await?;

        let pg = ProgressBar::new_spinner()
            .with_message(Cow::Borrowed("Erasing app"))
            .with_style(ProgressStyle::with_template("{spinner} {msg} [{elapsed}]")?);

        let mut retries = 1000; // 100 retries with a 10ms delay is 1s, which should be plenty of time
                               // to erase any of our current apps.

        loop {
            let resp = self.routine_get_results(0xf00f).await?;
            if resp[0] == 0x7F {
                info!("App erase failed, not started");
                return Err(anyhow!("App erase failed, not started"));
            }

            let sid: u8 = UdsCommand::RoutineControl.into();
            if resp[0] == sid + 0x40 {
                if resp.len() == 3 {
                    // FIXME: this should be an enum, not an int
                    match resp[2] {
                        0 => {
                            pg.set_style(ProgressStyle::with_template(&format!(
                                "App erase failed in {:.2}s",
                                pg.elapsed().as_secs_f32()
                            ))?);
                            pg.tick();
                            pg.finish();
                            error!("App erase failed");
                            return Err(anyhow!("App erase failed"));
                        }
                        1 => {
                            debug!("App erase in progress");
                            pg.tick();
                        }
                        2 => {
                            pg.set_style(ProgressStyle::with_template(&format!(
                                "App erased succesfully in {:.2}s",
                                pg.elapsed().as_secs_f32()
                            ))?);
                            pg.tick();
                            pg.finish();
                            debug!("App erase completed successfully!");
                            return Ok(());
                        }
                        _ => {
                            pg.set_style(ProgressStyle::with_template(&format!(
                                "App erase failed in {:.2}s",
                                pg.elapsed().as_secs_f32()
                            ))?);
                            pg.tick();
                            pg.finish();
                            error!("App erase failed, unexpected response from ECU");
                            return Err(anyhow!("App erase failed, unexpected response from ECU"));
                        }
                    }
                } else {
                    pg.set_style(ProgressStyle::with_template(&format!(
                        "App erase failed in {:.2}s",
                        pg.elapsed().as_secs_f32()
                    ))?);
                    pg.tick();
                    pg.finish();
                    error!("Unexpected response size from ECU");
                    return Err(anyhow!("Unexpected response size from ECU"));
                }
            } else {
                pg.set_style(ProgressStyle::with_template(&format!(
                    "App erase failed in {:.2}s",
                    pg.elapsed().as_secs_f32()
                ))?);
                pg.tick();
                pg.finish();
                error!("Response is not a positive response for this SID");
                return Err(anyhow!("Response is not a positive response for this SID"));
            }

            retries -= 1;
            if retries == 0 {
                pg.set_style(ProgressStyle::with_template(&format!(
                    "App erase timed out after {:.2}s",
                    pg.elapsed().as_secs_f32()
                ))?);
                pg.tick();
                pg.finish();
                error!("App erase did not report success within 1s");
                return Err(anyhow!("App erase did not report success within 1s"));
            }

            // retry at 10ms
            tokio::time::sleep(time::Duration::from_millis(10)).await;
        }
    }

    /// Negotiate the start of a download with the ECU
    async fn download_start(&mut self, data: UdsDownloadStart) -> Result<DownloadParams> {
        let mut buf = vec![UdsCommand::RequestDownload.into()];

        buf.extend_from_slice(&data.to_bytes());

        debug!("Starting UDS download: {:02x?}", buf);

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        debug!("Download start response: {:02x?}", resp);

        if resp[0] == 0x7F {
            let nrc = UdsErrorByte::from(*resp.last().unwrap());
            // FIXME: some of these should result in a retry
            if nrc.is_ecu_busy() {
                error!("ECU is processing the command");
                Err(anyhow!("ECU is processing the command"))
            } else if nrc.is_repeat_request() {
                error!("ECU requests retransmission");
                Err(anyhow!("ECU requests retransmission"))
            } else {
                error!("In ECU response: {}", nrc.desc());
                Err(anyhow!("Error in ECU response: {}", nrc.desc()))
            }
        } else {
            Ok(DownloadParams {
                counter: 0,
                chunksize_len: resp[1] >> 4,
                chunksize: resp[2] as u16,
            })
        }
    }

    /// Stop transfer (download or upload)
    async fn transfer_stop(&mut self) -> Result<()> {
        let buf = [UdsCommand::RequestTransferExit.into()];
        debug!("Stopping UDS transfer: {:02x?}", buf);

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        debug!("Transfer stop response: {:02x?}", resp);

        Ok(())
    }

    /// Transfer Payload
    async fn transfer_payload(&mut self, params: &mut DownloadParams, data: &[u8]) -> Result<()> {
        let mut buf = vec![UdsCommand::TransferData.into(), params.counter];

        params.counter = params.counter.wrapping_add(1);
        buf.extend_from_slice(data);
        let chunk_crc = CRC8.checksum(data);
        buf.push(chunk_crc);

        debug!("Transferring data over UDS: {:02x?}", buf);

        // send the frame
        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        debug!("Transfer payload response: {:02x?}", resp);

        if resp[0] == 0x7f {
            let nrc = UdsErrorByte::from(*resp.last().unwrap());
            error!("ECU reports failure: {}", nrc.desc());
            return Err(anyhow!("ECU reports failure"));
        }

        Ok(())
    }

    /// Trigger an app download process
    ///
    /// Starts by telling the device to erase the current app, then starts the app download, and lastly
    /// starts actually transferring the app
    pub async fn app_download(&mut self, file: PathBuf, address: u32) -> Result<()> {
        // disable tester present. bootloader is not currently robust to having tester present enabled
        // during an app download
        self.send_cmd(PrdCmd::PersistentTesterPresent(false))
            .await?;

        // start by erasing the app
        self.app_erase().await?;

        // figure out how many bytes we need to transfer
        let file_len_bytes = std::fs::metadata(&file)?.len();

        // start the download, which returns the parameters to use for the subsequent transmissions
        let mut dl_params = self
            .download_start(UdsDownloadStart::default(
                address,
                file_len_bytes.try_into()?,
            ))
            .await?;

        // actually transfer some data

        // load the binary to download
        // FIXME: this is not efficient. Figure out how to accomplish this (the chunking) with
        // BufReader
        let file = read(file)?;

        let pg = ProgressBar::new(file_len_bytes)
            .with_message(Cow::Borrowed("Downloading app: "))
            .with_style(ProgressStyle::with_template(
                "{msg} {bar} {decimal_bytes} / {decimal_total_bytes} [{duration}]",
            )?);

        let mut error = false;
        // iterate over the file in chunks of the size specified by the ECUs response to
        // the "start transfer" command
        for chunk in file.chunks(dl_params.chunksize as usize - 1) {
            // FIXME: this `if` might not be necessary
            if !chunk.is_empty() {
                if let Err(_) = self.transfer_payload(&mut dl_params, chunk).await {
                    pg.set_style(ProgressStyle::with_template(&format!(
                        "App download failed in {:.2}s",
                        pg.elapsed().as_secs_f32()
                    ))?);
                    pg.tick();
                    pg.finish();
                    error = true;
                    break
                }
                pg.inc(dl_params.chunksize.into());
            }
        }

        if !error {
            debug!("File read and transferred in its entirety");
            pg.set_style(ProgressStyle::with_template(&format!(
                "App download completed! Transferred {{decimal_total_bytes}} in {:.2}s",
                pg.elapsed().as_secs_f32()
            ))?);
            pg.tick();
            pg.finish();
        }

        // finish the download, either because it completed successfully or because it failed
        // early
        self.transfer_stop().await?;

        Ok(())
    }
}
