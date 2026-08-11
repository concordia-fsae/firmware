use std::collections::VecDeque;
use std::sync::{LazyLock, Mutex};

unsafe extern "C" {
    fn rig_runtime_can_notify_rx(bus: u8);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanPacket {
    pub id: u32,
    pub len: u8,
    pub data: [u8; 8],
}

impl CanPacket {
    pub fn new(id: u32, data: [u8; 8], len: u8) -> Self {
        Self { id, len, data }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanEvent {
    pub bus: u8,
    pub timestamp_ns: u64,
    pub packet: CanPacket,
}

#[derive(Default)]
struct CanRuntime {
    rx: Vec<VecDeque<CanPacket>>,
    tx: Vec<VecDeque<CanEvent>>,
    network: Option<CanNetwork>,
}

impl CanRuntime {
    fn configure(&mut self, bus_count: u8) {
        self.rx = vec![VecDeque::new(); bus_count as usize];
        self.tx = vec![VecDeque::new(); bus_count as usize];
    }

    fn reset(&mut self) {
        self.rx.clear();
        self.tx.clear();
        self.network = None;
    }

    fn bus_count(&self) -> u8 {
        self.rx.len() as u8
    }

    fn push_rx(&mut self, bus: u8, packet: CanPacket) -> bool {
        self.rx
            .get_mut(bus as usize)
            .map(|queue| queue.push_back(packet))
            .is_some()
    }

    fn push_rx_many(&mut self, bus: u8, packets: &[CanPacket]) -> u32 {
        let Some(queue) = self.rx.get_mut(bus as usize) else {
            return 0;
        };
        let count = packets.len().min(u32::MAX as usize);
        queue.extend(packets.iter().copied().take(count));
        count as u32
    }

    fn pop_rx(&mut self, bus: u8) -> Option<CanPacket> {
        self.rx.get_mut(bus as usize).and_then(VecDeque::pop_front)
    }

    fn push_tx(&mut self, bus: u8, packet: CanPacket, timestamp_ns: u64) -> bool {
        self.tx
            .get_mut(bus as usize)
            .map(|queue| {
                queue.push_back(CanEvent {
                    bus,
                    timestamp_ns,
                    packet,
                })
            })
            .is_some()
    }

    fn pop_tx(&mut self, bus: u8) -> Option<CanEvent> {
        self.tx.get_mut(bus as usize).and_then(VecDeque::pop_front)
    }

    fn pop_tx_many(&mut self, bus: u8, out: &mut [CanEvent]) -> u32 {
        let Some(queue) = self.tx.get_mut(bus as usize) else {
            return 0;
        };
        let mut count = 0;
        for slot in out.iter_mut() {
            let Some(event) = queue.pop_front() else {
                break;
            };
            *slot = event;
            count += 1;
        }
        count
    }

    fn rx_count(&self, bus: u8) -> u32 {
        self.rx
            .get(bus as usize)
            .map(VecDeque::len)
            .unwrap_or_default() as u32
    }

    fn tx_count(&self, bus: u8) -> u32 {
        self.tx
            .get(bus as usize)
            .map(VecDeque::len)
            .unwrap_or_default() as u32
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanMessageDescriptor {
    pub bus: u8,
    pub id: u32,
    pub len: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanSignalDescriptor {
    pub bus: u8,
    pub message_id: u32,
    pub kind: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanEnumValueDescriptor {
    pub raw: i32,
}

pub type CanBusNameFn = fn(u8) -> Option<&'static str>;
pub type CanMessageCountFn = fn() -> u32;
pub type CanMessageDescriptorFn = fn(u32) -> Option<CanMessageDescriptor>;
pub type CanMessageNameFn = fn(u32) -> Option<&'static str>;
pub type CanSignalCountFn = fn() -> u32;
pub type CanSignalDescriptorFn = fn(u32) -> Option<CanSignalDescriptor>;
pub type CanSignalStringFn = fn(u32) -> Option<&'static str>;
pub type CanEnumValueCountFn = fn() -> u32;
pub type CanEnumValueDescriptorFn = fn(u32) -> Option<CanEnumValueDescriptor>;
pub type CanEnumValueStringFn = fn(u32) -> Option<&'static str>;
pub type CanDecodeSignalFn = fn(u8, &CanPacket, &str) -> Option<f64>;
pub type CanEncodeSignalFn = fn(u8, &str, &str, f64, &mut CanPacket) -> bool;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CanSignalValue {
    pub value: f64,
}

#[derive(Clone, Copy)]
pub struct CanNetwork {
    pub bus_count: fn() -> u8,
    pub bus_name: CanBusNameFn,
    pub message_count: CanMessageCountFn,
    pub message_descriptor: CanMessageDescriptorFn,
    pub message_name: CanMessageNameFn,
    pub tx_message_count: CanMessageCountFn,
    pub tx_message_descriptor: CanMessageDescriptorFn,
    pub tx_message_name: CanMessageNameFn,
    pub signal_count: CanSignalCountFn,
    pub signal_descriptor: CanSignalDescriptorFn,
    pub signal_message_name: CanSignalStringFn,
    pub signal_name: CanSignalStringFn,
    pub signal_unit: CanSignalStringFn,
    pub signal_enum_name: CanSignalStringFn,
    pub tx_signal_count: CanSignalCountFn,
    pub tx_signal_descriptor: CanSignalDescriptorFn,
    pub tx_signal_message_name: CanSignalStringFn,
    pub tx_signal_name: CanSignalStringFn,
    pub tx_signal_unit: CanSignalStringFn,
    pub tx_signal_enum_name: CanSignalStringFn,
    pub enum_value_count: CanEnumValueCountFn,
    pub enum_value_descriptor: CanEnumValueDescriptorFn,
    pub enum_value_enum_name: CanEnumValueStringFn,
    pub enum_value_label: CanEnumValueStringFn,
    pub decode_signal: CanDecodeSignalFn,
    pub encode_signal: CanEncodeSignalFn,
}

static CAN_RUNTIME: LazyLock<Mutex<CanRuntime>> =
    LazyLock::new(|| Mutex::new(CanRuntime::default()));

pub fn configure(bus_count: u8) {
    CAN_RUNTIME.lock().unwrap().configure(bus_count);
}

pub fn configure_network(network: CanNetwork) {
    let mut runtime = CAN_RUNTIME.lock().unwrap();
    runtime.configure((network.bus_count)());
    runtime.network = Some(network);
}

pub fn reset() {
    CAN_RUNTIME.lock().unwrap().reset();
}

pub fn bus_count() -> u8 {
    CAN_RUNTIME.lock().unwrap().bus_count()
}

pub fn send(bus: u8, packet: &CanPacket) -> bool {
    if CAN_RUNTIME.lock().unwrap().push_rx(bus, *packet) {
        unsafe { rig_runtime_can_notify_rx(bus) };
        true
    } else {
        false
    }
}

pub fn send_many(bus: u8, packets: &[CanPacket]) -> u32 {
    let count = CAN_RUNTIME.lock().unwrap().push_rx_many(bus, packets);
    if count > 0 {
        unsafe { rig_runtime_can_notify_rx(bus) };
    }
    count
}

pub fn recv_event(bus: u8) -> Option<CanEvent> {
    CAN_RUNTIME.lock().unwrap().pop_tx(bus)
}

pub fn recv_events(bus: u8, out: &mut [CanEvent]) -> u32 {
    CAN_RUNTIME.lock().unwrap().pop_tx_many(bus, out)
}

pub fn recv(bus: u8) -> Option<CanPacket> {
    recv_event(bus).map(|event| event.packet)
}

pub fn rx_count(bus: u8) -> u32 {
    CAN_RUNTIME.lock().unwrap().rx_count(bus)
}

pub fn tx_count(bus: u8) -> u32 {
    CAN_RUNTIME.lock().unwrap().tx_count(bus)
}

fn with_network<T>(default: T, f: impl FnOnce(CanNetwork) -> T) -> T {
    CAN_RUNTIME
        .lock()
        .unwrap()
        .network
        .map(f)
        .unwrap_or(default)
}

pub fn codegen_bus_count() -> u8 {
    with_network(0, |network| (network.bus_count)())
}

pub fn codegen_bus_name(bus: u8) -> Option<&'static str> {
    with_network(None, |network| (network.bus_name)(bus))
}

pub fn codegen_message_count() -> u32 {
    with_network(0, |network| (network.message_count)())
}

pub fn codegen_message_descriptor(index: u32) -> Option<CanMessageDescriptor> {
    with_network(None, |network| (network.message_descriptor)(index))
}

pub fn codegen_message_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.message_name)(index))
}

pub fn codegen_tx_message_count() -> u32 {
    with_network(0, |network| (network.tx_message_count)())
}

pub fn codegen_tx_message_descriptor(index: u32) -> Option<CanMessageDescriptor> {
    with_network(None, |network| (network.tx_message_descriptor)(index))
}

pub fn codegen_tx_message_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.tx_message_name)(index))
}

