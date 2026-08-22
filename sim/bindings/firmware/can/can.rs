use std::collections::{HashMap, VecDeque};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::cluster::{function_pointer, with_runtime, FirmwareBackend};
use super::dataflow::{
    DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowChannel, DataflowEdge,
    DataflowEdgeKey, DataflowEvent, DataflowRuntime, DataflowWait,
};
use super::interfaces::{InterfaceCaller, InterfaceDataflow, InterfaceEndpoint, InterfaceImplementation};
use super::registry::{InterfaceRoute, RuntimeInterfaces};
use super::runtime::RigRuntime;
use super::scheduler;

pub(super) type ClusterCanTxCountFn = unsafe extern "C" fn(u8) -> u32;
pub(super) type ClusterCanRecvEventsFn = unsafe extern "C" fn(u8, *mut CanEvent, u32) -> u32;
pub(super) type ClusterCanSendManyFn = unsafe extern "C" fn(u8, *const CanPacket, u32) -> u32;

impl RigRuntime<FirmwareBackend> {
    pub(crate) fn latest_can_message(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
    ) -> Option<CanEvent> {
        self.interfaces
            .latest_can_message(source_node, bus, message_id)
    }

    pub(crate) fn latest_can_signal(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
    ) -> Option<f64> {
        let event = self.latest_can_message(source_node, bus, message_id)?;
        self.interfaces.decode_can_signal(
            bus,
            &CanPacket {
                id: event.packet.id,
                len: event.packet.len,
                data: event.packet.data,
            },
            signal_name,
        )
    }
}

unsafe extern "C" {
    fn rig_runtime_can_notify_rx(bus: u8);
    fn rig_runtime_get_time_ns() -> u64;
}

unsafe fn write_model_string(value: &str, out: *mut c_char, out_len: usize) -> bool {
    if out.is_null() || out_len == 0 || value.len() + 1 > out_len {
        return false;
    }
    unsafe {
        ptr::copy_nonoverlapping(value.as_ptr(), out.cast::<u8>(), value.len());
        *out.add(value.len()) = 0;
    }
    true
}

