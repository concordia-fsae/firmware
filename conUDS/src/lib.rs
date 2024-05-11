use anyhow::Result;
use crc::Crc;
use tokio::sync::mpsc;
use tokio::sync::oneshot;

pub mod modules;

const CRC8: Crc<u8> = Crc::<u8>::new(&crc::CRC_8_SAE_J1850);

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
            Err(e) => Err(e.into()),
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

#[derive(Default)]
pub struct DownloadParams {
    pub counter: u8,
    pub chunksize_len: u8,
    pub chunksize: u16,
}


    }
}

}


}

}
