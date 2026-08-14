use std::collections::{HashMap, VecDeque};
use std::ffi::CStr;
use std::mem;
use std::os::raw::c_char;
use std::sync::{LazyLock, Mutex};

use super::can;
use super::battery_source::BatterySourceModel;
use super::dc_load::DcLoadModel;
use super::simple::PeriodicCanSource;

pub type ClusterNodeRunForFn = unsafe extern "C" fn(u64);
pub type ClusterNodeFastForwardForFn = unsafe extern "C" fn(u64);
pub type ClusterNodeNextStepFn = unsafe extern "C" fn(u64) -> u64;
pub type ClusterNodeResetFn = unsafe extern "C" fn();
pub type ClusterPythonScheduledFn = unsafe extern "C" fn(*const SchedulerCallbackContext);
pub type ClusterRouteFn = unsafe extern "C" fn(u64);
pub type ClusterCanTxCountFn = unsafe extern "C" fn(u8) -> u32;
pub type ClusterCanRecvEventsFn = unsafe extern "C" fn(u8, *mut CanEvent, u32) -> u32;
pub type ClusterCanSendManyFn = unsafe extern "C" fn(u8, *const CanPacket, u32) -> u32;
pub type ClusterTimerCountFn = unsafe extern "C" fn(i32, i32) -> u32;
pub type ClusterTimerRecvManyFn =
    unsafe extern "C" fn(i32, i32, *mut TimerChannelEvent, u32) -> u32;
pub type ClusterTimerSendManyFn = unsafe extern "C" fn(*const TimerChannelEvent, u32) -> u32;
pub type ClusterSpiCountFn = unsafe extern "C" fn(i32) -> u32;
pub type ClusterSpiRecvManyFn = unsafe extern "C" fn(i32, *mut SpiTransaction, u32) -> u32;
pub type ClusterSpiSendManyFn = unsafe extern "C" fn(*const SpiTransaction, u32) -> u32;
pub type ClusterScalarCountFn = unsafe extern "C" fn() -> u32;
pub type ClusterScalarRecvManyFn = unsafe extern "C" fn(*mut ScalarEvent, u32) -> u32;
pub type ClusterScalarSendManyFn = unsafe extern "C" fn(*const ScalarEvent, u32) -> u32;
pub type ClusterScalarSinkSetFn = unsafe extern "C" fn(i32, f32);

