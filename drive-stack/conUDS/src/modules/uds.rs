use core::time;
use std::borrow::Cow;
use std::fs::File;
use std::fs::read;
use std::io::Read;
use std::io::Seek;
use std::io::SeekFrom;
use std::io::stdin;
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::Mutex;
use std::time::Instant;

use anyhow::Result;
use anyhow::anyhow;
use automotive_diag::uds::UdsCommand;
use automotive_diag::uds::UdsError;
use automotive_diag::uds::UdsErrorByte;
use automotive_diag::uds::{ResetType, RoutineControlType, UdsSessionType};
use ecu_diagnostics::dynamic_diag::DiagProtocol;
use ecu_diagnostics::dynamic_diag::EcuNRC;
use ecu_diagnostics::uds::UDSProtocol;
use indicatif::{ProgressBar, ProgressStyle};
use log::{debug, error, info};
use tokio::sync::{mpsc, oneshot};
use tokio::task::JoinHandle;

use crate::CRC8;
use crate::DownloadParams;
use crate::SupportedDiagnosticSessions;
use crate::SupportedResetTypes;
use crate::UdsDownloadStart;
use crate::modules::canio::CANIO;
use crate::{CanioCmd, PrdCmd};
use crate::{FlashStatus, UpdateResult};

const UDS_DID_CRC: u16 = 0x03;
const UDS_DID_CURRENT_SESSION: u16 = 0x0102;