pub fn codegen_signal_count() -> u32 {
    with_network(0, |network| (network.signal_count)())
}

pub fn codegen_signal_descriptor(index: u32) -> Option<CanSignalDescriptor> {
    with_network(None, |network| (network.signal_descriptor)(index))
}

pub fn codegen_signal_message_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.signal_message_name)(index))
}

pub fn codegen_signal_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.signal_name)(index))
}

pub fn codegen_signal_unit(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.signal_unit)(index))
}

pub fn codegen_signal_enum_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.signal_enum_name)(index))
}

pub fn codegen_tx_signal_count() -> u32 {
    with_network(0, |network| (network.tx_signal_count)())
}

pub fn codegen_tx_signal_descriptor(index: u32) -> Option<CanSignalDescriptor> {
    with_network(None, |network| (network.tx_signal_descriptor)(index))
}

pub fn codegen_tx_signal_message_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.tx_signal_message_name)(index))
}

pub fn codegen_tx_signal_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.tx_signal_name)(index))
}

pub fn codegen_tx_signal_unit(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.tx_signal_unit)(index))
}

pub fn codegen_tx_signal_enum_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.tx_signal_enum_name)(index))
}

pub fn codegen_enum_value_count() -> u32 {
    with_network(0, |network| (network.enum_value_count)())
}

