use std::path::PathBuf;
use std::str::FromStr;
use::std::time::Duration;

use anyhow::{anyhow, Result};
use crc::Crc;
use tokio::sync::mpsc;
use tokio::sync::oneshot;

pub mod arguments;
pub mod config;
#[cfg(target_os = "linux")]
pub mod modules;

const CRC8: Crc<u8> = Crc::<u8>::new(&crc::CRC_8_SAE_J1850);

#[derive(Debug)]
pub enum CanioCmd {
    UdsCmdNoResponse(Vec<u8>),
    UdsCmdWithResponse {
        buf: Vec<u8>,
        resp_channel: oneshot::Sender<Option<Vec<u8>>>,
        timeout_ms: u32,
    },
}

impl CanioCmd {
    pub async fn send_recv(
        buf: &[u8],
        queue: mpsc::Sender<CanioCmd>,
        timeout_ms: u32,
    ) -> Result<Vec<u8>> {
        let (tx, rx) = oneshot::channel();
        match queue
            .send(Self::UdsCmdWithResponse {
                buf: buf.to_owned(),
                resp_channel: tx,
                timeout_ms,
            })
            .await
        {
            Ok(_) => {
                return match rx.await {
                    Err(_) => Err(anyhow!("Unable to receive response")),
                    Ok(resp) => {
                        if let None = resp {
                            Err(anyhow!("No responses received"))
                        } else {
                            Ok(resp.expect("Illegal state"))
                        }
                    }
                };
            },
            Err(e) => Err(e.into()),
        }
    }
}

#[derive(Debug)]
pub struct UpdateResult {
    pub bin: PathBuf,
    pub result: FlashStatus,
    pub duration: Duration,
}

#[derive(Debug)]
pub enum FlashStatus {
    Failed(String),
    CrcMatch,
    DownloadSuccess,
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

#[derive(Default)]
pub struct DownloadParams {
    pub counter: u8,
    pub chunksize_len: u8,
    pub chunksize: u16,
}

pub struct ParseError {}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Error parsing data")
    }
}

#[derive(Debug)]
pub enum SupportedResetTypes {
    Hard = 0x01,
    Soft = 0x03,
}

impl FromStr for SupportedResetTypes {
    type Err = ParseError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "hard" => Ok(Self::Hard),
            "soft" => Ok(Self::Soft),
            _ => Err(ParseError {}),
        }
    }
}

impl From<SupportedResetTypes> for automotive_diag::uds::ResetType {
    fn from(value: SupportedResetTypes) -> Self {
        match value {
            SupportedResetTypes::Hard => Self::HardReset,
            SupportedResetTypes::Soft => Self::SoftReset,
        }
    }
}
