use core::time;
use std::fs::read;
use std::path::PathBuf;

use anyhow::anyhow;
use anyhow::Result;
use automotive_diag::uds::UdsCommand;
use automotive_diag::uds::UdsErrorByte;
use automotive_diag::uds::{ResetType, RoutineControlType};
use ecu_diagnostics::dynamic_diag::EcuNRC;
use log::{error, info};
use tokio::sync::mpsc;

use crate::DownloadParams;
use crate::SupportedResetTypes;
use crate::UdsDownloadStart;
use crate::CRC8;
use crate::{CanioCmd, PrdCmd};

#[derive(Debug)]
pub struct UdsClient {
    cmd_queue_tx: mpsc::Sender<PrdCmd>,
    uds_queue_tx: mpsc::Sender<CanioCmd>,
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

        info!("Starting routine 0x{:02x}: {:02x?}", routine_id, buf);

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

        info!(
            "Getting routine results for routine 0x{:02x}: {:02x?}",
            routine_id, buf
        );

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;

        info!(
            "Routine results response for 0x{:02x}: {:02x?}",
            routine_id, resp
        );

        Ok(resp)
    }

    /// Tell the ECU to erase the app
    pub async fn app_erase(&mut self) -> Result<()> {
        self.routine_start(0xf00f, None).await?;

        let mut retries = 100; // 100 retries with a 10ms delay is 1s, which should be plenty of time
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
                            println!("App erase failed");
                            return Err(anyhow!("App erase failed"));
                        }
                        1 => {
                            println!("App erase in progress");
                        }
                        2 => {
                            println!("App erase completed!");
                            return Ok(());
                        }
                        _ => {
                            return Err(anyhow!("Unexpected response from ECU"));
                        }
                    }
                } else {
                    return Err(anyhow!("Unexpected response size from ECU"));
                }
            } else {
                return Err(anyhow!("Response is not a positive response for this SID"));
            }

            retries -= 1;
            if retries == 0 {
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

        info!("Starting UDS download: {:02x?}", buf);

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        info!("Download start response: {:02x?}", resp);

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
        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        info!("Transfer stop response: {:02x?}", resp);

        Ok(())
    }

    /// Transfer Payload
    async fn transfer_payload(&mut self, params: &mut DownloadParams, data: &[u8]) -> Result<()> {
        let mut buf = vec![UdsCommand::TransferData.into(), params.counter];

        params.counter = params.counter.wrapping_add(1);
        buf.extend_from_slice(data);
        let chunk_crc = CRC8.checksum(data);
        buf.push(chunk_crc);

        info!("Transferring data over UDS: {:02x?}", buf);

        // send the frame
        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50)
            .await?
            .await?;
        info!("Transfer payload response: {:02x?}", resp);

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
    pub async fn app_download(&mut self, file: PathBuf) -> Result<()> {
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
                0x08002000,
                file_len_bytes.try_into()?,
            ))
            .await?;

        // actually transfer some data

        // load the binary to download
        // FIXME: this is not efficient. Figure out how to accomplish this (the chunking) with
        // BufReader
        let file = read(file)?;

        // iterate over the file in chunks of the size specified by the ECUs response to
        // the "start transfer" command
        for chunk in file.chunks(dl_params.chunksize as usize - 1) {
            if !chunk.is_empty() {
                self.transfer_payload(&mut dl_params, chunk).await?;
            } else {
                info!("File read and transferred in its entirety");
                return Ok(());
            }
        }

        // finish the download, either because it completed successfully or because it failed
        // early
        self.transfer_stop().await?;

        Ok(())
    }
}