unsafe fn model_c_str(value: *const c_char) -> Option<&'static str> {
    if value.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(value) }.to_str().ok()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_bus_count() -> u8 {
    bus_count()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_send(bus: u8, packet: *const CanPacket) -> bool {
    if packet.is_null() {
        return false;
    }
    send(bus, unsafe { &*packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_recv(bus: u8, packet: *mut CanPacket) -> bool {
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
pub extern "C" fn rig_model_can_recv_event(bus: u8, event: *mut CanEvent) -> bool {
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
pub extern "C" fn rig_model_can_recv_events(
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
pub extern "C" fn rig_model_can_send_many(
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
pub extern "C" fn rig_model_can_rx_count(bus: u8) -> u32 {
    rx_count(bus)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_tx_count(bus: u8) -> u32 {
    tx_count(bus)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_bus_count() -> u8 {
    codegen_bus_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_bus_name(
    bus: u8,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_bus_name(bus) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_message_count() -> u32 {
    codegen_message_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_message_descriptor(
    index: u32,
    out: *mut CanMessageDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = codegen_message_descriptor(index) else {
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
    let Some(name) = codegen_message_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_tx_message_count() -> u32 {
    codegen_tx_message_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_message_descriptor(
    index: u32,
    out: *mut CanMessageDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = codegen_tx_message_descriptor(index) else {
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
    let Some(name) = codegen_tx_message_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_signal_count() -> u32 {
    codegen_signal_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_descriptor(
    index: u32,
    out: *mut CanSignalDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = codegen_signal_descriptor(index) else {
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
    let Some(name) = codegen_signal_message_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_signal_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_unit(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_signal_unit(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_signal_enum_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_signal_enum_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_tx_signal_count() -> u32 {
    codegen_tx_signal_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_descriptor(
    index: u32,
    out: *mut CanSignalDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = codegen_tx_signal_descriptor(index) else {
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
    let Some(name) = codegen_tx_signal_message_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_tx_signal_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_unit(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_tx_signal_unit(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_tx_signal_enum_name(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_tx_signal_enum_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_can_codegen_enum_value_count() -> u32 {
    codegen_enum_value_count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_enum_value_descriptor(
    index: u32,
    out: *mut CanEnumValueDescriptor,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(descriptor) = codegen_enum_value_descriptor(index) else {
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
    let Some(name) = codegen_enum_value_enum_name(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_codegen_enum_value_label(
    index: u32,
    out: *mut c_char,
    out_len: usize,
) -> bool {
    let Some(name) = codegen_enum_value_label(index) else {
        return false;
    };
    unsafe { write_model_string(name, out, out_len) }
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
    let Some(signal_name) = (unsafe { model_c_str(signal_name) }) else {
        return false;
    };
    let Some(value) = decode_signal(bus, unsafe { &*packet }, signal_name) else {
        return false;
    };
    unsafe { *value_out = value };
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_decode_signals(
    bus: u8,
    packet: *const CanPacket,
    signal_names: *const *const c_char,
    values: *mut CanSignalValue,
    count: u32,
) -> u32 {
    if packet.is_null() || signal_names.is_null() || values.is_null() {
        return 0;
    }
    let signal_names = unsafe { std::slice::from_raw_parts(signal_names, count as usize) };
    let mut names = Vec::with_capacity(signal_names.len());
    for signal_name in signal_names {
        let Some(name) = (unsafe { model_c_str(*signal_name) }) else {
            return names.len() as u32;
        };
        names.push(name);
    }
    let values = unsafe { std::slice::from_raw_parts_mut(values, count as usize) };
    decode_signals(bus, unsafe { &*packet }, &names, values)
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
    let Some(message_name) = (unsafe { model_c_str(message_name) }) else {
        return false;
    };
    let Some(signal_name) = (unsafe { model_c_str(signal_name) }) else {
        return false;
    };
    encode_signal(bus, message_name, signal_name, value, unsafe { &mut *packet })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_model_can_encode_signals(
    bus: u8,
    message_name: *const c_char,
    signal_names: *const *const c_char,
    values: *const CanSignalValue,
    count: u32,
    packet: *mut CanPacket,
) -> u32 {
    if message_name.is_null() || signal_names.is_null() || values.is_null() || packet.is_null() {
        return 0;
    }
    let Some(message_name) = (unsafe { model_c_str(message_name) }) else {
        return 0;
    };
    let signal_names = unsafe { std::slice::from_raw_parts(signal_names, count as usize) };
    let mut names = Vec::with_capacity(signal_names.len());
    for signal_name in signal_names {
        let Some(name) = (unsafe { model_c_str(*signal_name) }) else {
            return names.len() as u32;
        };
        names.push(name);
    }
    let values = unsafe { std::slice::from_raw_parts(values, count as usize) };
    encode_signals(bus, message_name, &names, values, unsafe { &mut *packet })
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

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanSignalWake {
    pub source_node: u32,
    pub bus: u8,
    pub message_id: u32,
    pub signal_index: u32,
}

pub type CanSignalWakeCallback = unsafe extern "C" fn(*const CanEvent);
pub type CanSignalDecoderFn = unsafe extern "C" fn(
    u8,
    *const CanPacket,
    *const c_char,
    *mut f64,
) -> bool;

enum CanSignalWakeConsumer {
    Callback(CanSignalWakeCallback),
    Wait {
        comparisons: Vec<CanSignalComparison>,
        decoder: CanSignalDecoderFn,
        wait: Option<DataflowWait>,
    },
}

struct CanSignalWakeRegistration {
    wakes: Vec<CanSignalWake>,
    last_timestamp_ns: Vec<u64>,
    initialized: Vec<bool>,
    pending_events: VecDeque<CanEvent>,
    consumer: CanSignalWakeConsumer,
}

type CanSignalWakeKey = (u32, u8, u32);

impl CanSignalWakeRegistration {
    fn source_node(&self) -> u32 {
        self.wakes.first().map(|wake| wake.source_node).unwrap_or(u32::MAX)
    }

    fn edge(&self, registration_index: usize) -> DataflowEdgeKey {
        DataflowEdge::<CanEvent>::new(
            self.source_node(),
            DataflowChannel {
                interface: -1,
                port: -1,
                channel: registration_index as i32,
            },
        )
        .key()
    }
}

impl DataflowEvent for CanEvent {}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CanSignalComparison {
    pub bus: u8,
    pub message_id: u32,
    pub signal_index: u32,
    pub signal_name: [u8; 128],
    pub expected: f64,
    pub tolerance: f64,
    pub comparison: u8,
}

impl Default for CanSignalComparison {
    fn default() -> Self {
        Self {
            bus: 0,
            message_id: 0,
            signal_index: 0,
            signal_name: [0; 128],
            expected: 0.0,
            tolerance: 0.0,
            comparison: 0,
        }
    }
}

#[derive(Clone, Copy)]
pub(super) struct ClusterCanRoute {
    pub(super) source_node: u32,
    pub(super) source_bus: u8,
    pub(super) source_tx_count: ClusterCanTxCountFn,
    pub(super) source_recv_events: ClusterCanRecvEventsFn,
    pub(super) sink_node: Option<u32>,
    pub(super) sink_bus: u8,
    pub(super) sink_send_many: Option<ClusterCanSendManyFn>,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterCanSink {
    pub(super) sink_node: u32,
    pub(super) sink_bus: u8,
    pub(super) sink_send_many: ClusterCanSendManyFn,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterCanRecord {
    pub(super) source_node: u32,
    pub(super) bus: u8,
    pub(super) event: CanEvent,
}

pub(super) struct CanRouteResult {
    pub(super) input_pending_nodes: Vec<u32>,
    pub(super) ready_edges: Vec<DataflowEdgeKey>,
}

pub(super) struct CanInterfaceFanout {
    pub(super) source_node: u32,
    pub(super) endpoint: CanEndpoint,
    pub(super) record_index: usize,
    pub(super) source_tx_count: ClusterCanTxCountFn,
    pub(super) source_recv_events: ClusterCanRecvEventsFn,
    pub(super) sinks: Vec<ClusterCanSink>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub(super) struct CanEndpoint {
    bus: u8,
}

impl CanEndpoint {
    pub(super) fn new(bus: u8) -> Self {
        Self { bus }
    }

    pub(super) fn bus(self) -> u8 {
        self.bus
    }
}

impl InterfaceEndpoint for CanEndpoint {
    fn dataflow_channel(self) -> DataflowChannel {
        DataflowChannel {
            interface: self.bus as i32,
            ..Default::default()
        }
    }
}

struct CanRecordStream {
    source_node: u32,
    records: VecDeque<CanEvent>,
}

/// CAN packet runtime interface: bus fanout, source-only records, and latest-message lookup.
#[derive(Default)]
pub(super) struct CanInterface {
    pub(super) fanout_indexes: HashMap<(u32, CanEndpoint), usize>,
    pub(super) fanouts: Vec<CanInterfaceFanout>,
    pub(super) native_source_events: VecDeque<ClusterCanRecord>,
    record_indexes: HashMap<(u32, CanEndpoint), usize>,
    records: Vec<CanRecordStream>,
    signal_wakes: Vec<CanSignalWakeRegistration>,
    signal_wake_indexes: HashMap<CanSignalWakeKey, Vec<usize>>,
}

impl InterfaceImplementation for CanInterface {
    fn reset_interface(&mut self) {
        self.fanout_indexes.clear();
        self.fanouts.clear();
        self.native_source_events.clear();
        self.record_indexes.clear();
        self.records.clear();
        self.signal_wake_indexes.clear();
        for registration in &mut self.signal_wakes {
            registration.last_timestamp_ns.fill(0);
            registration.initialized.fill(false);
            registration.pending_events.clear();
            if let CanSignalWakeConsumer::Wait { wait, .. } = &mut registration.consumer {
                *wait = None;
            }
        }
        for registration_index in 0..self.signal_wakes.len() {
            self.index_signal_wake_registration(registration_index);
        }
    }
}

impl InterfaceCaller for CanInterface {
    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        specs.push(DataflowAlgorithm::source(
            u32::MAX,
            (u32::MAX, 0, 0),
            Vec::new(),
            Arc::new(NativeCanSourceAlgorithm),
        ));
        for (index, group) in self.fanouts.iter().enumerate() {
            specs.push(DataflowAlgorithm::source(
                group.source_node,
                (group.source_node, 1, index),
                vec![<Self as InterfaceDataflow<CanEvent>>::edge(
                    group.source_node,
                    group.endpoint,
                )],
                Arc::new(CanFanoutAlgorithm { group_index: index }),
            ));
        }
        for (index, registration) in self.signal_wakes.iter().enumerate() {
            specs.push(DataflowAlgorithm::event_sink(
                // A CAN wake is an ingress event owned by the runtime.  Its
                // source node identifies the edge's producer, but must not
                // gate delivery on that model node being online.
                u32::MAX,
                (registration.source_node(), 2, index),
                vec![registration.edge(index)],
                Arc::new(CanSignalWakeAlgorithm { registration_index: index }),
            ));
        }
    }
}

impl InterfaceDataflow<CanEvent> for CanInterface {
    type Endpoint = CanEndpoint;
}

impl CanInterface {
    #[cfg(test)]
    pub(super) fn signal_wake_count(&self) -> usize {
        self.signal_wakes.len()
    }

    #[cfg(test)]
    fn indexed_signal_wake_count(&self, source_node: u32, bus: u8, message_id: u32) -> usize {
        self.signal_wake_indexes
            .get(&(source_node, bus, message_id))
            .map_or(0, Vec::len)
    }

    fn index_signal_wake_registration(&mut self, registration_index: usize) {
        let Some(registration) = self.signal_wakes.get(registration_index) else {
            return;
        };
        if matches!(
            &registration.consumer,
            CanSignalWakeConsumer::Wait { wait: None, .. }
        ) {
            return;
        }
        let keys: Vec<_> = registration
            .wakes
            .iter()
            .map(|wake| (wake.source_node, wake.bus, wake.message_id))
            .collect();
        for key in keys {
            let registrations = self.signal_wake_indexes.entry(key).or_default();
            if !registrations.contains(&registration_index) {
                registrations.push(registration_index);
            }
        }
    }

    fn remove_signal_wake_registration_from_index(&mut self, registration_index: usize) {
        let Some(registration) = self.signal_wakes.get(registration_index) else {
            return;
        };
        let keys: Vec<_> = registration
            .wakes
            .iter()
            .map(|wake| (wake.source_node, wake.bus, wake.message_id))
            .collect();
        for key in keys {
            let Some(registrations) = self.signal_wake_indexes.get_mut(&key) else {
                continue;
            };
            registrations.retain(|index| *index != registration_index);
            if registrations.is_empty() {
                self.signal_wake_indexes.remove(&key);
            }
        }
    }

    pub(super) fn upsert_fanout(&mut self, route: ClusterCanRoute) {
        let endpoint = CanEndpoint::new(route.source_bus);
        let key = (route.source_node, endpoint);
        let record_index = self.ensure_record_stream(route.source_node, endpoint);
        let group_index = *self.fanout_indexes.entry(key).or_insert_with(|| {
            self.fanouts.push(CanInterfaceFanout {
                source_node: route.source_node,
                endpoint,
                record_index,
                source_tx_count: route.source_tx_count,
                source_recv_events: route.source_recv_events,
                sinks: Vec::new(),
            });
            self.fanouts.len() - 1
        });
        if let (Some(sink_node), Some(sink_send_many)) = (route.sink_node, route.sink_send_many) {
            if self.fanouts[group_index]
                .sinks
                .iter()
                .any(|sink| sink.sink_node == sink_node && sink.sink_bus == route.sink_bus)
            {
                return;
            }
            self.fanouts[group_index].sinks.push(ClusterCanSink {
                sink_node,
                sink_bus: route.sink_bus,
                sink_send_many,
            });
        }
    }

    fn ensure_record_stream(&mut self, source_node: u32, endpoint: CanEndpoint) -> usize {
        let key = (source_node, endpoint);
        *self.record_indexes.entry(key).or_insert_with(|| {
            self.records.push(CanRecordStream {
                source_node,
                records: VecDeque::new(),
            });
            self.records.len() - 1
        })
    }

    pub(super) fn record(&mut self, source_node: u32, bus: u8, event: CanEvent) -> Vec<DataflowEdgeKey> {
        let endpoint = CanEndpoint::new(bus);
        let stream_index = self.ensure_record_stream(source_node, endpoint);
        self.record_at(stream_index, event)
    }

    pub(super) fn record_at(&mut self, stream_index: usize, event: CanEvent) -> Vec<DataflowEdgeKey> {
        self.records[stream_index].records.push_back(event);
        let source_node = self.records[stream_index].source_node;
        let mut ready_edges = Vec::new();
        let Some(registration_indexes) =
            self.signal_wake_indexes
                .get(&(source_node, event.bus, event.packet.id))
        else {
            return ready_edges;
        };
        for &registration_index in registration_indexes {
            let registration = &mut self.signal_wakes[registration_index];
            let mut triggered = false;
            for (wake_index, wake) in registration.wakes.iter().enumerate() {
                if (registration.initialized[wake_index]
                    && event.timestamp_ns <= registration.last_timestamp_ns[wake_index])
                {
                    continue;
                }
                registration.last_timestamp_ns[wake_index] = event.timestamp_ns;
                registration.initialized[wake_index] = true;
                triggered = true;
            }
            if !triggered {
                continue;
            }
            registration.pending_events.push_back(event);
            ready_edges.push(registration.edge(registration_index));
        }
        ready_edges
    }

    pub(super) fn register_signal_wake(
        &mut self,
        wake: CanSignalWake,
        callback: CanSignalWakeCallback,
    ) -> bool {
        if self.signal_wakes.iter().any(|registration| {
            registration.wakes == [wake]
                && matches!(
                    &registration.consumer,
                    CanSignalWakeConsumer::Callback(existing)
                        if *existing as usize == callback as usize
                )
        }) {
            return true;
        }
        self.signal_wakes.push(CanSignalWakeRegistration {
            wakes: vec![wake],
            last_timestamp_ns: vec![0],
            initialized: vec![false],
            pending_events: VecDeque::new(),
            consumer: CanSignalWakeConsumer::Callback(callback),
        });
        self.index_signal_wake_registration(self.signal_wakes.len() - 1);
        true
    }

    pub(super) fn begin_signal_wait(
        &mut self,
        source_node: u32,
        comparisons: &[CanSignalComparison],
        decoder: CanSignalDecoderFn,
        wait: DataflowWait,
    ) -> bool {
        let wakes: Vec<_> = comparisons
            .iter()
            .map(|comparison| CanSignalWake {
                source_node,
                bus: comparison.bus,
                message_id: comparison.message_id,
                signal_index: comparison.signal_index,
            })
            .collect();
        let matched = self.signal_comparisons_match(source_node, comparisons, decoder);
        self.signal_wakes.push(CanSignalWakeRegistration {
            last_timestamp_ns: vec![0; wakes.len()],
            initialized: vec![false; wakes.len()],
            wakes,
            pending_events: VecDeque::new(),
            consumer: CanSignalWakeConsumer::Wait {
                comparisons: comparisons.to_vec(),
                decoder,
                wait: Some(wait),
            },
        });
        self.index_signal_wake_registration(self.signal_wakes.len() - 1);
        matched
    }

    pub(super) fn cancel_signal_wait(&mut self, wait: DataflowWait) {
        let Some(wait_id) = self.signal_wakes.iter().position(|registration| {
            matches!(
                &registration.consumer,
                CanSignalWakeConsumer::Wait {
                    wait: Some(candidate),
                    ..
                } if *candidate == wait
            )
        }) else {
            return;
        };
        if wait_id + 1 == self.signal_wakes.len() {
            self.remove_signal_wake_registration_from_index(wait_id);
            self.signal_wakes.pop();
        } else if let Some(registration) = self.signal_wakes.get_mut(wait_id) {
            if let CanSignalWakeConsumer::Wait { wait, .. } = &mut registration.consumer {
                *wait = None;
            }
            self.remove_signal_wake_registration_from_index(wait_id);
        }
    }

    pub(super) fn run_signal_wake(
        &mut self,
        registration_index: usize,
    ) -> Option<DataflowWait> {
        let Some(registration) = self.signal_wakes.get_mut(registration_index) else {
            return None;
        };
        let events = std::mem::take(&mut registration.pending_events);
        let (callback, wait) = match &registration.consumer {
            CanSignalWakeConsumer::Callback(callback) => (Some(*callback), None),
            CanSignalWakeConsumer::Wait {
                comparisons,
                decoder,
                wait,
            } => wait
                .map(|wait| (None, Some((registration.source_node(), comparisons.clone(), *decoder, wait))))
                .unwrap_or((None, None)),
        };
        if let Some(callback) = callback {
            for event in events {
                unsafe { callback(&event) };
            }
        }
        if let Some((source_node, comparisons, decoder, wait)) = wait {
            if self.signal_comparisons_match(source_node, &comparisons, decoder) {
                return Some(wait);
            }
        }
        None
    }

    fn signal_comparisons_match(
        &self,
        source_node: u32,
        comparisons: &[CanSignalComparison],
        decoder: CanSignalDecoderFn,
    ) -> bool {
        comparisons.iter().all(|comparison| {
            let Some(event) =
                self.latest_message(source_node, comparison.bus, comparison.message_id)
            else {
                return false;
            };
            let Some(signal_name) = comparison_signal_name(comparison) else {
                return false;
            };
            let Some(value) = decode_signal_with(decoder, comparison.bus, &event.packet, signal_name)
            else {
                return false;
            };
            compare_signal_value(
                value,
                comparison.expected,
                comparison.tolerance,
                comparison.comparison,
            )
        })
    }

    pub(super) fn push_native_source_event(&mut self, record: ClusterCanRecord) {
        self.native_source_events.push_back(record);
    }

    pub(super) fn pop_native_source_event(&mut self) -> Option<ClusterCanRecord> {
        self.native_source_events.pop_front()
    }

    pub(super) fn native_source_pending(&self, mut source_online: impl FnMut(u32) -> bool) -> bool {
        self.native_source_events
            .iter()
            .any(|record| source_online(record.source_node))
    }

    pub(super) fn route_event(
        &mut self,
        source_node: u32,
        bus: u8,
        event: CanEvent,
    ) -> CanRouteResult {
        let Some(group_index) = self
            .fanout_indexes
            .get(&(source_node, CanEndpoint::new(bus)))
            .copied()
        else {
            return CanRouteResult {
                input_pending_nodes: Vec::new(),
                ready_edges: self.record(source_node, bus, event),
            };
        };

        let mut input_pending_nodes = Vec::new();
        let record_index = self.fanouts[group_index].record_index;
        for sink in &self.fanouts[group_index].sinks {
            let accepted = unsafe { (sink.sink_send_many)(sink.sink_bus, &event.packet, 1) };
            if accepted > 0 {
                input_pending_nodes.push(sink.sink_node);
            }
        }
        CanRouteResult {
            input_pending_nodes,
            ready_edges: self.record_at(record_index, event),
        }
    }

    pub(super) fn fanout_pending(
        &self,
        group_index: usize,
        mut source_online: impl FnMut(u32) -> bool,
    ) -> bool {
        let Some(group) = self.fanouts.get(group_index) else {
            return false;
        };
        source_online(group.source_node)
            && unsafe { (group.source_tx_count)(group.endpoint.bus()) } != 0
    }

    pub(super) fn route_fanout(
        &mut self,
        group_index: usize,
        mut source_online: impl FnMut(u32) -> bool,
    ) -> Option<CanRouteResult> {
        let Some(group) = self.fanouts.get(group_index) else {
            return None;
        };
        let source_node = group.source_node;
        let source_bus = group.endpoint.bus();
        let record_index = group.record_index;
        let source_tx_count = group.source_tx_count;
        let source_recv_events = group.source_recv_events;
        let sink_count = group.sinks.len();

        if !source_online(source_node) {
            return None;
        }

        let pending = unsafe { source_tx_count(source_bus) };
        if pending == 0 {
            return None;
        }

        let mut events = vec![CanEvent::default(); pending as usize];
        let count = unsafe { source_recv_events(source_bus, events.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return None;
        }
        events.truncate(count);

        let packets: Vec<_> = events.iter().map(|event| event.packet).collect();
        let mut input_pending_nodes = Vec::new();
        if !packets.is_empty() {
            for sink_index in 0..sink_count {
                let sink = self.fanouts[group_index].sinks[sink_index];
                let accepted = unsafe {
                    (sink.sink_send_many)(
                        sink.sink_bus,
                        packets.as_ptr(),
                        packets.len().min(u32::MAX as usize) as u32,
                    )
                };
                if accepted > 0 {
                    input_pending_nodes.push(sink.sink_node);
                }
            }
        }

        let mut ready_edges = Vec::new();
        for event in events {
            ready_edges.extend(self.record_at(record_index, event));
        }
        Some(CanRouteResult {
            input_pending_nodes,
            ready_edges,
        })
    }

    pub(super) fn latest_message(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
    ) -> Option<CanEvent> {
        let stream_index = self
            .record_indexes
            .get(&(source_node, CanEndpoint::new(bus)))
            .copied()?;
        self.records[stream_index]
            .records
            .iter()
            .rev()
            .find(|event| event.packet.id == message_id)
            .copied()
    }

    pub(super) fn latest_bus_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        let stream_index = self
            .record_indexes
            .get(&(source_node, CanEndpoint::new(bus)))
            .copied()?;
        self.records[stream_index].records.back().copied()
    }
}

fn decode_signal_with(
        decoder: CanSignalDecoderFn,
    bus: u8,
    packet: &CanPacket,
    signal_name: &str,
) -> Option<f64> {
    let signal_name = CString::new(signal_name).ok()?;
    let mut value = 0.0;
    if unsafe {
        decoder(
            bus,
            packet as *const CanPacket,
            signal_name.as_ptr(),
            &mut value,
        )
    } {
        Some(value)
    } else {
        None
    }
}

fn comparison_signal_name(comparison: &CanSignalComparison) -> Option<&str> {
    let length = comparison
        .signal_name
        .iter()
        .position(|byte| *byte == 0)?;
    std::str::from_utf8(&comparison.signal_name[..length]).ok()
}

fn compare_signal_value(value: f64, expected: f64, tolerance: f64, comparison: u8) -> bool {
    match comparison {
        0 => (value - expected).abs() <= tolerance,
        1 => value > expected + tolerance,
        2 => value >= expected - tolerance,
        3 => value < expected - tolerance,
        4 => value <= expected + tolerance,
        _ => false,
    }
}

struct CanFanoutAlgorithm {
    group_index: usize,
}

impl DataflowAlgorithmExecutor for CanFanoutAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any()
            .downcast_ref::<RigRuntime<FirmwareBackend>>()
            .expect("CAN fanout requires the firmware runtime backend");
        runtime
            .interfaces
            .can
            .fanout_pending(self.group_index, |source_node| {
                runtime.node_online(source_node)
            })
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any_mut()
            .downcast_mut::<RigRuntime<FirmwareBackend>>()
            .expect("CAN fanout requires the firmware runtime backend");
        run_can_fanout(runtime, self.group_index)
    }
}

struct CanSignalWakeAlgorithm {
    registration_index: usize,
}

impl DataflowAlgorithmExecutor for CanSignalWakeAlgorithm {
    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any_mut()
            .downcast_mut::<RigRuntime<FirmwareBackend>>()
            .expect("CAN signal wake requires the firmware runtime backend");
        let wait = runtime
            .interfaces
            .can
            .run_signal_wake(self.registration_index);
        if let Some(wait) = wait {
            scheduler::complete_dataflow_wait(runtime, wait);
        }
        false
    }
}

fn run_can_fanout(runtime: &mut RigRuntime<FirmwareBackend>, group_index: usize) -> bool {
    let online_nodes = runtime.online_nodes();
    let Some(result) = runtime
        .interfaces
        .can
        .route_fanout(group_index, |node| online_node(&online_nodes, node))
    else {
        return false;
    };
    for edge in result.ready_edges {
        scheduler::mark_dataflow_edge_ready(runtime, edge);
    }
    for sink_node in result.input_pending_nodes {
        scheduler::mark_input_pending(runtime, sink_node);
    }
    true
}

struct NativeCanSourceAlgorithm;

impl DataflowAlgorithmExecutor for NativeCanSourceAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any()
            .downcast_ref::<RigRuntime<FirmwareBackend>>()
            .expect("native CAN source requires the firmware runtime backend");
        runtime
            .interfaces
            .can_native_source_pending(|source_node| runtime.node_online(source_node))
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any_mut()
            .downcast_mut::<RigRuntime<FirmwareBackend>>()
            .expect("native CAN source requires the firmware runtime backend");
        let mut routed = false;
        let mut input_pending_nodes = Vec::new();
        let mut ready_edges = Vec::new();
        while let Some(record) = runtime.interfaces.can_pop_native_source_event() {
            if !runtime.node_online(record.source_node) {
                continue;
            }
            routed = true;
            let result = runtime.interfaces.can_route_event(
                record.source_node,
                record.bus,
                record.event,
            );
            input_pending_nodes.extend(result.input_pending_nodes);
            ready_edges.extend(result.ready_edges);
        }
        for edge in ready_edges {
            scheduler::mark_dataflow_edge_ready(runtime, edge);
        }
        for sink_node in input_pending_nodes {
            scheduler::mark_input_pending(runtime, sink_node);
        }
        routed
    }
}

fn online_node(online_nodes: &[bool], node: u32) -> bool {
    online_nodes.get(node as usize).copied().unwrap_or(false)
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

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_can_tx_count(_bus: u8) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_can_recv_events(
    _bus: u8,
    _events: *mut CanEvent,
    _capacity: u32,
) -> u32 {
    0
}

#[derive(Clone, Copy)]
struct PeriodicCanSource {
    node: u32,
    bus: u8,
    period_ns: u64,
    last_emit_ns: u64,
    packet: CanPacket,
}

impl PeriodicCanSource {
    fn new(node: u32, bus: u8, period_ns: u64, packet: CanPacket) -> Self {
        Self { node, bus, period_ns, last_emit_ns: 0, packet }
    }

    fn node(&self) -> u32 { self.node }
    fn bus(&self) -> u8 { self.bus }
    fn period_ns(&self) -> u64 { self.period_ns }
    fn due_at_ns(&self) -> u64 { self.last_emit_ns.saturating_add(self.period_ns) }

    fn has_pending_event(&self, elapsed_ns: u64) -> bool {
        elapsed_ns.saturating_sub(self.last_emit_ns) >= self.period_ns
    }

    fn update_packet(&mut self, packet: CanPacket) { self.packet = packet; }

    fn emit_if_due(&mut self, elapsed_ns: u64) -> Option<CanEvent> {
        if !self.has_pending_event(elapsed_ns) {
            return None;
        }
        self.last_emit_ns = elapsed_ns;
        Some(CanEvent { bus: self.bus, timestamp_ns: elapsed_ns, packet: self.packet })
    }
}

#[derive(Default)]
struct PeriodicCanSources {
    sources: Vec<PeriodicCanSource>,
}

impl PeriodicCanSources {
    fn reset(&mut self) { self.sources.clear(); }
}

static PERIODIC_CAN_SOURCES: LazyLock<Mutex<PeriodicCanSources>> =
    LazyLock::new(|| Mutex::new(PeriodicCanSources::default()));

pub(super) fn add_periodic_can_source(
    runtime: &mut RigRuntime<FirmwareBackend>,
    node: u32,
    bus: u8,
    period_ns: u64,
    packet: CanPacket,
) -> u32 {
    if !runtime.node_exists(node) || period_ns == 0 {
        return u32::MAX;
    }
    let mut sources = PERIODIC_CAN_SOURCES.lock().unwrap();
    let handle = sources.sources.len();
    sources.sources.push(PeriodicCanSource::new(node, bus, period_ns, packet));
    let source = sources.sources[handle];
    drop(sources);

    if !algorithms::register_algorithm(
        runtime,
        DataflowAlgorithm::periodic_source(
            source.node(),
            (source.node(), 0, handle),
            vec![RuntimeInterfaces::can_edge(source.node(), source.bus())],
            Arc::new(PeriodicCanSourceAlgorithm { source_index: handle }),
            source.period_ns(),
            source.due_at_ns(),
        )
        .with_runtime_reset(reset_periodic_can_sources),
    ) {
        PERIODIC_CAN_SOURCES.lock().unwrap().sources.pop();
        return u32::MAX;
    }
    handle as u32
}

pub(super) fn update_periodic_can_source(
    _runtime: &mut RigRuntime<FirmwareBackend>,
    handle: u32,
    packet: CanPacket,
) -> bool {
    let mut sources = PERIODIC_CAN_SOURCES.lock().unwrap();
    let Some(source) = sources.sources.get_mut(handle as usize) else { return false; };
    source.update_packet(packet);
    true
}

fn reset_periodic_can_sources() {
    PERIODIC_CAN_SOURCES.lock().unwrap().reset();
}

struct PeriodicCanSourceAlgorithm {
    source_index: usize,
}

impl DataflowAlgorithmExecutor for PeriodicCanSourceAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any()
            .downcast_ref::<RigRuntime<FirmwareBackend>>()
            .expect("periodic CAN source requires the firmware runtime backend");
        let sources = PERIODIC_CAN_SOURCES.lock().unwrap();
        let Some(source) = sources.sources.get(self.source_index) else { return false; };
        runtime.node_online(source.node()) && source.has_pending_event(runtime.elapsed_ns)
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let runtime = runtime
            .as_any_mut()
            .downcast_mut::<RigRuntime<FirmwareBackend>>()
            .expect("periodic CAN source requires the firmware runtime backend");
        let mut sources = PERIODIC_CAN_SOURCES.lock().unwrap();
        let Some(source) = sources.sources.get_mut(self.source_index) else { return false; };
        let source_node = source.node();
        let source_bus = source.bus();
        if !runtime.node_online(source_node) {
            return false;
        }
        let Some(event) = source.emit_if_due(runtime.elapsed_ns) else { return false; };
        drop(sources);

        let result = runtime.interfaces.can.route_event(source_node, source_bus, event);
        for edge in result.ready_edges {
            scheduler::mark_dataflow_edge_ready(runtime, edge);
        }
        for sink_node in result.input_pending_nodes {
            scheduler::mark_input_pending(runtime, sink_node);
        }
        true
    }
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

            let Some(message) = $model::encode_transmitted_signal(
                bus,
                message_name,
                &mut packet.data,
                signal_name,
                value,
            ) else {
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
    let timestamp_ns = unsafe { rig_runtime_get_time_ns() };
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

// Cluster-facing CAN registration and observation APIs belong to the CAN
// binding. The cluster module owns only the runtime handle and scheduler;
// CAN remains responsible for translating its ABI into CAN edge operations.
#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_can_route(
    source_node: u32,
    source_bus: u8,
    source_tx_count: usize,
    source_recv_events: usize,
    sink_node: u32,
    sink_bus: u8,
    sink_send_many: usize,
) -> bool {
    let Some(source_tx_count) =
        (unsafe { function_pointer::<ClusterCanTxCountFn>(source_tx_count) })
    else {
        return false;
    };
    let Some(source_recv_events) =
        (unsafe { function_pointer::<ClusterCanRecvEventsFn>(source_recv_events) })
    else {
        return false;
    };
    let sink_send_many = if sink_node == u32::MAX {
        if sink_send_many != 0 {
            return false;
        }
        None
    } else {
        let Some(sink_send_many) =
            (unsafe { function_pointer::<ClusterCanSendManyFn>(sink_send_many) })
        else {
            return false;
        };
        Some(sink_send_many)
    };

    with_runtime(|runtime| {
        runtime.register_route(InterfaceRoute::Can(ClusterCanRoute {
            source_node,
            source_bus,
            source_tx_count,
            source_recv_events,
            sink_node: (sink_node != u32::MAX).then_some(sink_node),
            sink_bus,
            sink_send_many,
        }))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_periodic_can_source(
    node: u32,
    bus: u8,
    period_ns: u64,
    packet: *const CanPacket,
) -> u32 {
    if packet.is_null() {
        return u32::MAX;
    }
    with_runtime(|runtime| {
        add_periodic_can_source(runtime, node, bus, period_ns, unsafe { *packet })
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_update_periodic_can_source(
    handle: u32,
    packet: *const CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    with_runtime(|runtime| update_periodic_can_source(runtime, handle, unsafe { *packet }))
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_send_native_can_source_event(
    node: u32,
    bus: u8,
    packet: *const CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    with_runtime(|runtime| {
        if !runtime.node_exists(node) {
            return false;
        }
        let elapsed_ns = runtime.elapsed_ns();
        runtime.interfaces.send_native_can_source_event(
            node,
            bus,
            elapsed_ns,
            unsafe { *packet },
        );
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_register_can_signal_wake(
    wake: *const CanSignalWake,
    callback: usize,
) -> bool {
    if wake.is_null() {
        return false;
    }
    let Some(callback) = (unsafe { function_pointer::<CanSignalWakeCallback>(callback) }) else {
        return false;
    };
    with_runtime(|runtime| {
        let wake = unsafe { *wake };
        if !runtime.node_exists(wake.source_node) {
            return false;
        }
        let registered = runtime.interfaces.can.register_signal_wake(wake, callback);
        if registered {
            runtime.scheduler.mark_dirty();
        }
        registered
    })
}

fn begin_can_signal_wait(
    runtime: &mut super::runtime::RigRuntime<FirmwareBackend>,
    source_node: u32,
    comparisons: &[CanSignalComparison],
    decoder: CanSignalDecoderFn,
) -> DataflowWait {
    let wait = runtime.begin_dataflow_wait();
    let matched = runtime
        .interfaces
        .begin_can_signal_wait(source_node, comparisons, decoder, wait);
    if matched {
        super::scheduler::complete_dataflow_wait(runtime, wait);
    }
    runtime.scheduler.mark_dirty();
    wait
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_begin_can_signal_wait(
    source_node: u32,
    comparisons: *const CanSignalComparison,
    comparison_count: u32,
    decoder: usize,
) -> u64 {
    if comparisons.is_null() || comparison_count == 0 || decoder == 0 {
        return u64::MAX;
    }
    let comparisons = unsafe { std::slice::from_raw_parts(comparisons, comparison_count as usize) };
    let decoder = unsafe { std::mem::transmute::<usize, CanSignalDecoderFn>(decoder) };
    with_runtime(|runtime| begin_can_signal_wait(runtime, source_node, comparisons, decoder).0)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_can_message(
    source_node: u32,
    bus: u8,
    message_id: u32,
    out: *mut CanEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let event = with_runtime(|runtime| runtime.interfaces.latest_can_message(
        source_node,
        bus,
        message_id,
    ));
    let Some(event) = event else {
        return false;
    };
    unsafe { *out = event };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_can_bus_event(
    source_node: u32,
    bus: u8,
    out: *mut CanEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let event = with_runtime(|runtime| runtime.interfaces.latest_can_event(source_node, bus));
    let Some(event) = event else {
        return false;
    };
    unsafe { *out = event };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_can_signal(
    source_node: u32,
    bus: u8,
    message_id: u32,
    signal_name: *const c_char,
    out: *mut f64,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(signal_name) = (unsafe { model_c_str(signal_name) }) else {
        return false;
    };
    let value = with_runtime(|runtime| {
        let event = runtime.interfaces.latest_can_message(source_node, bus, message_id)?;
        runtime.interfaces.decode_can_signal(
            bus,
            &CanPacket {
                id: event.packet.id,
                len: event.packet.len,
                data: event.packet.data,
            },
            signal_name,
        )
    });
    let Some(value) = value else {
        return false;
    };
    unsafe { *out = value };
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    static WAKE_COUNT: AtomicU64 = AtomicU64::new(0);
    static LAST_WAKE_TIMESTAMP: AtomicU64 = AtomicU64::new(0);

    unsafe extern "C" fn record_wake(event: *const CanEvent) {
        let event = unsafe { &*event };
        WAKE_COUNT.fetch_add(1, Ordering::Relaxed);
        LAST_WAKE_TIMESTAMP.store(event.timestamp_ns, Ordering::Relaxed);
    }

    #[test]
    fn signal_wake_is_deduplicated_at_can_edge_ingress() {
        WAKE_COUNT.store(0, Ordering::Relaxed);
        LAST_WAKE_TIMESTAMP.store(0, Ordering::Relaxed);
        let mut interface = CanInterface::default();
        assert!(interface.register_signal_wake(
            CanSignalWake {
                source_node: 3,
                bus: 1,
                message_id: 0x123,
                signal_index: 7,
                ..Default::default()
            },
            record_wake,
        ));

        let make_event = |timestamp_ns, id| CanEvent {
            bus: 1,
            timestamp_ns,
            packet: CanPacket::new(id, [0; 8], 0),
        };
        interface.record(2, 1, make_event(1, 0x123));
        interface.record(3, 1, make_event(1, 0x123));
        assert!(!interface.signal_wakes[0].pending_events.is_empty());
        interface.run_signal_wake(0);
        assert_eq!(WAKE_COUNT.load(Ordering::Relaxed), 1);
        assert_eq!(LAST_WAKE_TIMESTAMP.load(Ordering::Relaxed), 1);

        interface.record(3, 1, make_event(1, 0x123));
        assert!(interface.signal_wakes[0].pending_events.is_empty());
        assert_eq!(WAKE_COUNT.load(Ordering::Relaxed), 1);
        interface.record(3, 1, make_event(2, 0x123));
        interface.run_signal_wake(0);
        assert_eq!(WAKE_COUNT.load(Ordering::Relaxed), 2);
        assert_eq!(LAST_WAKE_TIMESTAMP.load(Ordering::Relaxed), 2);
    }

    #[test]
    fn signal_wake_dispatch_indexes_only_matching_can_messages() {
        let mut interface = CanInterface::default();
        let wake = |message_id| CanSignalWake {
            source_node: 3,
            bus: 1,
            message_id,
            signal_index: 7,
        };

        assert!(interface.register_signal_wake(wake(0x123), record_wake));
        assert!(interface.register_signal_wake(wake(0x456), record_wake));
        assert_eq!(interface.indexed_signal_wake_count(3, 1, 0x123), 1);
        assert_eq!(interface.indexed_signal_wake_count(3, 1, 0x456), 1);
        assert_eq!(interface.indexed_signal_wake_count(3, 1, 0x789), 0);

        let event = CanEvent {
            bus: 1,
            timestamp_ns: 1,
            packet: CanPacket::new(0x123, [0; 8], 0),
        };
        let ready_edges = interface.record(3, 1, event);

        assert_eq!(ready_edges.len(), 1);
        assert!(!interface.signal_wakes[0].pending_events.is_empty());
        assert!(interface.signal_wakes[1].pending_events.is_empty());
    }

    unsafe extern "C" fn wait_node_run_for(_elapsed_ns: u64) {}
    unsafe extern "C" fn wait_node_reset() {}
    unsafe extern "C" fn wait_decode(
        _bus: u8,
        _packet: *const CanPacket,
        _signal_name: *const c_char,
        _value: *mut f64,
    ) -> bool {
        false
    }

    #[test]
    fn canceling_a_can_wait_removes_its_backend_ingress_registration() {
        let mut runtime = RigRuntime::<FirmwareBackend>::default();
        runtime.add_node(wait_node_run_for, wait_node_reset, true);

        let wait = begin_can_signal_wait(
            &mut runtime,
            0,
            &[CanSignalComparison::default()],
            wait_decode,
        );
        assert_eq!(runtime.backend().interfaces.can.signal_wake_count(), 1);

        runtime.cancel_dataflow_wait(wait);

        assert_eq!(runtime.backend().interfaces.can.signal_wake_count(), 0);
        assert_eq!(
            runtime
                .backend()
                .interfaces
                .can
                .indexed_signal_wake_count(0, 0, 0),
            0
        );
    }
}
