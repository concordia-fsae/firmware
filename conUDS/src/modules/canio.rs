use std::marker::PhantomData;

/// CANIO module
use anyhow::{anyhow, Result};

use ecu_diagnostics::hardware::{socketcan::SocketCanScanner, Hardware, HardwareScanner};
use ecu_diagnostics::channel::{IsoTPChannel, IsoTPSettings};
use ecu_diagnostics::uds::UDSProtocol;
use ecu_diagnostics::dynamic_diag::DiagProtocol;

pub struct CANIO<'a> {
    channel: Box<dyn IsoTPChannel>,
    _marker: PhantomData<&'a str>
}

impl<'a> CANIO<'_> {
    pub fn new(send_id: u32, recv_id: u32) -> Result<Self> {
        let scanner = SocketCanScanner::new();
        let devices = scanner.list_devices();

        if devices.len() == 0 {
            return Err(anyhow!("No CAN devices detected"));
        }
        println!("Devices: {:#?}", devices);

        let mut device = scanner.open_device_by_name("can0").unwrap();
        println!("opened device: {:#?}", device);

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
            channel,
            _marker: PhantomData,
        })
    }
}

pub fn process() -> Result<()> {
    let mut canio = CANIO::new(0x456, 0x123)?;

    canio.channel.write_bytes(0x456, None, &UDSProtocol::create_tp_msg(true).to_bytes(), 100)?;

    Ok(())
}
