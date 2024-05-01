/// CANIO module
use anyhow::{anyhow, Result};

use ecu_diagnostics::channel::{ChannelResult, IsoTPChannel, IsoTPSettings};
use ecu_diagnostics::hardware::{socketcan::SocketCanScanner, Hardware, HardwareScanner};
use tokio::sync::mpsc::Receiver;

use crate::CanioCmd;

struct IDs {
    tx: u32,
    #[allow(dead_code)]
    rx: u32,
}

pub struct CANIO<'a> {
    ids: IDs,
    channel: Box<dyn IsoTPChannel>,
    uds_queue: &'a mut Receiver<CanioCmd>,
}

impl<'a> CANIO<'a> {
    pub fn new(send_id: u32, recv_id: u32, uds_queue: &'a mut Receiver<CanioCmd>) -> Result<Self> {
        let scanner = SocketCanScanner::new();
        let devices = scanner.list_devices();

        if devices.is_empty() {
            return Err(anyhow!("No CAN devices detected"));
        }
        // println!("Devices: {:#?}", devices);

        let mut device = scanner.open_device_by_name("can0").unwrap();
        // println!("opened device: {:#?}", device);

        let mut channel = device.create_iso_tp_channel().unwrap();

        let iso_tp_cfg = IsoTPSettings {
            block_size: 5,
            st_min: 5,
            extended_addresses: None,
            pad_frame: false,
            can_speed: 1000000,
            can_use_ext_addr: false,
        };

        channel.set_iso_tp_cfg(iso_tp_cfg)?;
        channel.set_ids(send_id, recv_id)?;
        channel.open()?;

        Ok(Self {
            ids: IDs {
                tx: send_id,
                rx: recv_id,
            },
            channel,
            uds_queue,
        })
    }

    fn uds_send(&mut self, buf: &[u8], timeout_ms: u32) -> ChannelResult<()> {
        self.channel.write_bytes(self.ids.tx, None, buf, timeout_ms)
    }

    #[allow(dead_code)]
    fn uds_recv(&mut self, timeout_ms: u32) -> ChannelResult<Vec<u8>> {
        self.channel.read_bytes(timeout_ms)
    }

    #[allow(dead_code)]
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
                    // println!("msg received from channel {:#?}", msg);
                    let _ = self.uds_send(&msg, 5);
                    // println!("result: {:#?}", res);
                }
                CanioCmd::UdsCmdWithResponse(msg, resp_channel) => {
                    match self.uds_send_recv(&msg, 5, 5) {
                        Ok(resp) => {
                            let _ = resp_channel.send(resp);
                        }
                        Err(_) => {}
                    }
                }
            }
        }

        Ok(())
    }
}