pub fn codegen_enum_value_descriptor(index: u32) -> Option<CanEnumValueDescriptor> {
    with_network(None, |network| (network.enum_value_descriptor)(index))
}

pub fn codegen_enum_value_enum_name(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.enum_value_enum_name)(index))
}

pub fn codegen_enum_value_label(index: u32) -> Option<&'static str> {
    with_network(None, |network| (network.enum_value_label)(index))
}

pub fn decode_signal(bus: u8, packet: &CanPacket, signal_name: &str) -> Option<f64> {
    with_network(None, |network| {
        (network.decode_signal)(bus, packet, signal_name)
    })
}

pub fn decode_signals(
    bus: u8,
    packet: &CanPacket,
    signal_names: &[&str],
    values: &mut [CanSignalValue],
) -> u32 {
    with_network(0, |network| {
        let count = signal_names.len().min(values.len()).min(u32::MAX as usize);
        let mut decoded = 0;
        for (signal_name, value) in signal_names.iter().zip(values.iter_mut()).take(count) {
            let Some(decoded_value) = (network.decode_signal)(bus, packet, signal_name) else {
                break;
            };
            value.value = decoded_value;
            decoded += 1;
        }
        decoded
    })
}

pub fn encode_signal(
    bus: u8,
    message_name: &str,
    signal_name: &str,
    value: f64,
    packet: &mut CanPacket,
) -> bool {
    with_network(false, |network| {
        (network.encode_signal)(bus, message_name, signal_name, value, packet)
    })
}

pub fn encode_signals(
    bus: u8,
    message_name: &str,
    signal_names: &[&str],
    values: &[CanSignalValue],
    packet: &mut CanPacket,
) -> u32 {
    with_network(0, |network| {
        let count = signal_names.len().min(values.len()).min(u32::MAX as usize);
        let mut encoded = 0;
        for (signal_name, value) in signal_names.iter().zip(values.iter()).take(count) {
            if !(network.encode_signal)(bus, message_name, signal_name, value.value, packet) {
                break;
            }
            encoded += 1;
        }
        encoded
    })
}