const COMPARE_EQ: u8 = 0;
const COMPARE_GT: u8 = 1;
const COMPARE_GE: u8 = 2;
const COMPARE_LT: u8 = 3;
const COMPARE_LE: u8 = 4;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanPacket {
    pub id: u32,
    pub len: u8,
    pub data: [u8; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanEvent {
    pub bus: u8,
    pub timestamp_ns: u64,
    pub packet: CanPacket,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TimerChannelEvent {
    pub port: i32,
    pub channel: i32,
    pub value: f32,
    pub timestamp_ns: u64,
}

pub const RIG_SPI_TRANSACTION_MAX_BYTES: usize = 256;

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

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ScalarEvent {
    pub value: f32,
    pub timestamp_ns: u64,
}

#[derive(Clone, Copy)]
struct TimerScaledScalarSource {
    node: u32,
    route_id: u32,
    timer_interface: u16,
    timer_port: i32,
    timer_channel: i32,
    scale_route_id: u32,
    scale: f32,
    offset: f32,
    output_value: f32,
    pending_value: bool,
}

impl TimerScaledScalarSource {
    fn new(
        node: u32,
        route_id: u32,
        timer_interface: u16,
        timer_port: i32,
        timer_channel: i32,
        scale_route_id: u32,
        scale: f32,
        offset: f32,
    ) -> Self {
        Self {
            node,
            route_id,
            timer_interface,
            timer_port,
            timer_channel,
            scale_route_id,
            scale,
            offset,
            output_value: 0.0,
            pending_value: false,
        }
    }

    fn node(&self) -> u32 {
        self.node
    }

    fn timer_input_key(&self) -> (u32, u16, i32, i32) {
        (
            self.node,
            self.timer_interface,
            self.timer_port,
            self.timer_channel,
        )
    }

    fn config_matches(
        &self,
        node: u32,
        route_id: u32,
        timer_interface: u16,
        timer_port: i32,
        timer_channel: i32,
        scale_route_id: u32,
    ) -> bool {
        self.node == node
            && self.route_id == route_id
            && self.timer_interface == timer_interface
            && self.timer_port == timer_port
            && self.timer_channel == timer_channel
            && self.scale_route_id == scale_route_id
    }

    fn reset(&mut self) {
        self.output_value = 0.0;
        self.pending_value = false;
    }

    fn update_timer(&mut self, events: &[TimerChannelEvent]) {
        let Some(event) = events.last() else {
            return;
        };
        self.output_value = event.value * self.scale + self.offset;
        self.pending_value = true;
    }

    fn take_scalar_event(&mut self, elapsed_ns: u64, scale_value: f32) -> Option<ScalarEvent> {
        if !self.pending_value {
            return None;
        }
        self.pending_value = false;
        Some(ScalarEvent {
            value: self.output_value * scale_value,
            timestamp_ns: elapsed_ns,
        })
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CanSignalComparison {
    pub bus: u8,
    pub message_id: u32,
    pub signal_index: u32,
    pub expected: f64,
    pub tolerance: f64,
    pub comparison: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SchedulerCallbackContext {
    pub elapsed_ns: u64,
    pub delta_ns: u64,
    pub fast_forward: bool,
}

#[derive(Clone, Copy)]
enum ClusterNodeScheduler {
    RustRuntimeModel,
    External {
        run_for: ClusterNodeRunForFn,
        fast_forward_for: ClusterNodeFastForwardForFn,
        next_step: ClusterNodeNextStepFn,
    },
    Python {
        scheduled: Option<ClusterPythonScheduledFn>,
        period_ns: u64,
        next_due_ns: u64,
        input_pending: bool,
    },
}

#[derive(Clone, Copy)]
struct ClusterNode {
    scheduler: ClusterNodeScheduler,
    reset: Option<ClusterNodeResetFn>,
    online: bool,
    elapsed_ns: u64,
}

impl ClusterNode {
    fn next_step(&self, cluster_elapsed_ns: u64, max_step_ns: u64) -> u64 {
        match self.scheduler {
            ClusterNodeScheduler::RustRuntimeModel => max_step_ns,
            ClusterNodeScheduler::External { next_step, .. } => unsafe { next_step(max_step_ns) },
            ClusterNodeScheduler::Python {
                period_ns,
                next_due_ns,
                input_pending,
                ..
            } => {
                if input_pending || period_ns == 0 {
                    max_step_ns
                } else if next_due_ns <= cluster_elapsed_ns {
                    0
                } else {
                    (next_due_ns - cluster_elapsed_ns).min(max_step_ns)
                }
            }
        }
    }

    fn run_for(&mut self, delta_ns: u64, fast_forward: bool) {
        match self.scheduler {
            ClusterNodeScheduler::RustRuntimeModel => {}
            ClusterNodeScheduler::External {
                run_for,
                fast_forward_for,
                ..
            } => {
                if fast_forward {
                    unsafe { fast_forward_for(delta_ns) };
                } else {
                    unsafe { run_for(delta_ns) };
                }
            }
            ClusterNodeScheduler::Python { .. } => {}
        }
        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
    }

    fn mark_input_pending(&mut self) {
        if let ClusterNodeScheduler::Python { input_pending, .. } = &mut self.scheduler {
            *input_pending = true;
        }
    }

    fn run_due_python(&mut self, cluster_elapsed_ns: u64, fast_forward: bool) {
        let ClusterNodeScheduler::Python {
            scheduled,
            period_ns,
            next_due_ns,
            input_pending,
        } = &mut self.scheduler
        else {
            return;
        };

        let due_to_period = *period_ns != 0 && *next_due_ns <= cluster_elapsed_ns;
        let due_to_input = *input_pending;
        if !due_to_period && !due_to_input {
            return;
        }

        if let Some(scheduled) = scheduled {
            let context = SchedulerCallbackContext {
                elapsed_ns: cluster_elapsed_ns,
                delta_ns: cluster_elapsed_ns.saturating_sub(self.elapsed_ns),
                fast_forward,
            };
            unsafe {
                scheduled(&context);
            };
        }
        self.elapsed_ns = cluster_elapsed_ns;
        *input_pending = false;
        if *period_ns != 0 {
            while *next_due_ns <= cluster_elapsed_ns {
                *next_due_ns = next_due_ns.saturating_add(*period_ns);
            }
        }
    }
}

#[derive(Clone, Copy)]
struct ClusterCanRoute {
    source_node: u32,
    source_bus: u8,
    source_tx_count: ClusterCanTxCountFn,
    source_recv_events: ClusterCanRecvEventsFn,
    sink_node: Option<u32>,
    sink_bus: u8,
    sink_send_many: Option<ClusterCanSendManyFn>,
}

#[derive(Clone, Copy)]
struct ClusterCanSink {
    sink_node: u32,
    sink_bus: u8,
    sink_send_many: ClusterCanSendManyFn,
}

struct ClusterCanRouteGroup {
    source_node: u32,
    source_bus: u8,
    source_tx_count: ClusterCanTxCountFn,
    source_recv_events: ClusterCanRecvEventsFn,
    sinks: Vec<ClusterCanSink>,
}

#[derive(Clone, Copy)]
struct ClusterTimerRoute {
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: ClusterTimerCountFn,
    source_recv_many: ClusterTimerRecvManyFn,
    sink_node: u32,
    sink_send_many: ClusterTimerSendManyFn,
}

#[derive(Clone, Copy)]
struct ClusterTimerSink {
    sink_node: u32,
    sink_send_many: ClusterTimerSendManyFn,
}

struct ClusterTimerRouteGroup {
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: ClusterTimerCountFn,
    source_recv_many: ClusterTimerRecvManyFn,
    sinks: Vec<ClusterTimerSink>,
}

#[derive(Clone, Copy)]
struct ClusterSpiRoute {
    source_node: u32,
    device: i32,
    source_count: ClusterSpiCountFn,
    source_recv_many: ClusterSpiRecvManyFn,
    sink_node: u32,
    sink_send_many: ClusterSpiSendManyFn,
}

#[derive(Clone, Copy)]
struct ClusterSpiSink {
    sink_node: u32,
    sink_send_many: ClusterSpiSendManyFn,
}

struct ClusterSpiRouteGroup {
    source_node: u32,
    device: i32,
    source_count: ClusterSpiCountFn,
    source_recv_many: ClusterSpiRecvManyFn,
    sinks: Vec<ClusterSpiSink>,
}

#[derive(Clone, Copy)]
enum ClusterScalarSink {
    SendMany(ClusterScalarSendManyFn),
    State {
        route_id: u32,
    },
    Native {
        sink_id: i32,
        value_scale: f32,
        set_value: ClusterScalarSinkSetFn,
    },
}

impl ClusterScalarSink {
    fn send_many(&self, events: &[ScalarEvent]) -> u32 {
        match self {
            Self::SendMany(send_many) => unsafe {
                send_many(
                    events.as_ptr(),
                    events.len().min(u32::MAX as usize) as u32,
                )
            },
            Self::State { .. } => events.len().min(u32::MAX as usize) as u32,
            Self::Native {
                sink_id,
                value_scale,
                set_value,
            } => {
                for event in events {
                    unsafe { set_value(*sink_id, event.value * *value_scale) };
                }
                events.len().min(u32::MAX as usize) as u32
            }
        }
    }
}

#[derive(Clone, Copy)]
struct ClusterScalarRoute {
    source_node: u32,
    route_id: u32,
    source_count: ClusterScalarCountFn,
    source_recv_many: ClusterScalarRecvManyFn,
    sink_node: u32,
    sink: ClusterScalarSink,
}

#[derive(Clone, Copy)]
struct ClusterScalarSinkRoute {
    sink_node: u32,
    sink: ClusterScalarSink,
}

struct ClusterScalarRouteGroup {
    source_node: u32,
    route_id: u32,
    source_count: ClusterScalarCountFn,
    source_recv_many: ClusterScalarRecvManyFn,
    sinks: Vec<ClusterScalarSinkRoute>,
}

#[derive(Clone, Copy)]
struct ClusterCanRecord {
    source_node: u32,
    bus: u8,
    event: CanEvent,
}

#[derive(Clone, Copy)]
struct ClusterTimerRecord {
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    event: TimerChannelEvent,
}

#[derive(Clone, Copy)]
struct ClusterSpiRecord {
    source_node: u32,
    device: i32,
    transaction: SpiTransaction,
}

#[derive(Clone, Copy)]
struct ClusterScalarRecord {
    source_node: u32,
    route_id: u32,
    event: ScalarEvent,
}

#[derive(Default)]
struct ClusterRuntime {
    nodes: Vec<ClusterNode>,
    can_route_group_indexes: HashMap<(u32, u8), usize>,
    can_route_groups: Vec<ClusterCanRouteGroup>,
    timer_route_group_indexes: HashMap<(u32, u16, i32, i32), usize>,
    timer_route_groups: Vec<ClusterTimerRouteGroup>,
    spi_route_group_indexes: HashMap<(u32, i32), usize>,
    spi_route_groups: Vec<ClusterSpiRouteGroup>,
    scalar_route_group_indexes: HashMap<(u32, u32), usize>,
    scalar_route_groups: Vec<ClusterScalarRouteGroup>,
    scalar_states: HashMap<(u32, u32), f32>,
    periodic_can_sources: Vec<PeriodicCanSource>,
    native_can_source_events: VecDeque<ClusterCanRecord>,
    battery_sources: Vec<BatterySourceModel>,
    battery_voltage_indexes: HashMap<(u32, u32), Vec<usize>>,
    timer_scaled_scalar_sources: Vec<TimerScaledScalarSource>,
    timer_scaled_scalar_timer_indexes: HashMap<(u32, u16, i32, i32), Vec<usize>>,
    dc_load_voltage_route_indexes: HashMap<(u32, u32), Vec<usize>>,
    dc_loads: Vec<DcLoadModel>,
    dc_load_voltage_indexes: HashMap<(u32, u32), Vec<usize>>,
    dc_load_current_indexes: HashMap<(u32, u32), Vec<usize>>,
    can_records: VecDeque<ClusterCanRecord>,
    timer_records: VecDeque<ClusterTimerRecord>,
    spi_records: VecDeque<ClusterSpiRecord>,
    scalar_records: VecDeque<ClusterScalarRecord>,
    elapsed_ns: u64,
}

impl ClusterRuntime {
    fn reset(&mut self) {
        self.nodes.clear();
        self.can_route_group_indexes.clear();
        self.can_route_groups.clear();
        self.timer_route_group_indexes.clear();
        self.timer_route_groups.clear();
        self.spi_route_group_indexes.clear();
        self.spi_route_groups.clear();
        self.scalar_route_group_indexes.clear();
        self.scalar_route_groups.clear();
        self.scalar_states.clear();
        self.periodic_can_sources.clear();
        self.native_can_source_events.clear();
        self.battery_sources.clear();
        self.battery_voltage_indexes.clear();
        self.timer_scaled_scalar_sources.clear();
        self.timer_scaled_scalar_timer_indexes.clear();
        self.dc_load_voltage_route_indexes.clear();
        self.dc_loads.clear();
        self.dc_load_voltage_indexes.clear();
        self.dc_load_current_indexes.clear();
        self.can_records.clear();
        self.timer_records.clear();
        self.spi_records.clear();
        self.scalar_records.clear();
        self.elapsed_ns = 0;
    }

    fn add_node(
        &mut self,
        run_for: ClusterNodeRunForFn,
        fast_forward_for: ClusterNodeFastForwardForFn,
        next_step: ClusterNodeNextStepFn,
        reset: ClusterNodeResetFn,
        online: bool,
    ) -> u32 {
        self.nodes.push(ClusterNode {
            scheduler: ClusterNodeScheduler::External {
                run_for,
                fast_forward_for,
                next_step,
            },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_rust_runtime_model_node(&mut self, online: bool) -> u32 {
        self.nodes.push(ClusterNode {
            scheduler: ClusterNodeScheduler::RustRuntimeModel,
            reset: None,
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_python_node(
        &mut self,
        scheduled: Option<ClusterPythonScheduledFn>,
        reset: ClusterNodeResetFn,
        period_ns: u64,
        online: bool,
    ) -> u32 {
        self.nodes.push(ClusterNode {
            scheduler: ClusterNodeScheduler::Python {
                scheduled,
                period_ns,
                next_due_ns: period_ns,
                input_pending: false,
            },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_can_route(&mut self, route: ClusterCanRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none() {
            return false;
        }
        if let Some(sink_node) = route.sink_node {
            if self.nodes.get(sink_node as usize).is_none() {
                return false;
            }
        }
        if route.sink_node.is_some() != route.sink_send_many.is_some() {
            return false;
        }
        self.upsert_can_route_group(route);
        true
    }

    fn add_timer_route(&mut self, route: ClusterTimerRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.upsert_timer_route_group(route);
        true
    }

    fn add_timer_source(
        &mut self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
        source_count: ClusterTimerCountFn,
        source_recv_many: ClusterTimerRecvManyFn,
    ) -> bool {
        if self.nodes.get(source_node as usize).is_none() {
            return false;
        }
        let key = (source_node, interface, port, channel);
        self.timer_route_group_indexes.entry(key).or_insert_with(|| {
            self.timer_route_groups.push(ClusterTimerRouteGroup {
                source_node,
                interface,
                port,
                channel,
                source_count,
                source_recv_many,
                sinks: Vec::new(),
            });
            self.timer_route_groups.len() - 1
        });
        true
    }

    fn add_spi_route(&mut self, route: ClusterSpiRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.upsert_spi_route_group(route);
        true
    }

    fn add_scalar_route(&mut self, route: ClusterScalarRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.upsert_scalar_route_group(route);
        true
    }

    fn add_scalar_state_sink(&mut self, node: u32, route_id: u32, initial_value: f32) -> bool {
        if self.nodes.get(node as usize).is_none() || !initial_value.is_finite() {
            return false;
        }
        self.scalar_states.insert((node, route_id), initial_value);
        true
    }

    fn add_scalar_state_route(
        &mut self,
        source_node: u32,
        route_id: u32,
        source_count: ClusterScalarCountFn,
        source_recv_many: ClusterScalarRecvManyFn,
        sink_node: u32,
        sink_route_id: u32,
    ) -> bool {
        if !self.add_scalar_route(ClusterScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ClusterScalarSink::State {
                route_id: sink_route_id,
            },
        }) {
            return false;
        }

        let events = self.native_scalar_events(source_node, route_id);
        if let Some(event) = events.last() {
            self.scalar_states
                .insert((sink_node, sink_route_id), event.value);
        }
        self.scalar_records
            .extend(events.into_iter().map(|event| ClusterScalarRecord {
                source_node,
                route_id,
                event,
            }));
        true
    }

    fn upsert_can_route_group(&mut self, route: ClusterCanRoute) {
        let key = (route.source_node, route.source_bus);
        let group_index = *self.can_route_group_indexes.entry(key).or_insert_with(|| {
            self.can_route_groups.push(ClusterCanRouteGroup {
                source_node: route.source_node,
                source_bus: route.source_bus,
                source_tx_count: route.source_tx_count,
                source_recv_events: route.source_recv_events,
                sinks: Vec::new(),
            });
            self.can_route_groups.len() - 1
        });
        if let (Some(sink_node), Some(sink_send_many)) = (route.sink_node, route.sink_send_many) {
            self.can_route_groups[group_index].sinks.push(ClusterCanSink {
                sink_node,
                sink_bus: route.sink_bus,
                sink_send_many,
            });
        }
    }

    fn upsert_timer_route_group(&mut self, route: ClusterTimerRoute) {
        let key = (
            route.source_node,
            route.interface,
            route.port,
            route.channel,
        );
        let group_index = *self.timer_route_group_indexes.entry(key).or_insert_with(|| {
            self.timer_route_groups.push(ClusterTimerRouteGroup {
                source_node: route.source_node,
                interface: route.interface,
                port: route.port,
                channel: route.channel,
                source_count: route.source_count,
                source_recv_many: route.source_recv_many,
                sinks: Vec::new(),
            });
            self.timer_route_groups.len() - 1
        });
        self.timer_route_groups[group_index]
            .sinks
            .push(ClusterTimerSink {
                sink_node: route.sink_node,
                sink_send_many: route.sink_send_many,
            });
    }

    fn upsert_spi_route_group(&mut self, route: ClusterSpiRoute) {
        let key = (route.source_node, route.device);
        let group_index = *self.spi_route_group_indexes.entry(key).or_insert_with(|| {
            self.spi_route_groups.push(ClusterSpiRouteGroup {
                source_node: route.source_node,
                device: route.device,
                source_count: route.source_count,
                source_recv_many: route.source_recv_many,
                sinks: Vec::new(),
            });
            self.spi_route_groups.len() - 1
        });
        self.spi_route_groups[group_index].sinks.push(ClusterSpiSink {
            sink_node: route.sink_node,
            sink_send_many: route.sink_send_many,
        });
    }

    fn upsert_scalar_route_group(&mut self, route: ClusterScalarRoute) {
        let key = (route.source_node, route.route_id);
        let group_index = *self
            .scalar_route_group_indexes
            .entry(key)
            .or_insert_with(|| {
                self.scalar_route_groups.push(ClusterScalarRouteGroup {
                    source_node: route.source_node,
                    route_id: route.route_id,
                    source_count: route.source_count,
                    source_recv_many: route.source_recv_many,
                    sinks: Vec::new(),
                });
                self.scalar_route_groups.len() - 1
            });
        self.scalar_route_groups[group_index]
            .sinks
            .push(ClusterScalarSinkRoute {
                sink_node: route.sink_node,
                sink: route.sink,
            });
    }

    fn add_periodic_can_source(
        &mut self,
        node: u32,
        bus: u8,
        period_ns: u64,
        packet: CanPacket,
    ) -> u32 {
        if self.nodes.get(node as usize).is_none() || period_ns == 0 {
            return u32::MAX;
        }
        self.periodic_can_sources
            .push(PeriodicCanSource::new(node, bus, period_ns, packet));
        (self.periodic_can_sources.len() - 1) as u32
    }

    fn update_periodic_can_source(&mut self, handle: u32, packet: CanPacket) -> bool {
        let Some(source) = self.periodic_can_sources.get_mut(handle as usize) else {
            return false;
        };
        source.update_packet(packet);
        true
    }

    fn send_native_can_source_event(&mut self, node: u32, bus: u8, packet: CanPacket) -> bool {
        if self.nodes.get(node as usize).is_none() {
            return false;
        }
        self.native_can_source_events.push_back(ClusterCanRecord {
            source_node: node,
            bus,
            event: CanEvent {
                bus,
                timestamp_ns: self.elapsed_ns,
                packet,
            },
        });
        true
    }

    fn add_dc_load(
        &mut self,
        node: u32,
        voltage_route_id: u32,
        current_route_id: u32,
        resistance_ohms: f32,
        inductance_henrys: f32,
        capacitance_farads: f32,
        scheduler_period_ns: u64,
    ) -> bool {
        if self.nodes.get(node as usize).is_none() {
            return false;
        }
        self.dc_loads.retain(|load| {
            !load.config_matches(
                node,
                voltage_route_id,
                current_route_id,
            )
        });
        self.dc_loads.push(DcLoadModel::new(
            node,
            voltage_route_id,
            current_route_id,
            resistance_ohms,
            inductance_henrys,
            capacitance_farads,
            scheduler_period_ns,
            self.elapsed_ns,
        ));
        self.rebuild_dc_load_indexes();
        true
    }

    fn add_battery_source(
        &mut self,
        node: u32,
        voltage_route_id: u32,
        voltage: f32,
        internal_resistance_ohms: f32,
        capacity_amp_hours: f32,
    ) -> bool {
        if self.nodes.get(node as usize).is_none()
            || !voltage.is_finite()
            || voltage < 0.0
            || !internal_resistance_ohms.is_finite()
            || internal_resistance_ohms < 0.0
            || capacity_amp_hours <= 0.0
        {
            return false;
        }
        self.battery_sources
            .retain(|source| !source.config_matches(node, voltage_route_id));
        self.battery_sources.push(BatterySourceModel::new(
            node,
            voltage_route_id,
            voltage,
            internal_resistance_ohms,
            capacity_amp_hours,
        ));
        self.rebuild_battery_source_indexes();
        true
    }

    fn add_timer_scaled_scalar_source(
        &mut self,
        node: u32,
        route_id: u32,
        timer_interface: u16,
        timer_port: i32,
        timer_channel: i32,
        scale_route_id: u32,
        scale: f32,
        offset: f32,
    ) -> bool {
        if self.nodes.get(node as usize).is_none() || !scale.is_finite() || !offset.is_finite() {
            return false;
        }
        self.timer_scaled_scalar_sources.retain(|source| {
            !source.config_matches(
                node,
                route_id,
                timer_interface,
                timer_port,
                timer_channel,
                scale_route_id,
            )
        });
        self.timer_scaled_scalar_sources
            .push(TimerScaledScalarSource::new(
                node,
                route_id,
                timer_interface,
                timer_port,
                timer_channel,
                scale_route_id,
                scale,
                offset,
            ));
        self.rebuild_timer_scaled_scalar_indexes();
        true
    }

    fn add_dc_load_voltage_route(
        &mut self,
        source_node: u32,
        route_id: u32,
        sink_node: u32,
    ) -> bool {
        if self.nodes.get(source_node as usize).is_none()
            || self.nodes.get(sink_node as usize).is_none()
        {
            return false;
        }
        let Some(load_indexes) = self.dc_load_voltage_indexes.get(&(sink_node, route_id)) else {
            return false;
        };
        let route_indexes = self
            .dc_load_voltage_route_indexes
            .entry((source_node, route_id))
            .or_default();
        for index in load_indexes {
            if !route_indexes.contains(index) {
                route_indexes.push(*index);
            }
        }
        true
    }

    fn rebuild_timer_scaled_scalar_indexes(&mut self) {
        self.timer_scaled_scalar_timer_indexes.clear();
        for (index, source) in self.timer_scaled_scalar_sources.iter().enumerate() {
            self.timer_scaled_scalar_timer_indexes
                .entry(source.timer_input_key())
                .or_default()
                .push(index);
        }
    }

    fn rebuild_dc_load_indexes(&mut self) {
        self.dc_load_voltage_indexes.clear();
        self.dc_load_current_indexes.clear();
        for (index, load) in self.dc_loads.iter().enumerate() {
            self.dc_load_voltage_indexes
                .entry(load.voltage_input_key())
                .or_default()
                .push(index);
            self.dc_load_current_indexes
                .entry(load.current_output_key())
                .or_default()
                .push(index);
        }
    }

    fn rebuild_battery_source_indexes(&mut self) {
        self.battery_voltage_indexes.clear();
        for (index, source) in self.battery_sources.iter().enumerate() {
            self.battery_voltage_indexes
                .entry(source.voltage_output_key())
                .or_default()
                .push(index);
        }
    }

    fn set_node_online(&mut self, node_index: u32, online: bool) -> bool {
        let elapsed_ns = self.elapsed_ns;
        let reset_runtime_models;
        let Some(node) = self.nodes.get_mut(node_index as usize) else {
            return false;
        };
        if node.online && !online {
            if let Some(reset) = node.reset {
                unsafe { reset() };
            }
            node.elapsed_ns = 0;
            reset_runtime_models = matches!(node.scheduler, ClusterNodeScheduler::RustRuntimeModel);
            if let ClusterNodeScheduler::Python {
                period_ns,
                next_due_ns,
                input_pending,
                ..
            } = &mut node.scheduler
            {
                *next_due_ns = *period_ns;
                *input_pending = false;
            }
        } else {
            reset_runtime_models = false;
        }
        if !node.online && online {
            if let ClusterNodeScheduler::Python {
                period_ns,
                next_due_ns,
                input_pending,
                ..
            } = &mut node.scheduler
            {
                *next_due_ns = elapsed_ns.saturating_add(*period_ns);
                *input_pending = false;
            }
        }
        node.online = online;
        if reset_runtime_models {
            self.reset_rust_runtime_node_models(node_index, elapsed_ns);
        }
        true
    }

    fn reset_rust_runtime_node_models(&mut self, node: u32, elapsed_ns: u64) {
        for source in self
            .timer_scaled_scalar_sources
            .iter_mut()
            .filter(|source| source.node() == node)
        {
            source.reset();
        }
        for source in self
            .battery_sources
            .iter_mut()
            .filter(|source| source.node() == node)
        {
            source.reset();
        }
        for load in self.dc_loads.iter_mut().filter(|load| load.node() == node) {
            load.reset(elapsed_ns);
        }
    }

    fn next_cluster_step(&self, max_step_ns: u64) -> u64 {
        let node_step = self
            .nodes
            .iter()
            .filter(|node| node.online)
            .map(|node| node.next_step(self.elapsed_ns, max_step_ns))
            .min()
            .unwrap_or(max_step_ns);
        let dc_load_step = self
            .dc_loads
            .iter()
            .filter(|load| self.node_online(load.node()))
            .map(|load| load.next_step_ns(self.elapsed_ns, max_step_ns))
            .min()
            .unwrap_or(max_step_ns);
        node_step.min(dc_load_step).min(max_step_ns)
    }

    fn node_online(&self, node: u32) -> bool {
        self.nodes
            .get(node as usize)
            .map(|node| node.online)
            .unwrap_or(false)
    }

    fn run_next_step(&mut self, remaining_ns: u64, max_step_ns: u64, fast_forward: bool) -> u64 {
        if remaining_ns == 0 || max_step_ns == 0 {
            return 0;
        }

        self.run_due_python_nodes(fast_forward);

        let max_delta_ns = remaining_ns.min(max_step_ns);
        let delta_ns = if fast_forward {
            max_delta_ns
        } else {
            self.next_cluster_step(max_delta_ns)
        };
        if delta_ns == 0 {
            return 0;
        }

        for node in self.nodes.iter_mut().filter(|node| node.online) {
            node.run_for(delta_ns, fast_forward);
        }

        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
        self.run_due_python_nodes(fast_forward);
        self.route_can();
        self.route_scalar_state_inputs();
        self.route_timer();
        self.run_dc_loads();
        self.route_spi();
        self.route_scalar();
        self.run_due_python_nodes(fast_forward);
        delta_ns
    }

    fn run_due_python_nodes(&mut self, fast_forward: bool) {
        let elapsed_ns = self.elapsed_ns;
        for node in self.nodes.iter_mut().filter(|node| node.online) {
            node.run_due_python(elapsed_ns, fast_forward);
        }
    }

    fn mark_input_pending(&mut self, node: u32) {
        let Some(node) = self.nodes.get_mut(node as usize) else {
            return;
        };
        node.mark_input_pending();
    }

    fn route_can(&mut self) {
        let nodes_online: Vec<bool> = self.nodes.iter().map(|node| node.online).collect();
        let mut input_pending_nodes = Vec::new();

        for source in self.periodic_can_sources.iter_mut() {
            if !nodes_online
                .get(source.node() as usize)
                .copied()
                .unwrap_or(false)
            {
                continue;
            }
            let Some(event) = source.emit_if_due(self.elapsed_ns) else {
                continue;
            };
            if let Some(group_index) = self
                .can_route_group_indexes
                .get(&(source.node(), source.bus()))
                .copied()
            {
                for sink in &self.can_route_groups[group_index].sinks {
                    unsafe {
                        (sink.sink_send_many)(sink.sink_bus, &event.packet, 1);
                    };
                    input_pending_nodes.push(sink.sink_node);
                }
            }
            self.can_records.push_back(ClusterCanRecord {
                source_node: source.node(),
                bus: source.bus(),
                event,
            });
        }
        for sink_node in input_pending_nodes.drain(..) {
            self.mark_input_pending(sink_node);
        }

        while let Some(record) = self.native_can_source_events.pop_front() {
            if !nodes_online
                .get(record.source_node as usize)
                .copied()
                .unwrap_or(false)
            {
                continue;
            }
            if let Some(group_index) = self
                .can_route_group_indexes
                .get(&(record.source_node, record.bus))
                .copied()
            {
                for sink in &self.can_route_groups[group_index].sinks {
                    let accepted = unsafe {
                        (sink.sink_send_many)(sink.sink_bus, &record.event.packet, 1)
                    };
                    if accepted > 0 {
                        input_pending_nodes.push(sink.sink_node);
                    }
                }
            }
            self.can_records.push_back(record);
        }
        for sink_node in input_pending_nodes.drain(..) {
            self.mark_input_pending(sink_node);
        }

        for group in &self.can_route_groups {
            let Some(source) = self.nodes.get(group.source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let pending = unsafe { (group.source_tx_count)(group.source_bus) };
            if pending == 0 {
                continue;
            }

            let mut events = vec![CanEvent::default(); pending as usize];
            let count = unsafe {
                (group.source_recv_events)(group.source_bus, events.as_mut_ptr(), pending)
            };
            let count = count.min(pending) as usize;
            if count == 0 {
                continue;
            }
            events.truncate(count);

            let packets: Vec<CanPacket> = events.iter().map(|event| event.packet).collect();
            if !packets.is_empty() {
                for sink in &group.sinks {
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

            self.can_records
                .extend(events.into_iter().map(|event| ClusterCanRecord {
                    source_node: group.source_node,
                    bus: group.source_bus,
                    event,
                }));
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
    }

    fn route_timer(&mut self) {
        let mut input_pending_nodes = Vec::new();

        for group_index in 0..self.timer_route_groups.len() {
            let source_node = self.timer_route_groups[group_index].source_node;
            let interface = self.timer_route_groups[group_index].interface;
            let port = self.timer_route_groups[group_index].port;
            let channel = self.timer_route_groups[group_index].channel;
            let source_count = self.timer_route_groups[group_index].source_count;
            let source_recv_many = self.timer_route_groups[group_index].source_recv_many;

            let Some(source) = self.nodes.get(source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let pending = unsafe { source_count(port, channel) };
            if pending == 0 {
                continue;
            }

            let mut events = vec![TimerChannelEvent::default(); pending as usize];
            let count = unsafe { source_recv_many(port, channel, events.as_mut_ptr(), pending) };
            let count = count.min(pending) as usize;
            if count == 0 {
                continue;
            }
            events.truncate(count);

            self.update_timer_scaled_scalar_source(source_node, interface, port, channel, &events);

            let sink_count = self.timer_route_groups[group_index].sinks.len();
            for sink_index in 0..sink_count {
                let sink = self.timer_route_groups[group_index].sinks[sink_index];
                let accepted = unsafe {
                    (sink.sink_send_many)(
                        events.as_ptr(),
                        events.len().min(u32::MAX as usize) as u32,
                    )
                };
                if accepted > 0 {
                    input_pending_nodes.push(sink.sink_node);
                }
            }

            self.timer_records
                .extend(events.into_iter().map(|event| ClusterTimerRecord {
                    source_node,
                    interface,
                    port,
                    channel,
                    event,
                }));
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
    }

    fn update_timer_scaled_scalar_source(
        &mut self,
        sink_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
        events: &[TimerChannelEvent],
    ) {
        let key = (sink_node, interface, port, channel);
        let index_count = self
            .timer_scaled_scalar_timer_indexes
            .get(&key)
            .map(|indexes| indexes.len())
            .unwrap_or(0);
        for index_position in 0..index_count {
            let Some(index) = self
                .timer_scaled_scalar_timer_indexes
                .get(&key)
                .and_then(|indexes| indexes.get(index_position))
                .copied()
            else {
                continue;
            };
            let Some(source_view) = self.timer_scaled_scalar_sources.get(index) else {
                continue;
            };
            let route_id = source_view.route_id;
            let scale_route_id = source_view.scale_route_id;
            let scale_value = if scale_route_id == 0 {
                1.0
            } else {
                *self
                    .scalar_states
                    .get(&(sink_node, scale_route_id))
                    .unwrap_or(&0.0)
            };
            if let Some(source) = self.timer_scaled_scalar_sources.get_mut(index) {
                source.update_timer(events);
                if let Some(event) = source.take_scalar_event(self.elapsed_ns, scale_value) {
                    self.update_dc_load_voltage_routes(sink_node, route_id, event);
                    self.scalar_records.push_back(ClusterScalarRecord {
                        source_node: sink_node,
                        route_id,
                        event,
                    });
                }
            }
        }
    }

    fn update_dc_load_voltage_routes(
        &mut self,
        source_node: u32,
        route_id: u32,
        event: ScalarEvent,
    ) {
        let Some(sink_nodes) = self
            .dc_load_voltage_route_indexes
            .get(&(source_node, route_id))
        else {
            return;
        };
        for index in sink_nodes.iter().copied() {
            if let Some(load) = self.dc_loads.get_mut(index) {
                load.update_voltage_event(event);
            }
        }
    }

    fn run_dc_loads(&mut self) {
        let elapsed_ns = self.elapsed_ns;
        for load in self.dc_loads.iter_mut() {
            if !self
                .nodes
                .get(load.node() as usize)
                .map(|node| node.online)
                .unwrap_or(false)
            {
                continue;
            }
            load.run_until(elapsed_ns);
        }
    }

    fn route_spi(&mut self) {
        let mut input_pending_nodes = Vec::new();

        for group in &self.spi_route_groups {
            let Some(source) = self.nodes.get(group.source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let pending = unsafe { (group.source_count)(group.device) };
            if pending == 0 {
                continue;
            }

            let mut transactions = vec![SpiTransaction::default(); pending as usize];
            let count = unsafe {
                (group.source_recv_many)(group.device, transactions.as_mut_ptr(), pending)
            };
            let count = count.min(pending) as usize;
            if count == 0 {
                continue;
            }
            transactions.truncate(count);

            for sink in &group.sinks {
                let accepted = unsafe {
                    (sink.sink_send_many)(
                        transactions.as_ptr(),
                        transactions.len().min(u32::MAX as usize) as u32,
                    )
                };
                if accepted > 0 {
                    input_pending_nodes.push(sink.sink_node);
                }
            }

            self.spi_records.extend(
                transactions
                    .into_iter()
                    .map(|transaction| ClusterSpiRecord {
                        source_node: group.source_node,
                        device: group.device,
                        transaction,
                    }),
            );
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
    }

    fn route_scalar_state_inputs(&mut self) {
        let mut input_pending_nodes = Vec::new();

        for group_index in 0..self.scalar_route_groups.len() {
            let has_state_sink = self.scalar_route_groups[group_index]
                .sinks
                .iter()
                .any(|sink| matches!(sink.sink, ClusterScalarSink::State { .. }));
            if !has_state_sink {
                continue;
            }

            let source_node = self.scalar_route_groups[group_index].source_node;
            let route_id = self.scalar_route_groups[group_index].route_id;
            let source_count = self.scalar_route_groups[group_index].source_count;
            let source_recv_many = self.scalar_route_groups[group_index].source_recv_many;

            let Some(source) = self.nodes.get(source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let mut events = self.native_scalar_events(source_node, route_id);
            if events.is_empty() {
                let pending = unsafe { source_count() };
                if pending == 0 {
                    continue;
                }

                events = vec![ScalarEvent::default(); pending as usize];
                let count = unsafe { source_recv_many(events.as_mut_ptr(), pending) };
                let count = count.min(pending) as usize;
                if count == 0 {
                    continue;
                }
                events.truncate(count);
            }

            let sink_count = self.scalar_route_groups[group_index].sinks.len();
            for sink_index in 0..sink_count {
                let sink = self.scalar_route_groups[group_index].sinks[sink_index];
                let ClusterScalarSink::State { route_id } = sink.sink else {
                    continue;
                };
                if let Some(event) = events.last() {
                    self.scalar_states
                        .insert((sink.sink_node, route_id), event.value);
                }
                input_pending_nodes.push(sink.sink_node);
            }

            self.scalar_records
                .extend(events.into_iter().map(|event| ClusterScalarRecord {
                    source_node,
                    route_id,
                    event,
                }));
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
    }

    fn route_scalar(&mut self) {
        let mut input_pending_nodes = Vec::new();

        for group_index in 0..self.scalar_route_groups.len() {
            let only_state_sinks = self.scalar_route_groups[group_index]
                .sinks
                .iter()
                .all(|sink| matches!(sink.sink, ClusterScalarSink::State { .. }));
            if only_state_sinks {
                continue;
            }
            let source_node = self.scalar_route_groups[group_index].source_node;
            let route_id = self.scalar_route_groups[group_index].route_id;
            let source_count = self.scalar_route_groups[group_index].source_count;
            let source_recv_many = self.scalar_route_groups[group_index].source_recv_many;

            let Some(source) = self.nodes.get(source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let mut events = self.native_scalar_events(source_node, route_id);
            if events.is_empty() {
                let pending = unsafe { source_count() };
                if pending == 0 {
                    continue;
                }

                events = vec![ScalarEvent::default(); pending as usize];
                let count = unsafe { source_recv_many(events.as_mut_ptr(), pending) };
                let count = count.min(pending) as usize;
                if count == 0 {
                    continue;
                }
                events.truncate(count);
            }

            let sink_count = self.scalar_route_groups[group_index].sinks.len();
            for sink_index in 0..sink_count {
                let sink = self.scalar_route_groups[group_index].sinks[sink_index];
                let accepted = match sink.sink {
                    ClusterScalarSink::State { route_id } => {
                        if let Some(event) = events.last() {
                            self.scalar_states
                                .insert((sink.sink_node, route_id), event.value);
                        }
                        events.len().min(u32::MAX as usize) as u32
                    }
                    _ => sink.sink.send_many(&events),
                };
                if accepted > 0 {
                    input_pending_nodes.push(sink.sink_node);
                }
            }

            self.scalar_records
                .extend(events.into_iter().map(|event| ClusterScalarRecord {
                    source_node,
                    route_id,
                    event,
                }));
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
    }

    fn native_scalar_events(&mut self, source_node: u32, route_id: u32) -> Vec<ScalarEvent> {
        let mut events = Vec::new();
        if let Some(indexes) = self.dc_load_current_indexes.get(&(source_node, route_id)) {
            for index in indexes.iter().copied() {
                if let Some(load) = self.dc_loads.get_mut(index) {
                    if let Some(event) = load.take_current_event(self.elapsed_ns) {
                        events.push(event);
                    }
                }
            }
        }
        if let Some(indexes) = self.battery_voltage_indexes.get(&(source_node, route_id)) {
            for index in indexes.iter().copied() {
                if let Some(source) = self.battery_sources.get_mut(index) {
                    if let Some(event) = source.take_voltage_event(self.elapsed_ns) {
                        events.push(event);
                    }
                }
            }
        }
        events
    }

    fn node_elapsed_ns(&self, node: u32) -> u64 {
        self.nodes
            .get(node as usize)
            .map(|node| node.elapsed_ns)
            .unwrap_or(0)
    }

    fn node_elapsed_ns_many(&self, out: &mut [u64]) -> u32 {
        let count = self.nodes.len().min(out.len()).min(u32::MAX as usize);
        for (slot, node) in out.iter_mut().zip(self.nodes.iter()).take(count) {
            *slot = node.elapsed_ns;
        }
        count as u32
    }

    fn latest_can_message(&self, source_node: u32, bus: u8, message_id: u32) -> Option<CanEvent> {
        self.can_records
            .iter()
            .rev()
            .find(|record| {
                record.source_node == source_node
                    && record.bus == bus
                    && record.event.packet.id == message_id
            })
            .map(|record| record.event)
    }

    fn latest_can_bus_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        self.can_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.bus == bus)
            .map(|record| record.event)
    }

    fn latest_can_signal(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
    ) -> Option<f64> {
        let event = self.latest_can_message(source_node, bus, message_id)?;
        let packet = can::CanPacket {
            id: event.packet.id,
            len: event.packet.len,
            data: event.packet.data,
        };
        can::decode_signal(bus, &packet, signal_name)
    }

    fn latest_can_signal_eq(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
        expected: f64,
        tolerance: f64,
    ) -> bool {
        let Some(value) = self.latest_can_signal(source_node, bus, message_id, signal_name) else {
            return false;
        };
        compare_value(value, expected, tolerance, COMPARE_EQ)
    }

    fn latest_can_signal_cmp(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
        expected: f64,
        tolerance: f64,
        comparison: u8,
    ) -> bool {
        let Some(value) = self.latest_can_signal(source_node, bus, message_id, signal_name) else {
            return false;
        };
        compare_value(value, expected, tolerance, comparison)
    }

    fn can_signal_comparisons_match(
        &self,
        source_node: u32,
        comparisons: &[CanSignalComparison],
    ) -> bool {
        comparisons.iter().all(|comparison| {
            let Some(signal_name) = can::codegen_tx_signal_name(comparison.signal_index) else {
                return false;
            };
            self.latest_can_signal_cmp(
                source_node,
                comparison.bus,
                comparison.message_id,
                signal_name,
                comparison.expected,
                comparison.tolerance,
                comparison.comparison,
            )
        })
    }

    fn latest_timer_event(
        &self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
    ) -> Option<TimerChannelEvent> {
        self.timer_records
            .iter()
            .rev()
            .find(|record| {
                record.source_node == source_node
                    && record.interface == interface
                    && record.port == port
                    && record.channel == channel
            })
            .map(|record| record.event)
    }

    fn latest_spi_transaction(&self, source_node: u32, device: i32) -> Option<SpiTransaction> {
        self.spi_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.device == device)
            .map(|record| record.transaction)
    }

    fn latest_scalar_event(&self, source_node: u32, route_id: u32) -> Option<ScalarEvent> {
        self.scalar_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.route_id == route_id)
            .map(|record| record.event)
    }
}

static CLUSTER_RUNTIME: LazyLock<Mutex<ClusterRuntime>> =
    LazyLock::new(|| Mutex::new(ClusterRuntime::default()));

fn compare_value(value: f64, expected: f64, tolerance: f64, comparison: u8) -> bool {
    match comparison {
        COMPARE_EQ => (value - expected).abs() <= tolerance,
        COMPARE_GT => value > expected + tolerance,
        COMPARE_GE => value >= expected - tolerance,
        COMPARE_LT => value < expected - tolerance,
        COMPARE_LE => value <= expected + tolerance,
        _ => false,
    }
}

unsafe fn function_pointer<T>(address: usize) -> Option<T> {
    if address == 0 {
        return None;
    }
    Some(unsafe { mem::transmute_copy(&address) })
}

unsafe fn c_str_to_str<'a>(value: *const c_char) -> Option<&'a str> {
    if value.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(value) }.to_str().ok()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_reset() {
    CLUSTER_RUNTIME.lock().unwrap().reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_node(
    run_for: usize,
    fast_forward_for: usize,
    next_step: usize,
    reset: usize,
    online: bool,
) -> u32 {
    let Some(run_for) = (unsafe { function_pointer::<ClusterNodeRunForFn>(run_for) }) else {
        return u32::MAX;
    };
    let Some(fast_forward_for) =
        (unsafe { function_pointer::<ClusterNodeFastForwardForFn>(fast_forward_for) })
    else {
        return u32::MAX;
    };
    let Some(next_step) = (unsafe { function_pointer::<ClusterNodeNextStepFn>(next_step) }) else {
        return u32::MAX;
    };
    let Some(reset) = (unsafe { function_pointer::<ClusterNodeResetFn>(reset) }) else {
        return u32::MAX;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_node(run_for, fast_forward_for, next_step, reset, online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_python_node(
    scheduled: usize,
    reset: usize,
    period_ns: u64,
    online: bool,
) -> u32 {
    let scheduled = unsafe { function_pointer::<ClusterPythonScheduledFn>(scheduled) };
    let Some(reset) = (unsafe { function_pointer::<ClusterNodeResetFn>(reset) }) else {
        return u32::MAX;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_python_node(scheduled, reset, period_ns, online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_rust_runtime_model_node(online: bool) -> u32 {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_rust_runtime_model_node(online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_set_node_online(node: u32, online: bool) -> bool {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .set_node_online(node, online)
}

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

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_can_route(ClusterCanRoute {
            source_node,
            source_bus,
            source_tx_count,
            source_recv_events,
            sink_node: if sink_node == u32::MAX {
                None
            } else {
                Some(sink_node)
            },
            sink_bus,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_timer_route(
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_send_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterTimerCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterTimerRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterTimerSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_timer_route(ClusterTimerRoute {
            source_node,
            interface,
            port,
            channel,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_timer_source(
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: usize,
    source_recv_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterTimerCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterTimerRecvManyFn>(source_recv_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_timer_source(
            source_node,
            interface,
            port,
            channel,
            source_count,
            source_recv_many,
        )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_spi_route(
    source_node: u32,
    device: i32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_send_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterSpiCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterSpiRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterSpiSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_spi_route(ClusterSpiRoute {
            source_node,
            device,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_send_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterScalarSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_scalar_route(ClusterScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ClusterScalarSink::SendMany(sink_send_many),
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_sink_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_id: i32,
    value_scale: f32,
    set_value: usize,
) -> bool {
    if !value_scale.is_finite() {
        return false;
    }
    let Some(source_count) = (unsafe { function_pointer::<ClusterScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(set_value) =
        (unsafe { function_pointer::<ClusterScalarSinkSetFn>(set_value) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_scalar_route(ClusterScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ClusterScalarSink::Native {
                sink_id,
                value_scale,
                set_value,
            },
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_state_sink(
    node: u32,
    route_id: u32,
    initial_value: f32,
) -> bool {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_scalar_state_sink(node, route_id, initial_value)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_state_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_route_id: u32,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_scalar_state_route(
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink_route_id,
        )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_dc_load_voltage_route(
    source_node: u32,
    route_id: u32,
    sink_node: u32,
) -> bool {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_dc_load_voltage_route(source_node, route_id, sink_node)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_timer_scaled_scalar_source(
    node: u32,
    route_id: u32,
    timer_interface: u16,
    timer_port: i32,
    timer_channel: i32,
    scale_route_id: u32,
    scale: f32,
    offset: f32,
) -> bool {
    CLUSTER_RUNTIME.lock().unwrap().add_timer_scaled_scalar_source(
        node,
        route_id,
        timer_interface,
        timer_port,
        timer_channel,
        scale_route_id,
        scale,
        offset,
    )
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
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_periodic_can_source(node, bus, period_ns, unsafe { *packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_update_periodic_can_source(
    handle: u32,
    packet: *const CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .update_periodic_can_source(handle, unsafe { *packet })
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
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .send_native_can_source_event(node, bus, unsafe { *packet })
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

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_dc_load(
    node: u32,
    voltage_route_id: u32,
    current_route_id: u32,
    resistance_ohms: f32,
    inductance_henrys: f32,
    capacitance_farads: f32,
    scheduler_period_ns: u64,
) -> bool {
    CLUSTER_RUNTIME.lock().unwrap().add_dc_load(
        node,
        voltage_route_id,
        current_route_id,
        resistance_ohms,
        inductance_henrys,
        capacitance_farads,
        scheduler_period_ns,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_battery_source(
    node: u32,
    voltage_route_id: u32,
    voltage: f32,
    internal_resistance_ohms: f32,
    capacity_amp_hours: f32,
) -> bool {
    CLUSTER_RUNTIME.lock().unwrap().add_battery_source(
        node,
        voltage_route_id,
        voltage,
        internal_resistance_ohms,
        capacity_amp_hours,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_timer_count(_port: i32, _channel: i32) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_timer_recv_many(
    _port: i32,
    _channel: i32,
    _events: *mut TimerChannelEvent,
    _capacity: u32,
) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_timer_send_many(
    _events: *const TimerChannelEvent,
    count: u32,
) -> u32 {
    count
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_scalar_count() -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_scalar_recv_many(
    _events: *mut ScalarEvent,
    _capacity: u32,
) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_scalar_send_many(
    _events: *const ScalarEvent,
    count: u32,
) -> u32 {
    count
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_for(
    duration_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
) {
    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(duration_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_eq(
    timeout_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
    source_node: u32,
    bus: u8,
    message_id: u32,
    signal_name: *const c_char,
    expected: f64,
    tolerance: f64,
) -> u64 {
    let Some(signal_name) = (unsafe { c_str_to_str(signal_name) }) else {
        return u64::MAX;
    };
    if timeout_ns == 0 || max_step_ns == 0 {
        return if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
        ) {
            0
        } else {
            u64::MAX
        };
    }

    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(timeout_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
        source_node,
        bus,
        message_id,
        signal_name,
        expected,
        tolerance,
    ) {
        return 0;
    }

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }

        if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
        ) {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_index_eq(
    timeout_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
    source_node: u32,
    bus: u8,
    message_id: u32,
    signal_index: u32,
    expected: f64,
    tolerance: f64,
) -> u64 {
    let Some(signal_name) = can::codegen_tx_signal_name(signal_index) else {
        return u64::MAX;
    };
    if timeout_ns == 0 || max_step_ns == 0 {
        return if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
        ) {
            0
        } else {
            u64::MAX
        };
    }

    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(timeout_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
        source_node,
        bus,
        message_id,
        signal_name,
        expected,
        tolerance,
    ) {
        return 0;
    }

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }

        if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
        ) {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_index_cmp(
    timeout_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
    source_node: u32,
    bus: u8,
    message_id: u32,
    signal_index: u32,
    expected: f64,
    tolerance: f64,
    comparison: u8,
) -> u64 {
    let Some(signal_name) = can::codegen_tx_signal_name(signal_index) else {
        return u64::MAX;
    };
    if timeout_ns == 0 || max_step_ns == 0 {
        return if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_cmp(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
            comparison,
        ) {
            0
        } else {
            u64::MAX
        };
    }

    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(timeout_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_cmp(
        source_node,
        bus,
        message_id,
        signal_name,
        expected,
        tolerance,
        comparison,
    ) {
        return 0;
    }

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }

        if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_cmp(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
            comparison,
        ) {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_comparisons(
    timeout_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
    source_node: u32,
    comparisons: *const CanSignalComparison,
    comparison_count: u32,
) -> u64 {
    if comparisons.is_null() {
        return u64::MAX;
    }
    let comparisons =
        unsafe { std::slice::from_raw_parts(comparisons, comparison_count as usize) };
    if comparisons.is_empty() {
        return u64::MAX;
    }
    if timeout_ns == 0 || max_step_ns == 0 {
        return if CLUSTER_RUNTIME
            .lock()
            .unwrap()
            .can_signal_comparisons_match(source_node, comparisons)
        {
            0
        } else {
            u64::MAX
        };
    }

    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(timeout_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    if CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .can_signal_comparisons_match(source_node, comparisons)
    {
        return 0;
    }

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }

        if CLUSTER_RUNTIME
            .lock()
            .unwrap()
            .can_signal_comparisons_match(source_node, comparisons)
        {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_elapsed_ns() -> u64 {
    CLUSTER_RUNTIME.lock().unwrap().elapsed_ns
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_node_elapsed_ns(node: u32) -> u64 {
    CLUSTER_RUNTIME.lock().unwrap().node_elapsed_ns(node)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_node_elapsed_ns_many(out: *mut u64, capacity: u32) -> u32 {
    if out.is_null() {
        return 0;
    }
    let out = unsafe { std::slice::from_raw_parts_mut(out, capacity as usize) };
    CLUSTER_RUNTIME.lock().unwrap().node_elapsed_ns_many(out)
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
    let Some(event) =
        CLUSTER_RUNTIME
            .lock()
            .unwrap()
            .latest_can_message(source_node, bus, message_id)
    else {
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
    let Some(event) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_can_bus_event(source_node, bus)
    else {
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
    let Some(signal_name) = (unsafe { c_str_to_str(signal_name) }) else {
        return false;
    };
    let Some(value) = CLUSTER_RUNTIME.lock().unwrap().latest_can_signal(
        source_node,
        bus,
        message_id,
        signal_name,
    ) else {
        return false;
    };
    unsafe { *out = value };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_timer_event(
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    out: *mut TimerChannelEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(event) =
        CLUSTER_RUNTIME
            .lock()
            .unwrap()
            .latest_timer_event(source_node, interface, port, channel)
    else {
        return false;
    };
    unsafe { *out = event };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_spi_transaction(
    source_node: u32,
    device: i32,
    out: *mut SpiTransaction,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(transaction) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_spi_transaction(source_node, device)
    else {
        return false;
    };
    unsafe { *out = transaction };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_scalar_event(
    source_node: u32,
    route_id: u32,
    out: *mut ScalarEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(event) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_scalar_event(source_node, route_id)
    else {
        return false;
    };
    unsafe { *out = event };
    true
}
