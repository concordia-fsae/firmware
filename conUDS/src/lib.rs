use anyhow::{anyhow, Result};
use automotive_diag::uds::RoutineControlType;
use automotive_diag::uds::UdsCommand;
use tokio::sync::mpsc;
use tokio::sync::oneshot;

pub mod modules;

#[derive(Debug)]
pub enum CanioCmd {
    UdsCmdNoResponse(Vec<u8>),
    UdsCmdWithResponse {
        buf: Vec<u8>,
        resp_channel: oneshot::Sender<Vec<u8>>,
        timeout_ms: u32,
    },
}

impl CanioCmd {
    pub async fn send_recv(
        buf: &[u8],
        queue: mpsc::Sender<CanioCmd>,
        timeout_ms: u32,
    ) -> Result<oneshot::Receiver<Vec<u8>>> {
        let (tx, rx) = oneshot::channel();
        match queue
            .send(Self::UdsCmdWithResponse {
                buf: buf.to_owned(),
                resp_channel: tx,
                timeout_ms,
            })
            .await
        {
            Ok(_) => Ok(rx),
            Err(e) => Err(anyhow!(e)),
        }
    }
}

pub enum PrdCmd {
    PersistentTesterPresent(bool),
}

pub struct UdsDownloadStart {
    pub compression: u8,
    pub encryption: u8,
    pub size_len: u8,
    pub addr_len: u8,
    pub size: u32,
    pub addr: u32,
}

impl UdsDownloadStart {
    pub fn default(addr: u32, size: u32) -> Self {
        Self {
            compression: 0,
            encryption: 0,
            size_len: 4,
            addr_len: 4,
            size,
            addr,
        }
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        let mut ret = vec![
            self.compression << 4 | self.encryption,
            self.size_len << 4 | self.addr_len,
        ];

        ret.extend_from_slice(&self.size.to_le_bytes());
        ret.extend_from_slice(&self.addr.to_le_bytes());

        ret
    }
}

#[allow(dead_code)]
#[derive(Default)]
pub struct DownloadParams {
    pub counter: u8,
    pub chunksize_len: u8,
    pub chunksize: u16,
}

pub fn start_routine_frame(routine_id: u16, data: Option<Vec<u8>>) -> Vec<u8> {
    let mut ret = vec![
        UdsCommand::RoutineControl.into(),
        RoutineControlType::StartRoutine.into(),
    ];

    ret.extend_from_slice(&routine_id.to_le_bytes());

    if let Some(data) = data {
        ret.extend(data)
    }

    ret
}

pub fn start_download_frame(data: UdsDownloadStart) -> Vec<u8> {
    let mut ret = vec![UdsCommand::RequestDownload.into()];

    ret.extend_from_slice(&data.to_bytes());

    ret
}

pub fn stop_download_frame() -> Vec<u8> {
    vec![UdsCommand::RequestTransferExit.into()]
}

pub fn transfer_data_frame(params: &mut DownloadParams, data: &[u8]) -> Vec<u8> {
    let mut ret = vec![UdsCommand::TransferData.into(), params.counter.clone()];

    params.counter = params.counter.wrapping_add(1);
    ret.extend_from_slice(data);

    ret
}
