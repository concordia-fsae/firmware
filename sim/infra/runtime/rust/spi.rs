use std::sync::{LazyLock, Mutex};

use super::datapath::{DataPath, DataPathEvent};

pub const RIG_SPI_TRANSACTION_MAX_BYTES: usize = 256;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct SpiDevice {
    pub device: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SpiTransaction {
    pub device: i32,
    pub tx_len: u16,
    pub rx_len: u16,
    pub tx_data: [u8; RIG_SPI_TRANSACTION_MAX_BYTES],
    pub rx_data: [u8; RIG_SPI_TRANSACTION_MAX_BYTES],
    pub timestamp_ns: u64,
}

impl Default for SpiTransaction {
    fn default() -> Self {
        Self {
            device: 0,
            tx_len: 0,
            rx_len: 0,
            tx_data: [0; RIG_SPI_TRANSACTION_MAX_BYTES],
            rx_data: [0; RIG_SPI_TRANSACTION_MAX_BYTES],
            timestamp_ns: 0,
        }
    }
}

impl SpiTransaction {
    fn spi_device(&self) -> SpiDevice {
        SpiDevice {
            device: self.device,
        }
    }
}

impl DataPathEvent for SpiTransaction {
    type Channel = SpiDevice;

    fn channel(&self) -> Self::Channel {
        self.spi_device()
    }
}

#[derive(Debug)]
struct SpiPeripheral {
    device: SpiDevice,
    inputs: DataPath<SpiTransaction>,
    outputs: DataPath<SpiTransaction>,
}

impl SpiPeripheral {
    fn new(device: SpiDevice) -> Self {
        Self {
            device,
            inputs: DataPath::new(device),
            outputs: DataPath::new(device),
        }
    }

    fn reset(&mut self) {
        self.inputs.clear();
        self.outputs.clear();
    }
}

#[derive(Debug, Default)]
struct SpiModel {
    peripherals: Vec<SpiPeripheral>,
}

impl SpiModel {
    fn reset(&mut self) {
        for peripheral in &mut self.peripherals {
            peripheral.reset();
        }
    }

    fn peripheral(&mut self, device: SpiDevice) -> &mut SpiPeripheral {
        if let Some(index) = self
            .peripherals
            .iter()
            .position(|peripheral| peripheral.device == device)
        {
            return &mut self.peripherals[index];
        }

        self.peripherals.push(SpiPeripheral::new(device));
        self.peripherals.last_mut().unwrap()
    }
}

static SPI_MODEL: LazyLock<Mutex<SpiModel>> = LazyLock::new(|| Mutex::new(SpiModel::default()));

pub fn reset() {
    SPI_MODEL.lock().unwrap().reset();
}

pub fn configure_device(device: i32) {
    SPI_MODEL.lock().unwrap().peripheral(SpiDevice { device });
}

pub fn push_input(transaction: SpiTransaction) -> bool {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(transaction.spi_device())
        .inputs
        .push(transaction)
}

pub fn pop_input(device: i32) -> Option<SpiTransaction> {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(SpiDevice { device })
        .inputs
        .pop()
}

pub fn push_output(transaction: SpiTransaction) -> bool {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(transaction.spi_device())
        .outputs
        .push(transaction)
}

pub fn pop_output(device: i32) -> Option<SpiTransaction> {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(SpiDevice { device })
        .outputs
        .pop()
}

pub fn output_count(device: i32) -> u32 {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(SpiDevice { device })
        .outputs
        .count()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_push_input(transaction: *const SpiTransaction) -> bool {
    if transaction.is_null() {
        return false;
    }
    push_input(unsafe { *transaction })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_pop_input(device: i32, transaction: *mut SpiTransaction) -> bool {
    if transaction.is_null() {
        return false;
    }
    match pop_input(device) {
        Some(next) => {
            unsafe { *transaction = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_push_output(transaction: *const SpiTransaction) -> bool {
    if transaction.is_null() {
        return false;
    }
    push_output(unsafe { *transaction })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_pop_output(
    device: i32,
    transaction: *mut SpiTransaction,
) -> bool {
    if transaction.is_null() {
        return false;
    }
    match pop_output(device) {
        Some(next) => {
            unsafe { *transaction = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_output_count(device: i32) -> u32 {
    output_count(device)
}
