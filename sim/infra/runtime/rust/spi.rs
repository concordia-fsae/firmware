use std::sync::{LazyLock, Mutex};

use super::datapath::{DataPath, DataPathEvent};
use super::io;

pub const RIG_SPI_TRANSACTION_MAX_BYTES: usize = 256;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct SpiDevice {
    pub device: i32,
}

pub type SpiResponseFn =
    unsafe extern "C" fn(transaction: *const SpiTransaction, response: *mut SpiTransaction) -> bool;

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
    chip_select_pin: Option<i32>,
    inputs: DataPath<SpiTransaction>,
    outputs: DataPath<SpiTransaction>,
    responder: Option<SpiResponseFn>,
}

impl SpiPeripheral {
    fn new(device: SpiDevice) -> Self {
        Self {
            device,
            chip_select_pin: None,
            inputs: DataPath::new(device),
            outputs: DataPath::new(device),
            responder: None,
        }
    }

    fn reset(&mut self) {
        self.inputs.clear();
        self.outputs.clear();
        if let Some(chip_select_pin) = self.chip_select_pin {
            io::set_digital(chip_select_pin, true);
        }
    }

    fn push_response_for(&mut self, transaction: SpiTransaction) {
        if transaction.rx_len == 0
            || self.inputs.count() > 0
            || self.device != transaction.spi_device()
        {
            return;
        }

        let Some(responder) = self.responder else {
            return;
        };
        let mut response = SpiTransaction::default();
        if !unsafe { responder(&transaction, &mut response) } {
            return;
        }
        let _ = self.inputs.push(response);
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

    fn reset_chip_selects(&self) {
        for peripheral in &self.peripherals {
            if let Some(chip_select_pin) = peripheral.chip_select_pin {
                io::set_digital(chip_select_pin, true);
            }
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

    fn selected_device(&self) -> Result<Option<SpiDevice>, ()> {
        let mut selected = None;
        for peripheral in &self.peripherals {
            let Some(chip_select_pin) = peripheral.chip_select_pin else {
                continue;
            };
            if io::get_digital(chip_select_pin) {
                continue;
            }
            if selected.is_some() {
                return Err(());
            }
            selected = Some(peripheral.device);
        }
        Ok(selected)
    }
}

static SPI_MODEL: LazyLock<Mutex<SpiModel>> = LazyLock::new(|| Mutex::new(SpiModel::default()));

pub fn reset() {
    SPI_MODEL.lock().unwrap().reset();
}

pub fn reset_chip_selects() {
    SPI_MODEL.lock().unwrap().reset_chip_selects();
}

pub fn configure_device(device: i32) {
    SPI_MODEL.lock().unwrap().peripheral(SpiDevice { device });
}

pub fn configure_device_chip_select(device: i32, chip_select_pin: i32) {
    let mut spi = SPI_MODEL.lock().unwrap();
    spi.peripheral(SpiDevice { device }).chip_select_pin = Some(chip_select_pin);
    io::set_digital(chip_select_pin, true);
}

pub fn lock_device(device: i32) -> bool {
    let mut spi = SPI_MODEL.lock().unwrap();
    match spi.selected_device() {
        Ok(Some(selected_device)) if selected_device.device != device => return false,
        Err(()) => return false,
        _ => {}
    }
    let peripheral = spi.peripheral(SpiDevice { device });
    let Some(chip_select_pin) = peripheral.chip_select_pin else {
        return false;
    };
    io::set_digital(chip_select_pin, false);
    true
}

pub fn release_device(device: i32) -> bool {
    let mut spi = SPI_MODEL.lock().unwrap();
    let peripheral = spi.peripheral(SpiDevice { device });
    let Some(chip_select_pin) = peripheral.chip_select_pin else {
        return false;
    };
    io::set_digital(chip_select_pin, true);
    true
}

pub fn configure_responder(device: i32, responder: Option<SpiResponseFn>) {
    let mut spi = SPI_MODEL.lock().unwrap();
    spi.peripheral(SpiDevice { device }).responder = responder;
}

pub fn push_input(transaction: SpiTransaction) -> bool {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(transaction.spi_device())
        .inputs
        .push(transaction)
}

pub fn push_inputs(transactions: &[SpiTransaction]) -> u32 {
    let mut spi = SPI_MODEL.lock().unwrap();
    let mut count = 0;
    for transaction in transactions {
        if spi
            .peripheral(transaction.spi_device())
            .inputs
            .push(*transaction)
        {
            count += 1;
        }
    }
    count
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
    let mut spi = SPI_MODEL.lock().unwrap();
    let Ok(selected_device) = spi.selected_device() else {
        return false;
    };
    if selected_device != Some(transaction.spi_device()) {
        return spi.peripheral(transaction.spi_device()).outputs.push(transaction);
    }
    let peripheral = spi.peripheral(transaction.spi_device());
    peripheral.push_response_for(transaction);
    peripheral.outputs.push(transaction)
}

pub fn pop_output(device: i32) -> Option<SpiTransaction> {
    SPI_MODEL
        .lock()
        .unwrap()
        .peripheral(SpiDevice { device })
        .outputs
        .pop()
}

pub fn pop_outputs(device: i32, out: &mut [SpiTransaction]) -> u32 {
    let mut spi = SPI_MODEL.lock().unwrap();
    let output = &mut spi.peripheral(SpiDevice { device }).outputs;
    let mut count = 0;
    for slot in out.iter_mut() {
        let Some(transaction) = output.pop() else {
            break;
        };
        *slot = transaction;
        count += 1;
    }
    count
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
pub extern "C" fn rig_runtime_spi_configure_device_chip_select(
    device: i32,
    chip_select_pin: i32,
) {
    configure_device_chip_select(device, chip_select_pin);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_configure_responder(
    device: i32,
    responder: Option<SpiResponseFn>,
) {
    configure_responder(device, responder);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_lock_device(device: i32) -> bool {
    lock_device(device)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_release_device(device: i32) -> bool {
    release_device(device)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_push_inputs(
    transactions: *const SpiTransaction,
    count: u32,
) -> u32 {
    if transactions.is_null() {
        return 0;
    }
    let transactions = unsafe { std::slice::from_raw_parts(transactions, count as usize) };
    push_inputs(transactions)
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
pub extern "C" fn rig_runtime_spi_pop_outputs(
    device: i32,
    transactions: *mut SpiTransaction,
    capacity: u32,
) -> u32 {
    if transactions.is_null() {
        return 0;
    }
    let transactions = unsafe { std::slice::from_raw_parts_mut(transactions, capacity as usize) };
    pop_outputs(device, transactions)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_spi_output_count(device: i32) -> u32 {
    output_count(device)
}