#[macro_export]
macro_rules! rig_yamcan_network {
    ($network:ident, $model:ident, $decode:ident, $yamcan:ident) => {
        const $network: $crate::rig_runtime::CanNetwork = $crate::rig_runtime::CanNetwork {
            bus_count: rig_can_bus_count,
            bus_name: rig_can_bus_name,
            message_count: rig_can_message_count,
            message_descriptor: rig_can_message_descriptor,
            message_name: rig_can_message_name,
            tx_message_count: rig_can_tx_message_count,
            tx_message_descriptor: rig_can_tx_message_descriptor,
            tx_message_name: rig_can_tx_message_name,
            signal_count: rig_can_signal_count,
            signal_descriptor: rig_can_signal_descriptor,
            signal_message_name: rig_can_signal_message_name,
            signal_name: rig_can_signal_name,
            signal_unit: rig_can_signal_unit,
            signal_enum_name: rig_can_signal_enum_name,
            tx_signal_count: rig_can_tx_signal_count,
            tx_signal_descriptor: rig_can_tx_signal_descriptor,
            tx_signal_message_name: rig_can_tx_signal_message_name,
            tx_signal_name: rig_can_tx_signal_name,
            tx_signal_unit: rig_can_tx_signal_unit,
            tx_signal_enum_name: rig_can_tx_signal_enum_name,
            enum_value_count: rig_can_enum_value_count,
            enum_value_descriptor: rig_can_enum_value_descriptor,
            enum_value_enum_name: rig_can_enum_value_enum_name,
            enum_value_label: rig_can_enum_value_label,
            decode_signal: rig_can_decode_signal,
            encode_signal: rig_can_encode_signal,
        };

        fn rig_bus_from_raw(bus: u8) -> Option<$model::Bus> {
            <$decode::GeneratedNetwork as $yamcan::NetworkDecoder>::buses()
                .get(bus as usize)
                .map(|desc| desc.name)
        }

        fn rig_bus_to_raw(bus: $model::Bus) -> Option<u8> {
            <$decode::GeneratedNetwork as $yamcan::NetworkDecoder>::buses()
                .iter()
                .position(|desc| desc.name == bus)
                .map(|index| index as u8)
        }

        fn rig_signal_kind_to_raw(kind: $yamcan::SignalKind) -> u8 {
            match kind {
                $yamcan::SignalKind::Numeric => 0,
                $yamcan::SignalKind::Boolean => 1,
                $yamcan::SignalKind::Enum => 2,
            }
        }

        fn rig_can_bus_count() -> u8 {
            <$decode::GeneratedNetwork as $yamcan::NetworkDecoder>::buses().len() as u8
        }

        fn rig_can_bus_name(bus: u8) -> Option<&'static str> {
            rig_bus_from_raw(bus).map(<$model::Bus as $yamcan::NetworkBus>::as_str)
        }

        fn rig_can_message_count() -> u32 {
            $model::message_descriptors().len() as u32
        }

        fn rig_can_message_descriptor(
            index: u32,
        ) -> Option<$crate::rig_runtime::CanMessageDescriptor> {
            let Some(message) = $model::message_descriptors().get(index as usize) else {
                return None;
            };
            let Some(bus) = rig_bus_to_raw(message.bus) else {
                return None;
            };

            Some($crate::rig_runtime::CanMessageDescriptor {
                bus,
                id: message.id,
                len: message.len,
            })
        }

        fn rig_can_message_name(index: u32) -> Option<&'static str> {
            $model::message_descriptors()
                .get(index as usize)
                .map(|message| message.name)
        }

        fn rig_can_tx_message_count() -> u32 {
            $model::tx_message_descriptors().len() as u32
        }

        fn rig_can_tx_message_descriptor(
            index: u32,
        ) -> Option<$crate::rig_runtime::CanMessageDescriptor> {
            let Some(message) = $model::tx_message_descriptors().get(index as usize) else {
                return None;
            };
            let Some(bus) = rig_bus_to_raw(message.bus) else {
                return None;
            };

            Some($crate::rig_runtime::CanMessageDescriptor {
                bus,
                id: message.id,
                len: message.len,
            })
        }

        fn rig_can_tx_message_name(index: u32) -> Option<&'static str> {
            $model::tx_message_descriptors()
                .get(index as usize)
                .map(|message| message.name)
        }

        fn rig_can_signal_count() -> u32 {
            $model::signal_descriptors().len() as u32
        }

        fn rig_can_signal_descriptor(
            index: u32,
        ) -> Option<$crate::rig_runtime::CanSignalDescriptor> {
            let Some(signal) = $model::signal_descriptors().get(index as usize) else {
                return None;
            };
            let Some(bus) = rig_bus_to_raw(signal.bus) else {
                return None;
            };

            Some($crate::rig_runtime::CanSignalDescriptor {
                bus,
                message_id: signal.message_id,
                kind: rig_signal_kind_to_raw(signal.kind),
            })
        }

        fn rig_can_signal_message_name(index: u32) -> Option<&'static str> {
            $model::signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.message_name)
        }

        fn rig_can_signal_name(index: u32) -> Option<&'static str> {
            $model::signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.signal_name)
        }

        fn rig_can_signal_unit(index: u32) -> Option<&'static str> {
            $model::signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.unit.unwrap_or(""))
        }

        fn rig_can_signal_enum_name(index: u32) -> Option<&'static str> {
            $model::signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.enum_name.unwrap_or(""))
        }

        fn rig_can_tx_signal_count() -> u32 {
            $model::tx_signal_descriptors().len() as u32
        }

        fn rig_can_tx_signal_descriptor(
            index: u32,
        ) -> Option<$crate::rig_runtime::CanSignalDescriptor> {
            let Some(signal) = $model::tx_signal_descriptors().get(index as usize) else {
                return None;
            };
            let Some(bus) = rig_bus_to_raw(signal.bus) else {
                return None;
            };

            Some($crate::rig_runtime::CanSignalDescriptor {
                bus,
                message_id: signal.message_id,
                kind: rig_signal_kind_to_raw(signal.kind),
            })
        }

        fn rig_can_tx_signal_message_name(index: u32) -> Option<&'static str> {
            $model::tx_signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.message_name)
        }

        fn rig_can_tx_signal_name(index: u32) -> Option<&'static str> {
            $model::tx_signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.signal_name)
        }

        fn rig_can_tx_signal_unit(index: u32) -> Option<&'static str> {
            $model::tx_signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.unit.unwrap_or(""))
        }

        fn rig_can_tx_signal_enum_name(index: u32) -> Option<&'static str> {
            $model::tx_signal_descriptors()
                .get(index as usize)
                .map(|signal| signal.enum_name.unwrap_or(""))
        }

        fn rig_can_enum_value_count() -> u32 {
            $model::enum_value_descriptors().len() as u32
        }

        fn rig_can_enum_value_descriptor(
            index: u32,
        ) -> Option<$crate::rig_runtime::CanEnumValueDescriptor> {
            let enum_value = $model::enum_value_descriptors().get(index as usize)?;
            Some($crate::rig_runtime::CanEnumValueDescriptor {
                raw: enum_value.raw,
            })
        }

        fn rig_can_enum_value_enum_name(index: u32) -> Option<&'static str> {
            $model::enum_value_descriptors()
                .get(index as usize)
                .map(|enum_value| enum_value.enum_name)
        }

        fn rig_can_enum_value_label(index: u32) -> Option<&'static str> {
            $model::enum_value_descriptors()
                .get(index as usize)
                .map(|enum_value| enum_value.label)
        }

        fn rig_can_decode_signal(
            bus: u8,
            packet: &$crate::rig_runtime::CanPacket,
            signal_name: &str,
        ) -> Option<f64> {
            let Some(bus) = rig_bus_from_raw(bus) else {
                return None;
            };

            $model::decode_transmitted_signal(bus, packet.id, packet.data, signal_name)
                .map(|measurement| measurement.value)
        }

        fn rig_can_encode_signal(
            bus: u8,
            message_name: &str,
            signal_name: &str,
            value: f64,
            packet: &mut $crate::rig_runtime::CanPacket,
        ) -> bool {
            let Some(bus) = rig_bus_from_raw(bus) else {
                return false;
            };

            let Some(message) =
                $model::encode_transmitted_signal(bus, message_name, &mut packet.data, signal_name, value)
            else {
                return false;
            };
            packet.id = message.id;
            packet.len = message.len;
            true
        }
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_bus_count() -> u8 {
    bus_count()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_push_rx(bus: u8, packet: *const CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }
    send(bus, unsafe { &*packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_push_rx_many(
    bus: u8,
    packets: *const CanPacket,
    count: u32,
) -> u32 {
    if packets.is_null() {
        return 0;
    }
    let packets = unsafe { std::slice::from_raw_parts(packets, count as usize) };
    send_many(bus, packets)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_pop_rx(bus: u8, packet: *mut CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }

    match CAN_RUNTIME.lock().unwrap().pop_rx(bus) {
        Some(next) => {
            unsafe { *packet = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_push_tx(bus: u8, packet: *const CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }
    let timestamp_ns = unsafe { super::ffi::rig_runtime_get_time_ns() };
    CAN_RUNTIME
        .lock()
        .unwrap()
        .push_tx(bus, unsafe { *packet }, timestamp_ns)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_pop_tx(bus: u8, packet: *mut CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }

    match recv(bus) {
        Some(next) => {
            unsafe { *packet = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_pop_tx_event(bus: u8, event: *mut CanEvent) -> bool {
    if event.is_null() {
        return false;
    }

    match recv_event(bus) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_pop_tx_events(
    bus: u8,
    events: *mut CanEvent,
    capacity: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts_mut(events, capacity as usize) };
    recv_events(bus, events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_rx_count(bus: u8) -> u32 {
    rx_count(bus)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_tx_count(bus: u8) -> u32 {
    tx_count(bus)
}
