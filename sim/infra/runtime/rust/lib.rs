pub mod app {
    include!(env!("RIG_RUNTIME_RUST_APP_RS"));
}

pub mod can {
    include!(env!("RIG_RUNTIME_RUST_CAN_RS"));
}

mod ffi {
    include!(env!("RIG_RUNTIME_RUST_FFI_RS"));
}

mod core {
    include!(env!("RIG_RUNTIME_RUST_CORE_RS"));
}

pub mod datapath {
    include!(env!("RIG_RUNTIME_RUST_DATAPATH_RS"));
}

mod faults {
    include!(env!("RIG_RUNTIME_RUST_FAULTS_RS"));
}

mod io {
    include!(env!("RIG_RUNTIME_RUST_IO_RS"));
}

pub mod model {
    include!(env!("RIG_RUNTIME_RUST_MODEL_RS"));
}

pub mod module_desc {
    include!(env!("RIG_RUNTIME_RUST_MODULE_DESC_RS"));
}

pub mod nvm {
    include!(env!("RIG_RUNTIME_RUST_NVM_RS"));
}

pub mod rt_controller {
    include!(env!("RIG_RUNTIME_RUST_RT_CONTROLLER_RS"));
}

pub mod spi {
    include!(env!("RIG_RUNTIME_RUST_SPI_RS"));
}

pub mod timer {
    include!(env!("RIG_RUNTIME_RUST_TIMER_RS"));
}

pub use app::AppDesc;
pub use can::{
    CanEnumValueDescriptor, CanEvent, CanMessageDescriptor, CanNetwork, CanPacket,
    CanSignalDescriptor,
};
pub use model::{NodeModel, NodeTarget};
pub use module_desc::{ModuleDesc, ModuleTask};
pub use rt_controller::{
    PeriodicTask, RTController, Scheduler, TaskCallbacks, TaskFn, MAX_PERIODIC_TASKS,
};
use std::ffi::CStr;
use std::os::raw::c_char;
use std::ptr;

unsafe fn write_str(value: &str, out: *mut c_char, out_len: usize) -> bool {
    if out.is_null() || out_len == 0 || value.len() + 1 > out_len {
        return false;
    }

    unsafe {
        ptr::copy_nonoverlapping(value.as_ptr(), out.cast::<u8>(), value.len());
        *out.add(value.len()) = 0;
    }
    true
}

unsafe fn c_str_to_str<'a>(value: *const c_char) -> Option<&'a str> {
    if value.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(value) }.to_str().ok()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_set_analog_input(channel: i32, voltage: f32) {
    io::set_analog_input(channel, voltage);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_get_analog_input(channel: i32) -> f32 {
    io::get_analog_input(channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_set_digital_io(channel: i32, state: bool) {
    io::set_digital(channel, state);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_get_digital_io(channel: i32) -> bool {
    io::get_digital(channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_get_fault(fault: i32) -> bool {
    faults::get(fault)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_bus_count() -> u8 {
    can::bus_count()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_send(bus: u8, packet: *const CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }
    can::send(bus, unsafe { &*packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_recv(bus: u8, packet: *mut CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }

    match can::recv(bus) {
        Some(next) => {
            unsafe { *packet = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_recv_event(bus: u8, event: *mut CanEvent) -> bool {
    if event.is_null() {
        return false;
    }

    match can::recv_event(bus) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_rx_count(bus: u8) -> u32 {
    can::rx_count(bus)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_tx_count(bus: u8) -> u32 {
    can::tx_count(bus)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_send_duty(event: *const timer::TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    timer::push_duty_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_recv_duty(
    port: i32,
    channel: i32,
    event: *mut timer::TimerChannelEvent,
) -> bool {
    if event.is_null() {
        return false;
    }
    match timer::pop_duty_output(port, channel) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_duty_output_count(port: i32, channel: i32) -> u32 {
    timer::duty_output_count(port, channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_send_frequency(event: *const timer::TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    timer::push_frequency_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_recv_frequency(
    port: i32,
    channel: i32,
    event: *mut timer::TimerChannelEvent,
) -> bool {
    if event.is_null() {
        return false;
    }
    match timer::pop_frequency_output(port, channel) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_frequency_output_count(port: i32, channel: i32) -> u32 {
    timer::frequency_output_count(port, channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_timer_send_capture(event: *const timer::TimerCaptureEvent) -> bool {
    if event.is_null() {
        return false;
    }
    timer::push_capture_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_spi_send(transaction: *const spi::SpiTransaction) -> bool {
    if transaction.is_null() {
        return false;
    }
    spi::push_input(unsafe { *transaction })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_spi_recv(device: i32, transaction: *mut spi::SpiTransaction) -> bool {
    if transaction.is_null() {
        return false;
    }
    match spi::pop_output(device) {
        Some(next) => {
            unsafe { *transaction = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_spi_output_count(device: i32) -> u32 {
    spi::output_count(device)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_bus_count() -> u8 {
    can::codegen_bus_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_bus_name(
    bus: u8,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_bus_name(bus) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_message_count() -> u32 {
    can::codegen_message_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_message_descriptor(
    index: u32,
    out: *mut CanMessageDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = can::codegen_message_descriptor(index) else {
        return false;
    };
    unsafe { *out = descriptor };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_message_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_message_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_tx_message_count() -> u32 {
    can::codegen_tx_message_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_message_descriptor(
    index: u32,
    out: *mut CanMessageDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = can::codegen_tx_message_descriptor(index) else {
        return false;
    };
    unsafe { *out = descriptor };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_message_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_tx_message_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_signal_count() -> u32 {
    can::codegen_signal_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_descriptor(
    index: u32,
    out: *mut CanSignalDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = can::codegen_signal_descriptor(index) else {
        return false;
    };
    unsafe { *out = descriptor };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_message_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_signal_message_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_signal_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_unit(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_signal_unit(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_enum_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_signal_enum_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_tx_signal_count() -> u32 {
    can::codegen_tx_signal_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_descriptor(
    index: u32,
    out: *mut CanSignalDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = can::codegen_tx_signal_descriptor(index) else {
        return false;
    };
    unsafe { *out = descriptor };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_message_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_tx_signal_message_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_tx_signal_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_unit(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_tx_signal_unit(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_enum_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_tx_signal_enum_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_enum_value_count() -> u32 {
    can::codegen_enum_value_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_enum_value_descriptor(
    index: u32,
    out: *mut CanEnumValueDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = can::codegen_enum_value_descriptor(index) else {
        return false;
    };
    unsafe { *out = descriptor };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_enum_value_enum_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_enum_value_enum_name(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_enum_value_label(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = can::codegen_enum_value_label(index) else {
        return false;
    };
    unsafe { write_str(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_decode_signal(
    bus: u8,
    packet: *const CanPacket,
    signal_name: *const c_char,
    value_out: *mut f64,
) -> bool {
    if packet.is_null() || value_out.is_null() {
        return false;
    }
    let Some(signal_name) = (unsafe { c_str_to_str(signal_name) }) else {
        return false;
    };
    let Some(value) = can::decode_signal(bus, unsafe { &*packet }, signal_name) else {
        return false;
    };
    unsafe { *value_out = value };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_encode_signal(
    bus: u8,
    message_name: *const c_char,
    signal_name: *const c_char,
    value: f64,
    packet: *mut CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    let Some(message_name) = (unsafe { c_str_to_str(message_name) }) else {
        return false;
    };
    let Some(signal_name) = (unsafe { c_str_to_str(signal_name) }) else {
        return false;
    };
    can::encode_signal(bus, message_name, signal_name, value, unsafe {
        &mut *packet
    })
}
