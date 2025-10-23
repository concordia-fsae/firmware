/// CANIO module
use std::marker::PhantomData;

use anyhow::{anyhow, Result};
use ecu_diagnostics::channel::{ChannelResult, IsoTPChannel, IsoTPSettings};
use ecu_diagnostics::hardware::{socketcan::SocketCanScanner, Hardware, HardwareScanner};
use log::{debug, error};
use tokio::sync::mpsc::Receiver;

use crate::CanioCmd;

#[derive(Debug)]
struct IDs {
    tx: u32,
    #[allow(dead_code)]
    rx: u32,
}

pub struct CANIO<'a> {
    ids: IDs,
    channel: Box<dyn IsoTPChannel>,
    uds_queue: Receiver<CanioCmd>,
    _marker: PhantomData<&'a str>,
}

impl<'a> CANIO<'a> {
    pub fn new<'b>(
        device: &'b str,
        send_id: u32,
        recv_id: u32,
        uds_queue: Receiver<CanioCmd>,
    ) -> Result<Self> {
        let scanner = SocketCanScanner::new();
        let devices = scanner.list_devices();

        if devices.is_empty() {
            error!("No CAN devices detected");
            return Err(anyhow!("No CAN devices detected"));
        }
        debug!("CAN Devices: {:#?}", devices);

        let mut device = {
            match scanner.open_device_by_name(device) {
                Ok(dev) => dev,
                Err(e) => {
                    error!("Failed to open CAN device: {}", e);
                    return Err(anyhow!("Failed to open CAN device: {}", e));
                }
            }
        };
        debug!("opened CAN device: {:#?}", device);

        let mut channel = {
            match device.create_iso_tp_channel() {
                Ok(ch) => ch,
                Err(e) => {
                    error!("Failed to create ISO-TP channel: {}", e);
                    return Err(anyhow!("Failed to create ISO-TP channel: {}", e));
                }
            }
        };

        let iso_tp_cfg = IsoTPSettings {
            block_size: 5,
            st_min: 5,
            extended_addresses: None,
            pad_frame: false,
            can_speed: 1000000,
            can_use_ext_addr: false,
        };

        if let Err(e) = channel.set_iso_tp_cfg(iso_tp_cfg) {
            error!("Failed to set ISO-TP config on channel: {}", e);
            return Err(anyhow!("Failed to set ISO-TP config on channel: {}", e));
        }
        if let Err(e) = channel.set_ids(send_id, recv_id) {
            error!("Failed to set UDS IDs on channel: {}", e);
            return Err(anyhow!("Failed to set UDS IDs on channel: {}", e));
        }
        if let Err(e) = channel.open() {
            error!("Failed to open channel: {}", e);
            return Err(anyhow!("Failed to open channel: {}", e));
        }

        Ok(Self {
            ids: IDs {
                tx: send_id,
                rx: recv_id,
            },
            channel,
            uds_queue,
            _marker: PhantomData,
        })
    }

    fn uds_send(&mut self, buf: &[u8], timeout_ms: u32) -> ChannelResult<()> {
        self.channel.write_bytes(self.ids.tx, None, buf, timeout_ms)
    }

    #[allow(dead_code)]
    fn uds_recv(&mut self, timeout_ms: u32) -> ChannelResult<Vec<u8>> {
        self.channel.read_bytes(timeout_ms)
    }

    fn uds_send_recv(
        &mut self,
        buf: &[u8],
        tx_timeout_ms: u32,
        rx_timeout_ms: u32,
    ) -> ChannelResult<Vec<u8>> {
        self.channel
            .read_write_bytes(self.ids.tx, None, buf, tx_timeout_ms, rx_timeout_ms)
    }

    pub async fn process(&mut self) -> Result<()> {
        if let Ok(cmd) = self.uds_queue.try_recv() {
            match cmd {
                CanioCmd::UdsCmdNoResponse(msg) => {
                    let _ = self.uds_send(&msg, 5);
                }
                CanioCmd::UdsCmdWithResponse {
                    buf,
                    resp_channel,
                    timeout_ms,
                } => {
                    let resp = self.uds_send_recv(&buf, 5, timeout_ms);
                    match resp {
                        Err(e) => {
                            resp_channel.send(None);
                        },
                        Ok(b) => {
                            let _ = resp_channel.send(Some(b));
                        }
                    }
                }
            }
        } else {
            tokio::time::sleep(std::time::Duration::from_millis(1)).await;
        }

        Ok(())
    }
}

impl std::fmt::Display for CANIO<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Request ID: {:02x?}\nResponse ID: {:02x?}", self.ids.tx, self.ids.rx)
    }
}