#[derive(Debug, Clone)]
pub enum RoutineStartResponse {
    Positive {
        payload: Vec<u8>,
        text: Option<String>,
        raw: Vec<u8>,
    },
    Negative {
        nrc: u8,
        description: String,
        payload: Vec<u8>,
        text: Option<String>,
        raw: Vec<u8>,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiagnosticSessionKind {
    Default,
    Programming,
    Extended,
    SafetySystem,
}

impl DiagnosticSessionKind {
    pub fn as_uds_session_type(self) -> UdsSessionType {
        match self {
            Self::Default => UdsSessionType::Default,
            Self::Programming => UdsSessionType::Programming,
            Self::Extended => UdsSessionType::Extended,
            Self::SafetySystem => UdsSessionType::SafetySystem,
        }
    }
}

impl From<SupportedDiagnosticSessions> for DiagnosticSessionKind {
    fn from(value: SupportedDiagnosticSessions) -> Self {
        match value {
            SupportedDiagnosticSessions::Default => Self::Default,
            SupportedDiagnosticSessions::Programming => Self::Programming,
            SupportedDiagnosticSessions::Extended => Self::Extended,
            SupportedDiagnosticSessions::SafetySystem => Self::SafetySystem,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CurrentDiagnosticSession {
    Default,
    Programming,
    Extended,
    SafetySystem,
    Unknown(u8),
}

impl CurrentDiagnosticSession {
    pub fn key(self) -> &'static str {
        match self {
            Self::Default => "default",
            Self::Programming => "programming",
            Self::Extended => "extended",
            Self::SafetySystem => "safety-system",
            Self::Unknown(_) => "unknown",
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            Self::Default => "Default",
            Self::Programming => "Programming",
            Self::Extended => "Extended",
            Self::SafetySystem => "Safety System",
            Self::Unknown(_) => "Unknown",
        }
    }

    pub fn raw_value(self) -> u8 {
        match self {
            Self::Default => 0x01,
            Self::Programming => 0x02,
            Self::Extended => 0x03,
            Self::SafetySystem => 0x04,
            Self::Unknown(value) => value,
        }
    }

    fn from_did_payload(payload: &[u8]) -> Result<Self> {
        let Some(value) = payload.first().copied() else {
            return Err(anyhow!("Current session DID returned an empty payload"));
        };
        Ok(match value {
            0x01 => Self::Default,
            0x02 => Self::Programming,
            0x03 => Self::Extended,
            0x04 => Self::SafetySystem,
            _ => Self::Unknown(value),
        })
    }
}

#[derive(Debug, Clone)]
pub enum DiagnosticSessionResponse {
    Positive {
        session: DiagnosticSessionKind,
        payload: Vec<u8>,
        raw: Vec<u8>,
    },
    Negative {
        session: DiagnosticSessionKind,
        nrc: u8,
        description: String,
        payload: Vec<u8>,
        raw: Vec<u8>,
    },
}

impl DiagnosticSessionResponse {
    fn from_raw(session: DiagnosticSessionKind, resp: Vec<u8>) -> Result<Self> {
        if resp.is_empty() {
            return Err(anyhow!("Empty ECU response"));
        }

        if resp[0] == 0x7F {
            if resp.len() < 3 {
                return Err(anyhow!(
                    "Malformed negative diagnostic session response: {:02X?}",
                    resp
                ));
            }

            let payload = if resp.len() > 3 {
                resp[3..].to_vec()
            } else {
                Vec::new()
            };

            return Ok(Self::Negative {
                session,
                nrc: resp[2],
                description: UdsErrorByte::from(resp[2]).desc().to_string(),
                payload,
                raw: resp,
            });
        }

        let expected_sid = u8::from(UdsCommand::DiagnosticSessionControl) + 0x40;
        if resp[0] != expected_sid {
            return Err(anyhow!(
                "Unexpected diagnostic session response SID 0x{:02X}, expected 0x{:02X}",
                resp[0],
                expected_sid
            ));
        }

        let payload = if resp.len() > 2 {
            resp[2..].to_vec()
        } else {
            Vec::new()
        };

        Ok(Self::Positive {
            session,
            payload,
            raw: resp,
        })
    }
}

impl RoutineStartResponse {
    fn from_raw(resp: Vec<u8>) -> Result<Self> {
        if resp.is_empty() {
            return Err(anyhow!("Empty ECU response"));
        }

        if resp[0] == 0x7F {
            if resp.len() < 3 {
                return Err(anyhow!(
                    "Malformed negative routine start response: {:02X?}",
                    resp
                ));
            }

            let payload = if resp.len() > 3 {
                resp[3..].to_vec()
            } else {
                Vec::new()
            };

            return Ok(Self::Negative {
                nrc: resp[2],
                description: UdsErrorByte::from(resp[2]).desc().to_string(),
                text: decode_routine_payload_text(&payload),
                payload,
                raw: resp,
            });
        }

        let expected_sid = u8::from(UdsCommand::RoutineControl) + 0x40;
        if resp[0] != expected_sid {
            return Err(anyhow!(
                "Unexpected routine start response SID 0x{:02X}, expected 0x{:02X}",
                resp[0],
                expected_sid
            ));
        }

        if resp.len() < 4 {
            return Err(anyhow!(
                "Malformed positive routine start response: {:02X?}",
                resp
            ));
        }

        let payload = resp[4..].to_vec();

        Ok(Self::Positive {
            text: decode_routine_payload_text(&payload),
            payload,
            raw: resp,
        })
    }
}

fn decode_routine_payload_text(payload: &[u8]) -> Option<String> {
    let end = payload
        .iter()
        .rposition(|byte| *byte != 0)
        .map(|idx| idx + 1)
        .unwrap_or(0);
    if end == 0 {
        return None;
    }

    let text = String::from_utf8_lossy(&payload[..end]).trim().to_string();
    if text.is_empty() { None } else { Some(text) }
}

#[derive(Debug)]
pub struct UdsClient {
    cmd_queue_tx: mpsc::Sender<PrdCmd>,
    uds_queue_tx: mpsc::Sender<CanioCmd>,
}

pub struct UdsSession {
    pub client: UdsClient,
    exit: Arc<Mutex<bool>>,
    threads: [JoinHandle<Result<()>>; 2],
    interactive_session: bool,
}

enum UdsWorkerCommand {
    DidRead {
        did: u16,
        resp: oneshot::Sender<Result<Vec<u8>, String>>,
    },
    ReadCurrentSession {
        resp: oneshot::Sender<Result<CurrentDiagnosticSession, String>>,
    },
    EnterDiagnosticSession {
        session: DiagnosticSessionKind,
        resp: oneshot::Sender<Result<DiagnosticSessionResponse, String>>,
    },
    RoutineStart {
        routine_id: u16,
        data: Option<Vec<u8>>,
        resp: oneshot::Sender<Result<RoutineStartResponse, String>>,
    },
    ResetNode {
        reset_type: SupportedResetTypes,
        resp: oneshot::Sender<Result<(), String>>,
    },
    StartPersistentTp {
        resp: oneshot::Sender<Result<(), String>>,
    },
    StopPersistentTp {
        resp: oneshot::Sender<Result<(), String>>,
    },
    DownloadApp {
        binary_path: PathBuf,
        skip: bool,
        resp: oneshot::Sender<Result<UpdateResult, String>>,
    },
    FileDownload {
        binary_path: PathBuf,
        address: u32,
        resp: oneshot::Sender<Result<(), String>>,
    },
}

#[derive(Clone)]
pub struct UdsWorkerHandle {
    tx: mpsc::Sender<UdsWorkerCommand>,
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
            match CANIO::new(&device, request_id, response_id, uds_queue_rx) {
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
        let t2 =
            tokio::spawn(async move { tsk_10ms(app_10ms, &mut cmd_queue_rx, uds_queue_tx).await });
        Self {
            exit: exit,
            client: uds_client,
            threads: [t1, t2],
            interactive_session: is_interactive,
        }
    }

    pub async fn teardown(self) {
        {
            let mut guard = self.exit.lock().unwrap();
            *guard = true;
        }

        for handle in self.threads.into_iter() {
            let _ = handle.await;
        }
        debug!("Threads finished, exiting");
    }

    pub async fn reset_node(&mut self, reset_type: SupportedResetTypes) -> Result<()> {
        let resp = self
            .client
            .ecu_reset(reset_type, self.interactive_session)
            .await;
        if self.interactive_session {
            Ok(())
        } else {
            resp
        }
    }

    pub async fn read_current_session(&mut self) -> Result<CurrentDiagnosticSession> {
        self.client.read_current_session().await
    }

    pub async fn did_read(&mut self, did: u16) -> Result<Vec<u8>> {
        self.client.did_read(did).await.map_err(|error| match error {
            Some(error) => anyhow!("failed to read DID 0x{did:04X}: {error:?}"),
            None => anyhow!("failed to read DID 0x{did:04X}"),
        })
    }

    pub async fn enter_diagnostic_session(
        &mut self,
        session: DiagnosticSessionKind,
    ) -> Result<DiagnosticSessionResponse> {
        self.client.enter_diagnostic_session(session).await
    }

    pub async fn routine_start(
        &mut self,
        routine_id: u16,
        data: Option<Vec<u8>>,
    ) -> Result<RoutineStartResponse> {
        self.client.routine_start(routine_id, data).await
    }

    pub async fn start_persistent_tp(&mut self) -> Result<()> {
        self.client.start_persistent_tp().await
    }

    pub async fn stop_persistent_tp(&mut self) -> Result<()> {
        self.client.stop_persistent_tp().await
    }

    pub async fn file_download(&mut self, path: &PathBuf, address: u32) -> Result<()> {
        if self.interactive_session {
            info!("Waiting for the user to hit enter before continuing with download");
            let mut garbage = String::new();
            while stdin().read_line(&mut garbage).is_err() {
                // wait for user to hit enter
            }
            info!("Enter key detected, proceeding with download");
        } else {
            tokio::time::sleep(time::Duration::from_millis(50)).await;
        }

        self.client.app_download(path.to_path_buf(), address).await
    }

    pub async fn download_app_to_target(
        &mut self,
        binary_path: &PathBuf,
        skip: bool,
    ) -> UpdateResult {
        let node_start = Instant::now();

        if skip {
            let mut file = File::open(binary_path).expect("Binary does not exist!");
            file.seek(SeekFrom::End(-4)).expect("Seek failed");
            let mut buffer = [0u8; 4];
            file.read_exact(&mut buffer).expect("CRC read failed");
            let app_crc = u32::from_le_bytes(buffer);
            info!("Application CRC to download: 0x{:08X}", app_crc);

            let crc = match get_app_crc(self).await {
                Err(e) => {
                    let node_dur = node_start.elapsed();
                    return UpdateResult {
                        bin: binary_path.to_path_buf(),
                        result: FlashStatus::Failed(format!("{:?}", e)),
                        duration: node_dur,
                    };
                }
                Ok(crc) => crc,
            };

            if crc == app_crc {
                let node_dur = node_start.elapsed();
                return UpdateResult {
                    bin: binary_path.to_path_buf(),
                    result: FlashStatus::CrcMatch,
                    duration: node_dur,
                };
            } else {
                info!(
                    "CRC mismatch: node=0x{:08X}, app=0x{:08X}. Downloading...",
                    crc, app_crc
                );
            }
        }

        let _ = self.client.start_persistent_tp().await;
        if !self.interactive_session {
            if let Err(_) = self
                .client
                .ecu_reset(SupportedResetTypes::Hard, self.interactive_session)
                .await
            {
                let node_dur = node_start.elapsed();
                return UpdateResult {
                    bin: binary_path.to_path_buf(),
                    result: FlashStatus::Failed("Unable to communicate with ECU".to_string()),
                    duration: node_dur,
                };
            }
        }
        match self.file_download(&binary_path, 0x08002000).await {
            Ok(()) => {
                let node_dur = node_start.elapsed();
                return UpdateResult {
                    bin: binary_path.to_path_buf(),
                    result: FlashStatus::DownloadSuccess,
                    duration: node_dur,
                };
            }
            Err(e) => {
                let node_dur = node_start.elapsed();
                return UpdateResult {
                    bin: binary_path.to_path_buf(),
                    result: FlashStatus::Failed(format!("Error downloading binary: '{}'", e)),
                    duration: node_dur,
                };
            }
        }
    }
}

impl UdsWorkerHandle {
    pub fn new(
        device: String,
        request_id: u32,
        response_id: u32,
        is_interactive: bool,
    ) -> Self {
        let (tx, mut rx) = mpsc::channel(32);
        tokio::spawn(async move {
            let mut session: Option<UdsSession> = None;

            while let Some(cmd) = rx.recv().await {
                if session.is_none() {
                    session = Some(
                        UdsSession::new(&device, request_id, response_id, is_interactive).await,
                    );
                }
                let uds = session.as_mut().expect("session initialized");

                match cmd {
                    UdsWorkerCommand::DidRead { did, resp } => {
                        let _ = resp.send(uds.did_read(did).await.map_err(|e| e.to_string()));
                    }
                    UdsWorkerCommand::ReadCurrentSession { resp } => {
                        let _ = resp.send(uds.read_current_session().await.map_err(|e| e.to_string()));
                    }
                    UdsWorkerCommand::EnterDiagnosticSession { session, resp } => {
                        let _ = resp.send(
                            uds.enter_diagnostic_session(session)
                                .await
                                .map_err(|e| e.to_string()),
                        );
                    }
                    UdsWorkerCommand::RoutineStart {
                        routine_id,
                        data,
                        resp,
                    } => {
                        let _ = resp.send(
                            uds.routine_start(routine_id, data)
                                .await
                                .map_err(|e| e.to_string()),
                        );
                    }
                    UdsWorkerCommand::ResetNode { reset_type, resp } => {
                        let _ = resp.send(uds.reset_node(reset_type).await.map_err(|e| e.to_string()));
                    }
                    UdsWorkerCommand::StartPersistentTp { resp } => {
                        let _ =
                            resp.send(uds.start_persistent_tp().await.map_err(|e| e.to_string()));
                    }
                    UdsWorkerCommand::StopPersistentTp { resp } => {
                        let _ =
                            resp.send(uds.stop_persistent_tp().await.map_err(|e| e.to_string()));
                    }
                    UdsWorkerCommand::DownloadApp {
                        binary_path,
                        skip,
                        resp,
                    } => {
                        let _ = resp.send(Ok(uds.download_app_to_target(&binary_path, skip).await));
                    }
                    UdsWorkerCommand::FileDownload {
                        binary_path,
                        address,
                        resp,
                    } => {
                        let _ = resp.send(
                            uds.file_download(&binary_path, address)
                                .await
                                .map_err(|e| e.to_string()),
                        );
                    }
                }
            }

            if let Some(session) = session {
                session.teardown().await;
            }
        });

        Self { tx }
    }

    pub async fn read_current_session(&self) -> Result<CurrentDiagnosticSession> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::ReadCurrentSession { resp: tx })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn did_read(&self, did: u16) -> Result<Vec<u8>> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::DidRead { did, resp: tx })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn enter_diagnostic_session(
        &self,
        session: DiagnosticSessionKind,
    ) -> Result<DiagnosticSessionResponse> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::EnterDiagnosticSession { session, resp: tx })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn routine_start(
        &self,
        routine_id: u16,
        data: Option<Vec<u8>>,
    ) -> Result<RoutineStartResponse> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::RoutineStart {
                routine_id,
                data,
                resp: tx,
            })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn reset_node(&self, reset_type: SupportedResetTypes) -> Result<()> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::ResetNode {
                reset_type,
                resp: tx,
            })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn start_persistent_tp(&self) -> Result<()> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::StartPersistentTp { resp: tx })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn stop_persistent_tp(&self) -> Result<()> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::StopPersistentTp { resp: tx })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn download_app_to_target(
        &self,
        binary_path: PathBuf,
        skip: bool,
    ) -> Result<UpdateResult> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::DownloadApp {
                binary_path,
                skip,
                resp: tx,
            })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
    }

    pub async fn file_download(&self, binary_path: PathBuf, address: u32) -> Result<()> {
        let (tx, rx) = oneshot::channel();
        self.tx
            .send(UdsWorkerCommand::FileDownload {
                binary_path,
                address,
                resp: tx,
            })
            .await
            .map_err(|_| anyhow!("UDS worker is unavailable"))?;
        rx.await
            .map_err(|_| anyhow!("UDS worker response channel closed"))?
            .map_err(|e| anyhow!(e))
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
        } else {
            if send_tp {
                let _ = uds_queue
                    .send(CanioCmd::UdsCmdNoResponse(
                        UDSProtocol::create_tp_msg(false).to_bytes(),
                    ))
                    .await;
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
        Ok(self.send_cmd(PrdCmd::PersistentTesterPresent(true)).await?)
    }

    pub async fn stop_persistent_tp(&mut self) -> Result<()> {
        info!("Stopping persistent TP");
        Ok(self
            .send_cmd(PrdCmd::PersistentTesterPresent(false))
            .await?)
    }

    pub async fn read_current_session(&mut self) -> Result<CurrentDiagnosticSession> {
        let payload =
            self.did_read(UDS_DID_CURRENT_SESSION)
                .await
                .map_err(|error| match error {
                    Some(error) => anyhow!("failed to read current session DID: {error:?}"),
                    None => anyhow!("failed to read current session DID"),
                })?;
        CurrentDiagnosticSession::from_did_payload(&payload)
    }

    pub async fn did_read(&mut self, did: u16) -> Result<Vec<u8>, Option<UdsError>> {
        let id: [u8; 2] = [
            (did & 0xff).try_into().unwrap(),
            (did >> 8).try_into().unwrap(),
        ];
        let buf: [u8; 3] = [UdsCommand::ReadDataByIdentifier.into(), id[0], id[1]];

        match CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await {
            Ok(resp) => {
                if resp.is_empty() {
                    error!("Empty ECU response");
                    return Err(Some(UdsError::IncorrectMessageLengthOrInvalidFormat));
                }

                // Negative response? 0x7F, <requested SID>, <NRC>
                if resp[0] == 0x7F {
                    if resp.len() < 3 {
                        error!("Malformed negative response: {:02X?}", resp);
                        return Err(None);
                    }
                    let nrc_byte = resp[2];
                    let uds_err = match UdsError::try_from(nrc_byte) {
                        Ok(wrapped) => Some(wrapped.into()),
                        Err(_) => None,
                    };
                    return Err(uds_err);
                }

                // Positive response for 0x22 is 0x62
                if resp[0] != (u8::from(UdsCommand::ReadDataByIdentifier) + 0x40) {
                    error!("Unexpected positive SID: {:02X?}", resp);
                    return Err(None);
                }

                // We dont echo the DID
                if resp.len() < 2 {
                    error!("Positive response too short: {:02X?}", resp);
                    return Err(None);
                }

                let data = resp[1..].to_vec();
                Ok(data)
            }
            Err(e) => {
                error!("Waiting for ECU response failed: {}", e);
                Err(None)
            }
        }
    }

    pub async fn ecu_reset(
        &mut self,
        reset_type: SupportedResetTypes,
        recoverable: bool,
    ) -> Result<()> {
        let reset_kind: ResetType = reset_type.into();
        let buf = [UdsCommand::ECUReset.into(), reset_kind.into()];

        info!("Resetting ECU...");

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await?;
        debug!("ECU Reset results: {:02x?}", resp);

        if resp.is_empty() {
            if !recoverable {
                return Err(anyhow!("ECU reset failed: empty ECU response"));
            }
            return Ok(());
        }

        if resp[0] == 0x7F {
            if resp.len() < 3 {
                return Err(anyhow!(
                    "ECU reset failed: malformed negative response {:02X?}",
                    resp
                ));
            }

            let nrc = UdsErrorByte::from(resp[2]);
            if nrc == ecu_diagnostics::Standard(UdsError::ConditionsNotCorrect) {
                error!(
                    "ECU Reset conditions have not been met. If this is a bootloader updater, reflash with a correct hex."
                );
            }
            return Err(anyhow!("ECU reset rejected: {}", nrc.desc()));
        }

        let expected_sid = u8::from(UdsCommand::ECUReset) + 0x40;
        if resp[0] != expected_sid {
            return Err(anyhow!("ECU reset failed"));
        }

        if resp.len() < 2 {
            return Err(anyhow!(
                "ECU reset failed: malformed positive response {:02X?}",
                resp
            ));
        }

        if resp[1] != u8::from(reset_kind) {
            return Err(anyhow!(
                "ECU reset failed: unexpected reset echo 0x{:02X}, expected 0x{:02X}",
                resp[1],
                u8::from(reset_kind)
            ));
        }

        Ok(())
    }

    pub async fn enter_diagnostic_session(
        &mut self,
        session: DiagnosticSessionKind,
    ) -> Result<DiagnosticSessionResponse> {
        let buf = [
            UdsCommand::DiagnosticSessionControl.into(),
            session.as_uds_session_type().into(),
        ];

        debug!("Entering diagnostic session {:?}: {:02x?}", session, buf);

        match CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await {
            Ok(resp) => {
                debug!("Diagnostic session response: {:02x?}", resp);
                DiagnosticSessionResponse::from_raw(session, resp)
            }
            Err(e) => {
                error!(
                    "When waiting for diagnostic session response from ECU: {}",
                    e
                );
                Err(e.into())
            }
        }
    }

    /// Start the given routine
    pub async fn routine_start(
        &mut self,
        routine_id: u16,
        data: Option<Vec<u8>>,
    ) -> Result<RoutineStartResponse> {
        let mut buf = vec![
            UdsCommand::RoutineControl.into(),
            RoutineControlType::StartRoutine.into(),
        ];

        buf.extend_from_slice(&routine_id.to_le_bytes());

        if let Some(data) = data {
            buf.extend(data)
        }

        debug!("Starting routine 0x{:02x}: {:02x?}", routine_id, buf);

        match CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await {
            Ok(resp) => {
                debug!("Start routine response: {:02x?}", resp);
                RoutineStartResponse::from_raw(resp)
            }
            Err(e) => {
                error!("When waiting for response from ECU: {}", e);
                Err(e.into())
            }
        }
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

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await?;

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
                            return Err(anyhow!(
                                "App erase failed, unexpected response '{:02x}' from ECU for SID '{:02}'",
                                resp[2],
                                sid
                            ));
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
                return Err(anyhow!(
                    "Response '0x{:02x}' is not a positive response for SID '0x{:02}'",
                    resp[0],
                    sid
                ));
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
        info!("Starting UDS download...");
        let mut buf = vec![UdsCommand::RequestDownload.into()];

        buf.extend_from_slice(&data.to_bytes());

        debug!("Starting UDS download: {:02x?}", buf);

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await?;
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

        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 50).await?;
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
        let resp = CanioCmd::send_recv(&buf, self.uds_queue_tx.clone(), 100).await?;
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
                    break;
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

        if error {
            return Err(anyhow!("Download failed!"));
        }

        Ok(())
    }
}

async fn get_app_crc(uds_session: &mut UdsSession) -> Result<u32> {
    debug!("Getting app crc");
    let resp = uds_session.client.did_read(UDS_DID_CRC).await;
    if let Ok(node_crc) = resp {
        if let Ok(arr) = <[u8; 4]>::try_from(node_crc.as_slice()) {
            return Ok(u32::from_le_bytes(arr));
        } else {
            return Err(anyhow!("Invalid response length for CRC"));
        }
    }

    return Err(anyhow!("CRC read failure"));
}
