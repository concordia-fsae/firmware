use std::any::TypeId;
use std::collections::{HashMap, HashSet, VecDeque};
use std::ffi::CStr;
use std::marker::PhantomData;
use std::mem;
use std::os::raw::c_char;
use std::sync::{LazyLock, Mutex};

use super::battery_source::BatterySourceModel;
use super::can;
use super::dc_load::DcLoadModel;
use super::networks::{
    CanEndpoint, CanNetwork, ClusterCanRecord, ClusterCanRoute, ClusterSpiRoute, NetworkDataflow,
    RuntimeNetworks, SpiEndpoint, SpiNetwork,
};
use super::simple::PeriodicCanSource;

pub type ClusterNodeRunForFn = unsafe extern "C" fn(u64);
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
    scale_value: f32,
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
        scale_value: f32,
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
            scale_value,
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

    fn output_matches(&self, node: u32, route_id: u32) -> bool {
        self.node == node && self.route_id == route_id
    }

    fn config_equals(
        &self,
        node: u32,
        route_id: u32,
        timer_interface: u16,
        timer_port: i32,
        timer_channel: i32,
        scale_route_id: u32,
        scale: f32,
        offset: f32,
    ) -> bool {
        self.config_matches(
            node,
            route_id,
            timer_interface,
            timer_port,
            timer_channel,
            scale_route_id,
        ) && self.scale == scale
            && self.offset == offset
    }

    fn reset(&mut self) {
        self.output_value = 0.0;
        self.pending_value = false;
    }

    fn set_scale_value(&mut self, scale_route_id: u32, scale_value: f32) {
        if self.scale_route_id == scale_route_id {
            self.scale_value = scale_value;
        }
    }

    fn update_timer(&mut self, events: &[TimerChannelEvent]) {
        let Some(event) = events.last() else {
            return;
        };
        self.output_value = event.value * self.scale + self.offset;
        self.pending_value = true;
    }

    fn has_pending_value(&self) -> bool {
        self.pending_value
    }

    fn take_scalar_event(&mut self, elapsed_ns: u64) -> Option<ScalarEvent> {
        if !self.pending_value {
            return None;
        }
        self.pending_value = false;
        Some(ScalarEvent {
            value: self.output_value * self.scale_value,
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
}

#[derive(Clone, Copy)]
enum ClusterNodeScheduler {
    RustRuntimeModel,
    External {
        run_for: ClusterNodeRunForFn,
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
    fn needs_run_step(&self) -> bool {
        !matches!(self.scheduler, ClusterNodeScheduler::RustRuntimeModel)
    }

    fn is_python_scheduled(&self) -> bool {
        matches!(self.scheduler, ClusterNodeScheduler::Python { .. })
    }

    fn run_for(&mut self, delta_ns: u64) {
        match self.scheduler {
            ClusterNodeScheduler::RustRuntimeModel => {}
            ClusterNodeScheduler::External { run_for, .. } => {
                unsafe { run_for(delta_ns) };
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

    fn run_due_python_periodic(&mut self, cluster_elapsed_ns: u64) {
        let ClusterNodeScheduler::Python {
            scheduled,
            period_ns,
            next_due_ns,
            ..
        } = &mut self.scheduler
        else {
            return;
        };

        let due_to_period = *period_ns != 0 && *next_due_ns <= cluster_elapsed_ns;
        if !due_to_period {
            return;
        }

        if let Some(scheduled) = scheduled {
            let context = SchedulerCallbackContext {
                elapsed_ns: cluster_elapsed_ns,
                delta_ns: cluster_elapsed_ns.saturating_sub(self.elapsed_ns),
            };
            unsafe {
                scheduled(&context);
            };
        }
        self.elapsed_ns = cluster_elapsed_ns;
        while *next_due_ns <= cluster_elapsed_ns {
            *next_due_ns = next_due_ns.saturating_add(*period_ns);
        }
    }

    fn run_pending_python_input(&mut self, cluster_elapsed_ns: u64) {
        let ClusterNodeScheduler::Python {
            scheduled,
            input_pending,
            ..
        } = &mut self.scheduler
        else {
            return;
        };
        if !*input_pending {
            return;
        }

        if let Some(scheduled) = scheduled {
            let context = SchedulerCallbackContext {
                elapsed_ns: cluster_elapsed_ns,
                delta_ns: cluster_elapsed_ns.saturating_sub(self.elapsed_ns),
            };
            unsafe {
                scheduled(&context);
            };
        }
        self.elapsed_ns = cluster_elapsed_ns;
        *input_pending = false;
    }
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

struct TimerDataflowFanout {
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: ClusterTimerCountFn,
    source_recv_many: ClusterTimerRecvManyFn,
    sinks: Vec<ClusterTimerSink>,
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
    fn same_target(&self, other: Self) -> bool {
        match (*self, other) {
            (Self::SendMany(_), Self::SendMany(_)) => true,
            (
                Self::State { route_id },
                Self::State {
                    route_id: other_route_id,
                },
            ) => route_id == other_route_id,
            (
                Self::Native {
                    sink_id,
                    value_scale,
                    ..
                },
                Self::Native {
                    sink_id: other_sink_id,
                    value_scale: other_value_scale,
                    ..
                },
            ) => sink_id == other_sink_id && value_scale == other_value_scale,
            _ => false,
        }
    }

    fn send_many(&self, events: &[ScalarEvent]) -> u32 {
        match self {
            Self::SendMany(send_many) => unsafe {
                send_many(events.as_ptr(), events.len().min(u32::MAX as usize) as u32)
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

struct ScalarDataflowFanout {
    source_node: u32,
    route_id: u32,
    source_count: ClusterScalarCountFn,
    source_recv_many: ClusterScalarRecvManyFn,
    sinks: Vec<ClusterScalarSinkRoute>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub(super) struct DataflowChannel {
    pub(super) interface: i32,
    pub(super) port: i32,
    pub(super) channel: i32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(super) struct DataflowEdgeKey {
    node: u32,
    data_type: TypeId,
    channel: DataflowChannel,
}

#[derive(Clone, Copy, Debug)]
pub(super) struct DataflowEdge<T: 'static> {
    key: DataflowEdgeKey,
    _data: PhantomData<fn() -> T>,
}

impl<T: 'static> DataflowEdge<T> {
    pub(super) fn new(node: u32, channel: DataflowChannel) -> Self {
        Self {
            key: DataflowEdgeKey {
                node,
                data_type: TypeId::of::<T>(),
                channel,
            },
            _data: PhantomData,
        }
    }

    pub(super) fn key(self) -> DataflowEdgeKey {
        self.key
    }
}

fn scalar_edge(node: u32, route_id: u32) -> DataflowEdgeKey {
    DataflowEdge::<ScalarEvent>::new(
        node,
        DataflowChannel {
            interface: route_id as i32,
            ..Default::default()
        },
    )
    .key()
}

fn timer_edge(node: u32, interface: u16, port: i32, channel: i32) -> DataflowEdgeKey {
    DataflowEdge::<TimerChannelEvent>::new(
        node,
        DataflowChannel {
            interface: interface as i32,
            port,
            channel,
        },
    )
    .key()
}

fn can_edge(node: u32, bus: u8) -> DataflowEdgeKey {
    CanNetwork::edge(node, CanEndpoint::new(bus))
}

fn spi_edge(node: u32, device: i32) -> DataflowEdgeKey {
    SpiNetwork::edge(node, SpiEndpoint::from_device(device))
}

type DataflowRunFn = fn(&mut ClusterRuntime, usize) -> bool;
type DataflowPendingFn = fn(&ClusterRuntime, usize) -> bool;

#[derive(Clone)]
struct DataflowAlgorithm {
    owner_node: u32,
    sort_key: (u32, u32, usize),
    inputs: Vec<DataflowEdgeKey>,
    outputs: Vec<DataflowEdgeKey>,
    context: usize,
    pending: Option<DataflowPendingFn>,
    run: DataflowRunFn,
    period_ns: u64,
    next_due_ns: u64,
}

impl DataflowAlgorithm {
    fn source(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        outputs: Vec<DataflowEdgeKey>,
        context: usize,
        pending: DataflowPendingFn,
        run: DataflowRunFn,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs: Vec::new(),
            outputs,
            context,
            pending: Some(pending),
            run,
            period_ns: 0,
            next_due_ns: 0,
        }
    }

    fn periodic_source(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        outputs: Vec<DataflowEdgeKey>,
        context: usize,
        pending: DataflowPendingFn,
        run: DataflowRunFn,
        period_ns: u64,
        next_due_ns: u64,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs: Vec::new(),
            outputs,
            context,
            pending: Some(pending),
            run,
            period_ns,
            next_due_ns,
        }
    }

    fn transform(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        inputs: Vec<DataflowEdgeKey>,
        outputs: Vec<DataflowEdgeKey>,
        context: usize,
        run: DataflowRunFn,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs,
            outputs,
            context,
            pending: None,
            run,
            period_ns: 0,
            next_due_ns: 0,
        }
    }

    fn periodic_transform(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        inputs: Vec<DataflowEdgeKey>,
        outputs: Vec<DataflowEdgeKey>,
        context: usize,
        run: DataflowRunFn,
        period_ns: u64,
        next_due_ns: u64,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs,
            outputs,
            context,
            pending: None,
            run,
            period_ns,
            next_due_ns,
        }
    }
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
struct ClusterScalarRecord {
    source_node: u32,
    route_id: u32,
    event: ScalarEvent,
}

#[derive(Default)]
struct RuntimeDataflows {
    timer_fanout_indexes: HashMap<(u32, u16, i32, i32), usize>,
    timer_fanouts: Vec<TimerDataflowFanout>,
    scalar_fanout_indexes: HashMap<(u32, u32), usize>,
    scalar_fanouts: Vec<ScalarDataflowFanout>,
    scalar_state_route_count: usize,
    scalar_states: HashMap<(u32, u32), f32>,
    timer_records: VecDeque<ClusterTimerRecord>,
    scalar_records: VecDeque<ClusterScalarRecord>,
}

impl RuntimeDataflows {
    fn reset(&mut self) {
        self.timer_fanout_indexes.clear();
        self.timer_fanouts.clear();
        self.scalar_fanout_indexes.clear();
        self.scalar_fanouts.clear();
        self.scalar_state_route_count = 0;
        self.scalar_states.clear();
        self.timer_records.clear();
        self.scalar_records.clear();
    }
}

#[derive(Default)]
struct RuntimeComponents {
    periodic_can_sources: Vec<PeriodicCanSource>,
    battery_sources: Vec<BatterySourceModel>,
    battery_voltage_indexes: HashMap<(u32, u32), Vec<usize>>,
    timer_scaled_scalar_sources: Vec<TimerScaledScalarSource>,
    timer_scaled_scalar_timer_indexes: HashMap<(u32, u16, i32, i32), Vec<usize>>,
    dc_loads: Vec<DcLoadModel>,
    dc_load_current_indexes: HashMap<(u32, u32), Vec<usize>>,
}

impl RuntimeComponents {
    fn reset(&mut self) {
        self.periodic_can_sources.clear();
        self.battery_sources.clear();
        self.battery_voltage_indexes.clear();
        self.timer_scaled_scalar_sources.clear();
        self.timer_scaled_scalar_timer_indexes.clear();
        self.dc_loads.clear();
        self.dc_load_current_indexes.clear();
    }
}

#[derive(Default)]
struct DataflowGraph {
    dirty: bool,
    dataflow_algorithm_specs: Vec<DataflowAlgorithm>,
    dataflow_algorithms: Vec<DataflowAlgorithm>,
    dataflow_edge_dependents: HashMap<DataflowEdgeKey, Vec<usize>>,
    dataflow_algorithm_schedules_by_owner: Vec<Vec<usize>>,
    dataflow_polled_algorithms: Vec<usize>,
    dataflow_algorithm_queue: VecDeque<usize>,
    dataflow_algorithm_pending: Vec<bool>,
    dataflow_algorithm_available_inputs: Vec<HashSet<DataflowEdgeKey>>,
    dataflow_ready_edges: HashSet<DataflowEdgeKey>,
    configured_dataflow_algorithms: Vec<DataflowAlgorithm>,
}

impl DataflowGraph {
    fn reset(&mut self) {
        self.dirty = false;
        self.dataflow_algorithm_specs.clear();
        self.dataflow_algorithms.clear();
        self.dataflow_edge_dependents.clear();
        self.dataflow_algorithm_schedules_by_owner.clear();
        self.dataflow_polled_algorithms.clear();
        self.dataflow_algorithm_queue.clear();
        self.dataflow_algorithm_pending.clear();
        self.dataflow_algorithm_available_inputs.clear();
        self.dataflow_ready_edges.clear();
        self.configured_dataflow_algorithms.clear();
    }
}

#[derive(Default)]
struct ClusterRuntime {
    nodes: Vec<ClusterNode>,
    dataflows: RuntimeDataflows,
    networks: RuntimeNetworks,
    components: RuntimeComponents,
    graph: DataflowGraph,
    elapsed_ns: u64,
}

impl ClusterRuntime {
    fn reset(&mut self) {
        self.nodes.clear();
        self.dataflows.reset();
        self.networks.reset();
        self.components.reset();
        self.graph.reset();
        self.elapsed_ns = 0;
    }

    fn add_node(
        &mut self,
        run_for: ClusterNodeRunForFn,
        reset: ClusterNodeResetFn,
        online: bool,
    ) -> u32 {
        self.nodes.push(ClusterNode {
            scheduler: ClusterNodeScheduler::External { run_for },
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
        self.networks.can.upsert_fanout(route);
        self.graph.dirty = true;
        true
    }

    fn add_timer_route(&mut self, route: ClusterTimerRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.upsert_timer_fanout(route);
        self.graph.dirty = true;
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
        self.dataflows
            .timer_fanout_indexes
            .entry(key)
            .or_insert_with(|| {
                self.dataflows.timer_fanouts.push(TimerDataflowFanout {
                    source_node,
                    interface,
                    port,
                    channel,
                    source_count,
                    source_recv_many,
                    sinks: Vec::new(),
                });
                self.dataflows.timer_fanouts.len() - 1
            });
        self.graph.dirty = true;
        true
    }

    fn add_spi_route(&mut self, route: ClusterSpiRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.networks.spi.upsert_fanout(route);
        self.graph.dirty = true;
        true
    }

    fn add_scalar_route(&mut self, route: ClusterScalarRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        if matches!(route.sink, ClusterScalarSink::State { .. })
            && !self.scalar_route_exists(
                route.source_node,
                route.route_id,
                route.sink_node,
                route.sink,
            )
        {
            self.dataflows.scalar_state_route_count += 1;
        }
        self.upsert_scalar_fanout(route);
        self.graph.dirty = true;
        true
    }

    fn add_scalar_state_sink(&mut self, node: u32, route_id: u32, initial_value: f32) -> bool {
        if self.nodes.get(node as usize).is_none() || !initial_value.is_finite() {
            return false;
        }
        self.dataflows
            .scalar_states
            .insert((node, route_id), initial_value);
        self.graph
            .dataflow_ready_edges
            .insert(scalar_edge(node, route_id));
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
        if self.native_scalar_source_registered(source_node, route_id) {
            let events = self.native_scalar_events(source_node, route_id);
            if let Some(event) = events.last() {
                self.dataflows
                    .scalar_states
                    .insert((sink_node, sink_route_id), event.value);
                self.update_timer_scaled_scalar_scale(sink_node, sink_route_id, event.value);
                self.record_scalar_event(
                    sink_node,
                    sink_route_id,
                    ScalarEvent {
                        value: event.value,
                        timestamp_ns: self.elapsed_ns,
                    },
                );
                self.graph
                    .dataflow_ready_edges
                    .insert(scalar_edge(sink_node, sink_route_id));
            }
            for event in events {
                self.record_scalar_event(source_node, route_id, event);
            }
            self.graph.dirty = true;
            return true;
        }

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
            self.dataflows
                .scalar_states
                .insert((sink_node, sink_route_id), event.value);
            self.update_timer_scaled_scalar_scale(sink_node, sink_route_id, event.value);
            self.record_scalar_event(
                sink_node,
                sink_route_id,
                ScalarEvent {
                    value: event.value,
                    timestamp_ns: self.elapsed_ns,
                },
            );
            self.graph
                .dataflow_ready_edges
                .insert(scalar_edge(sink_node, sink_route_id));
        }
        for event in events {
            self.record_scalar_event(source_node, route_id, event);
        }
        self.graph.dirty = true;
        true
    }

    fn native_scalar_source_registered(&self, source_node: u32, route_id: u32) -> bool {
        self.components
            .dc_load_current_indexes
            .contains_key(&(source_node, route_id))
            || self
                .components
                .battery_voltage_indexes
                .contains_key(&(source_node, route_id))
    }

    fn scalar_route_exists(
        &self,
        source_node: u32,
        route_id: u32,
        sink_node: u32,
        sink: ClusterScalarSink,
    ) -> bool {
        let Some(group_index) = self
            .dataflows
            .scalar_fanout_indexes
            .get(&(source_node, route_id))
            .copied()
        else {
            return false;
        };
        self.dataflows.scalar_fanouts[group_index]
            .sinks
            .iter()
            .any(|existing| existing.sink_node == sink_node && existing.sink.same_target(sink))
    }

    fn upsert_timer_fanout(&mut self, route: ClusterTimerRoute) {
        let key = (
            route.source_node,
            route.interface,
            route.port,
            route.channel,
        );
        let group_index = *self
            .dataflows
            .timer_fanout_indexes
            .entry(key)
            .or_insert_with(|| {
                self.dataflows.timer_fanouts.push(TimerDataflowFanout {
                    source_node: route.source_node,
                    interface: route.interface,
                    port: route.port,
                    channel: route.channel,
                    source_count: route.source_count,
                    source_recv_many: route.source_recv_many,
                    sinks: Vec::new(),
                });
                self.dataflows.timer_fanouts.len() - 1
            });
        if self.dataflows.timer_fanouts[group_index]
            .sinks
            .iter()
            .any(|sink| sink.sink_node == route.sink_node)
        {
            return;
        }
        self.dataflows.timer_fanouts[group_index]
            .sinks
            .push(ClusterTimerSink {
                sink_node: route.sink_node,
                sink_send_many: route.sink_send_many,
            });
    }

    fn upsert_scalar_fanout(&mut self, route: ClusterScalarRoute) {
        let key = (route.source_node, route.route_id);
        let group_index = *self
            .dataflows
            .scalar_fanout_indexes
            .entry(key)
            .or_insert_with(|| {
                self.dataflows.scalar_fanouts.push(ScalarDataflowFanout {
                    source_node: route.source_node,
                    route_id: route.route_id,
                    source_count: route.source_count,
                    source_recv_many: route.source_recv_many,
                    sinks: Vec::new(),
                });
                self.dataflows.scalar_fanouts.len() - 1
            });
        if self.scalar_route_exists(
            route.source_node,
            route.route_id,
            route.sink_node,
            route.sink,
        ) {
            return;
        }
        self.dataflows.scalar_fanouts[group_index]
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
        self.components
            .periodic_can_sources
            .push(PeriodicCanSource::new(node, bus, period_ns, packet));
        self.graph.dirty = true;
        (self.components.periodic_can_sources.len() - 1) as u32
    }

    fn update_periodic_can_source(&mut self, handle: u32, packet: CanPacket) -> bool {
        let Some(source) = self
            .components
            .periodic_can_sources
            .get_mut(handle as usize)
        else {
            return false;
        };
        source.update_packet(packet);
        true
    }

    fn send_native_can_source_event(&mut self, node: u32, bus: u8, packet: CanPacket) -> bool {
        if self.nodes.get(node as usize).is_none() {
            return false;
        }
        self.networks
            .can
            .native_source_events
            .push_back(ClusterCanRecord {
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
        if self.components.dc_loads.iter().any(|load| {
            load.config_equals(
                node,
                current_route_id,
                resistance_ohms,
                inductance_henrys,
                capacitance_farads,
                scheduler_period_ns,
            )
        }) {
            return true;
        }
        self.components
            .dc_loads
            .retain(|load| !load.output_matches(node, current_route_id));
        self.components.dc_loads.push(DcLoadModel::new(
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
        self.graph.dirty = true;
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
        if self.components.battery_sources.iter().any(|source| {
            source.config_equals(
                node,
                voltage_route_id,
                voltage,
                internal_resistance_ohms,
                capacity_amp_hours,
            )
        }) {
            return true;
        }
        self.components
            .battery_sources
            .retain(|source| !source.config_matches(node, voltage_route_id));
        self.components
            .battery_sources
            .push(BatterySourceModel::new(
                node,
                voltage_route_id,
                voltage,
                internal_resistance_ohms,
                capacity_amp_hours,
            ));
        self.rebuild_battery_source_indexes();
        self.graph.dirty = true;
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
        let scale_value = if scale_route_id == 0 {
            1.0
        } else {
            *self
                .dataflows
                .scalar_states
                .get(&(node, scale_route_id))
                .unwrap_or(&0.0)
        };
        if self
            .components
            .timer_scaled_scalar_sources
            .iter()
            .any(|source| {
                source.config_equals(
                    node,
                    route_id,
                    timer_interface,
                    timer_port,
                    timer_channel,
                    scale_route_id,
                    scale,
                    offset,
                )
            })
        {
            return true;
        }
        self.components
            .timer_scaled_scalar_sources
            .retain(|source| !source.output_matches(node, route_id));
        self.components
            .timer_scaled_scalar_sources
            .push(TimerScaledScalarSource::new(
                node,
                route_id,
                timer_interface,
                timer_port,
                timer_channel,
                scale_route_id,
                scale_value,
                scale,
                offset,
            ));
        self.rebuild_timer_scaled_scalar_indexes();
        self.graph.dirty = true;
        true
    }

    fn update_timer_scaled_scalar_scale(
        &mut self,
        node: u32,
        scale_route_id: u32,
        scale_value: f32,
    ) {
        for source in self
            .components
            .timer_scaled_scalar_sources
            .iter_mut()
            .filter(|source| source.node() == node)
        {
            source.set_scale_value(scale_route_id, scale_value);
        }
    }

    fn add_dc_load_voltage_route(
        &mut self,
        source_node: u32,
        source_route_id: u32,
        sink_node: u32,
        sink_route_id: u32,
    ) -> bool {
        if self.nodes.get(source_node as usize).is_none()
            || self.nodes.get(sink_node as usize).is_none()
        {
            return false;
        }
        let mut matched = false;
        let mut changed = false;
        for load in self.components.dc_loads.iter_mut() {
            if load.configured_voltage_input_matches(sink_node, sink_route_id) {
                matched = true;
                if load.voltage_input_key() != (source_node, source_route_id) {
                    load.set_voltage_input(source_node, source_route_id);
                    changed = true;
                }
            }
        }
        if !matched {
            return false;
        }
        if !changed {
            return true;
        }
        self.rebuild_dc_load_indexes();
        self.graph.dirty = true;
        true
    }

    fn rebuild_timer_scaled_scalar_indexes(&mut self) {
        self.components.timer_scaled_scalar_timer_indexes.clear();
        for (index, source) in self
            .components
            .timer_scaled_scalar_sources
            .iter()
            .enumerate()
        {
            self.components
                .timer_scaled_scalar_timer_indexes
                .entry(source.timer_input_key())
                .or_default()
                .push(index);
        }
    }

    fn rebuild_dc_load_indexes(&mut self) {
        self.components.dc_load_current_indexes.clear();
        for (index, load) in self.components.dc_loads.iter().enumerate() {
            self.components
                .dc_load_current_indexes
                .entry(load.current_output_key())
                .or_default()
                .push(index);
        }
    }

    fn rebuild_battery_source_indexes(&mut self) {
        self.components.battery_voltage_indexes.clear();
        for (index, source) in self.components.battery_sources.iter().enumerate() {
            self.components
                .battery_voltage_indexes
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
            self.graph.dirty = true;
        }
        true
    }

    fn reset_rust_runtime_node_models(&mut self, node: u32, elapsed_ns: u64) {
        for source in self
            .components
            .timer_scaled_scalar_sources
            .iter_mut()
            .filter(|source| source.node() == node)
        {
            source.reset();
        }
        for source in self
            .components
            .battery_sources
            .iter_mut()
            .filter(|source| source.node() == node)
        {
            source.reset();
        }
        for load in self
            .components
            .dc_loads
            .iter_mut()
            .filter(|load| load.node() == node)
        {
            load.reset(elapsed_ns);
        }
    }

    fn node_online(&self, node: u32) -> bool {
        self.nodes
            .get(node as usize)
            .map(|node| node.online)
            .unwrap_or(false)
    }

    fn run_next_step(&mut self, remaining_ns: u64, max_step_ns: u64) -> u64 {
        if remaining_ns == 0 || max_step_ns == 0 {
            return 0;
        }

        self.ensure_dataflow_graph();

        let delta_ns = remaining_ns.min(max_step_ns);
        if delta_ns == 0 {
            return 0;
        }

        for node in self
            .nodes
            .iter_mut()
            .filter(|node| node.online && node.needs_run_step())
        {
            node.run_for(delta_ns);
        }

        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
        self.run_due_python_periodic_nodes();
        self.enqueue_pending_dataflow_algorithms();
        self.run_due_dataflow_algorithms();
        self.run_dataflow_algorithm_queue();
        self.run_pending_python_input_nodes();
        delta_ns
    }

    fn record_can_event(&mut self, source_node: u32, bus: u8, event: CanEvent) {
        self.networks.can.record(source_node, bus, event);
    }

    fn record_timer_event(
        &mut self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
        event: TimerChannelEvent,
    ) {
        self.dataflows.timer_records.push_back(ClusterTimerRecord {
            source_node,
            interface,
            port,
            channel,
            event,
        });
    }

    fn record_scalar_event(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        self.dataflows
            .scalar_records
            .push_back(ClusterScalarRecord {
                source_node,
                route_id,
                event,
            });
    }

    fn ensure_dataflow_graph(&mut self) {
        if !self.graph.dirty {
            return;
        }
        self.rebuild_dataflow_graph();
        self.graph.dirty = false;
    }

    fn compile_dataflow_graph(&mut self) -> bool {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.rebuild_dataflow_graph();
            self.graph.dirty = false;
        }))
        .is_ok()
    }

    fn add_dataflow_algorithm(&mut self, algorithm: DataflowAlgorithm) -> bool {
        if algorithm.owner_node != u32::MAX
            && self.nodes.get(algorithm.owner_node as usize).is_none()
        {
            return false;
        }
        self.graph.configured_dataflow_algorithms.push(algorithm);
        self.graph.dirty = true;
        true
    }

    fn rebuild_dataflow_algorithm_specs(&mut self) {
        self.graph.dataflow_algorithm_specs.clear();
        self.graph
            .dataflow_algorithm_specs
            .push(DataflowAlgorithm::source(
                u32::MAX,
                (u32::MAX, 0, 0),
                Vec::new(),
                0,
                dataflow_native_can_source_pending,
                dataflow_run_native_can_sources,
            ));
        for (index, source) in self.components.periodic_can_sources.iter().enumerate() {
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::periodic_source(
                    source.node(),
                    (source.node(), 0, index),
                    vec![can_edge(source.node(), source.bus())],
                    index,
                    dataflow_periodic_can_source_pending,
                    dataflow_run_periodic_can_source,
                    source.period_ns(),
                    source.due_at_ns(),
                ));
        }
        for (index, group) in self.networks.can.fanouts.iter().enumerate() {
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::source(
                    group.source_node,
                    (group.source_node, 1, index),
                    vec![can_edge(group.source_node, group.endpoint.bus())],
                    index,
                    dataflow_can_fanout_pending,
                    dataflow_run_can_fanout,
                ));
        }
        for (index, group) in self.dataflows.timer_fanouts.iter().enumerate() {
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::source(
                    group.source_node,
                    (group.source_node, 2, index),
                    vec![timer_edge(
                        group.source_node,
                        group.interface,
                        group.port,
                        group.channel,
                    )],
                    index,
                    dataflow_timer_fanout_pending,
                    dataflow_run_timer_fanout,
                ));
        }
        for (index, group) in self.networks.spi.fanouts.iter().enumerate() {
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::source(
                    group.source_node,
                    (group.source_node, 3, index),
                    vec![spi_edge(group.source_node, group.endpoint.device())],
                    index,
                    dataflow_spi_fanout_pending,
                    dataflow_run_spi_fanout,
                ));
        }
        for (index, group) in self.dataflows.scalar_fanouts.iter().enumerate() {
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::source(
                    group.source_node,
                    (group.source_node, 4, index),
                    vec![scalar_edge(group.source_node, group.route_id)],
                    index,
                    dataflow_scalar_fanout_pending,
                    dataflow_run_scalar_fanout,
                ));
        }
        for (index, source) in self.components.battery_sources.iter().enumerate() {
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::source(
                    source.node(),
                    (source.node(), 5, index),
                    vec![scalar_edge(source.node(), source.voltage_output_key().1)],
                    index,
                    dataflow_battery_source_pending,
                    dataflow_run_battery_source,
                ));
        }
        for (index, source) in self
            .components
            .timer_scaled_scalar_sources
            .iter()
            .enumerate()
        {
            let (node, interface, port, channel) = source.timer_input_key();
            let mut inputs = vec![timer_edge(node, interface, port, channel)];
            if source.scale_route_id != 0 {
                inputs.push(scalar_edge(source.node(), source.scale_route_id));
            }
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::transform(
                    source.node(),
                    (source.node(), 6, index),
                    inputs,
                    vec![scalar_edge(source.node(), source.route_id)],
                    index,
                    dataflow_run_timer_scaled_scalar,
                ));
        }
        for (index, load) in self.components.dc_loads.iter().enumerate() {
            let period_ns = load.scheduler_period_ns();
            self.graph
                .dataflow_algorithm_specs
                .push(DataflowAlgorithm::periodic_transform(
                    load.node(),
                    (load.node(), 7, index),
                    vec![scalar_edge(
                        load.voltage_input_key().0,
                        load.voltage_input_key().1,
                    )],
                    vec![scalar_edge(
                        load.current_output_key().0,
                        load.current_output_key().1,
                    )],
                    index,
                    dataflow_run_dc_load,
                    period_ns,
                    self.elapsed_ns.saturating_add(period_ns),
                ));
        }
        self.graph
            .dataflow_algorithm_specs
            .extend(self.graph.configured_dataflow_algorithms.iter().cloned());
    }

    fn rebuild_dataflow_graph(&mut self) {
        self.rebuild_dataflow_algorithm_specs();
        self.graph.dataflow_algorithms =
            self.ordered_dataflow_algorithms(&self.graph.dataflow_algorithm_specs);
        self.graph.dataflow_edge_dependents.clear();
        self.graph.dataflow_algorithm_schedules_by_owner = vec![Vec::new(); self.nodes.len()];
        self.graph.dataflow_polled_algorithms.clear();
        self.graph.dataflow_algorithm_queue.clear();
        self.graph.dataflow_algorithm_pending = vec![false; self.graph.dataflow_algorithms.len()];
        self.graph.dataflow_algorithm_available_inputs =
            vec![HashSet::new(); self.graph.dataflow_algorithms.len()];

        for index in 0..self.graph.dataflow_algorithms.len() {
            if self.graph.dataflow_algorithms[index].pending.is_some()
                && (self.graph.dataflow_algorithms[index].inputs.is_empty()
                    || self.graph.dataflow_algorithms[index].outputs.is_empty())
            {
                self.graph.dataflow_polled_algorithms.push(index);
            }
            if self.graph.dataflow_algorithms[index].period_ns != 0 {
                let owner_node = self.graph.dataflow_algorithms[index].owner_node as usize;
                if let Some(schedules) = self
                    .graph
                    .dataflow_algorithm_schedules_by_owner
                    .get_mut(owner_node)
                {
                    schedules.push(index);
                }
            }
            let inputs = self.graph.dataflow_algorithms[index].inputs.clone();
            for input in inputs {
                if self.dataflow_edge_available(input) {
                    if let Some(available) = self
                        .graph
                        .dataflow_algorithm_available_inputs
                        .get_mut(index)
                    {
                        available.insert(input);
                    }
                }
                self.graph
                    .dataflow_edge_dependents
                    .entry(input)
                    .or_default()
                    .push(index);
            }
        }
        for indexes in self.graph.dataflow_edge_dependents.values_mut() {
            indexes.sort_by_key(|index| self.graph.dataflow_algorithms[*index].sort_key);
            indexes.dedup();
        }
        for index in 0..self.graph.dataflow_algorithms.len() {
            if self.dataflow_algorithm_pending_state(index) {
                self.enqueue_dataflow_algorithm_if_ready(index);
            }
        }
    }

    fn dataflow_algorithm_inputs_ready(&self, index: usize) -> bool {
        let Some(algorithm) = self.graph.dataflow_algorithms.get(index) else {
            return false;
        };
        if algorithm.inputs.is_empty() {
            return true;
        }
        let Some(available) = self.graph.dataflow_algorithm_available_inputs.get(index) else {
            return false;
        };
        algorithm
            .inputs
            .iter()
            .all(|input| available.contains(input))
    }

    fn dataflow_edge_available(&self, edge: DataflowEdgeKey) -> bool {
        self.graph.dataflow_ready_edges.contains(&edge)
    }

    fn dataflow_algorithm_pending_state(&self, index: usize) -> bool {
        let Some(algorithm) = self.graph.dataflow_algorithms.get(index) else {
            return false;
        };
        algorithm
            .pending
            .map(|pending| pending(self, algorithm.context))
            .unwrap_or(false)
    }

    fn enqueue_pending_dataflow_algorithms(&mut self) {
        for position in 0..self.graph.dataflow_polled_algorithms.len() {
            let index = self.graph.dataflow_polled_algorithms[position];
            if self.dataflow_algorithm_pending_state(index) {
                self.enqueue_dataflow_algorithm_if_ready(index);
            }
        }
    }

    fn ordered_dataflow_algorithms(
        &self,
        algorithms: &[DataflowAlgorithm],
    ) -> Vec<DataflowAlgorithm> {
        let mut ordered_candidates = algorithms.to_vec();
        ordered_candidates.sort_by_key(|algorithm| algorithm.sort_key);

        let mut output_indexes: HashMap<DataflowEdgeKey, Vec<usize>> = HashMap::new();
        for (index, algorithm) in ordered_candidates.iter().enumerate() {
            for output in &algorithm.outputs {
                output_indexes.entry(*output).or_default().push(index);
            }
        }
        let mut dependencies: Vec<HashSet<usize>> = vec![HashSet::new(); ordered_candidates.len()];
        let mut dependents: Vec<HashSet<usize>> = vec![HashSet::new(); ordered_candidates.len()];

        for (dependent_index, algorithm) in ordered_candidates.iter().enumerate() {
            for input in &algorithm.inputs {
                let Some(source_indexes) = output_indexes.get(input) else {
                    continue;
                };
                for source_index in source_indexes.iter().copied() {
                    dependencies[dependent_index].insert(source_index);
                    dependents[source_index].insert(dependent_index);
                }
            }
        }
        let mut ready: Vec<usize> = dependencies
            .iter()
            .enumerate()
            .filter_map(|(index, deps)| if deps.is_empty() { Some(index) } else { None })
            .collect();
        ready.sort_by_key(|index| ordered_candidates[*index].sort_key);

        let mut ordered = Vec::with_capacity(ordered_candidates.len());
        let mut emitted = HashSet::new();
        while let Some(index) = ready.first().copied() {
            ready.remove(0);
            if !emitted.insert(index) {
                continue;
            }
            ordered.push(ordered_candidates[index].clone());

            let mut next_dependents: Vec<usize> = dependents[index].iter().copied().collect();
            next_dependents.sort_by_key(|index| ordered_candidates[*index].sort_key);
            for dependent in next_dependents {
                dependencies[dependent].remove(&index);
                if dependencies[dependent].is_empty()
                    && !emitted.contains(&dependent)
                    && !ready.contains(&dependent)
                {
                    ready.push(dependent);
                    ready.sort_by_key(|index| ordered_candidates[*index].sort_key);
                }
            }
        }

        if ordered.len() != ordered_candidates.len() {
            panic!("native dataflow graph contains a cycle");
        }
        ordered
    }

    fn enqueue_dataflow_algorithm(&mut self, index: usize) {
        if index >= self.graph.dataflow_algorithms.len() {
            return;
        }
        if !self
            .graph
            .dataflow_algorithm_pending
            .get(index)
            .copied()
            .unwrap_or(false)
        {
            if let Some(pending) = self.graph.dataflow_algorithm_pending.get_mut(index) {
                *pending = true;
            }
            self.graph.dataflow_algorithm_queue.push_back(index);
        }
    }

    fn enqueue_dataflow_algorithm_if_ready(&mut self, index: usize) {
        if self.dataflow_algorithm_inputs_ready(index) {
            self.enqueue_dataflow_algorithm(index);
        }
    }

    fn run_due_dataflow_algorithms(&mut self) {
        for node_index in 0..self.graph.dataflow_algorithm_schedules_by_owner.len() {
            if !self.node_online(node_index as u32) {
                continue;
            }
            for position in 0..self.graph.dataflow_algorithm_schedules_by_owner[node_index].len() {
                let index = self.graph.dataflow_algorithm_schedules_by_owner[node_index][position];
                let Some(algorithm) = self.graph.dataflow_algorithms.get(index) else {
                    continue;
                };
                if algorithm.period_ns == 0 || algorithm.next_due_ns > self.elapsed_ns {
                    continue;
                }
                let period_ns = algorithm.period_ns;
                self.enqueue_dataflow_algorithm_if_ready(index);
                if let Some(algorithm) = self.graph.dataflow_algorithms.get_mut(index) {
                    while algorithm.next_due_ns <= self.elapsed_ns {
                        algorithm.next_due_ns = algorithm.next_due_ns.saturating_add(period_ns);
                    }
                }
            }
        }
    }

    fn run_dataflow_algorithm_queue(&mut self) {
        while let Some(index) = self.graph.dataflow_algorithm_queue.pop_front() {
            let Some(pending) = self.graph.dataflow_algorithm_pending.get_mut(index) else {
                continue;
            };
            if !*pending {
                continue;
            }
            *pending = false;
            let Some(algorithm) = self.graph.dataflow_algorithms.get(index) else {
                continue;
            };
            let owner_node = algorithm.owner_node;
            let context = algorithm.context;
            let run = algorithm.run;
            let outputs = algorithm.outputs.clone();
            if owner_node != u32::MAX && !self.node_online(owner_node) {
                continue;
            }
            if run(self, context) {
                for output in outputs {
                    self.mark_dataflow_edge_ready(output);
                }
            }
        }
    }

    fn mark_dataflow_edge_ready(&mut self, key: DataflowEdgeKey) {
        self.ensure_dataflow_graph();
        let Some(indexes) = self.graph.dataflow_edge_dependents.get(&key).cloned() else {
            return;
        };
        for index in indexes {
            if let Some(available) = self
                .graph
                .dataflow_algorithm_available_inputs
                .get_mut(index)
            {
                available.insert(key);
            }
            self.enqueue_dataflow_algorithm_if_ready(index);
        }
    }

    fn run_due_python_periodic_nodes(&mut self) {
        let elapsed_ns = self.elapsed_ns;
        for node in self
            .nodes
            .iter_mut()
            .filter(|node| node.online && node.is_python_scheduled())
        {
            node.run_due_python_periodic(elapsed_ns);
        }
    }

    fn run_pending_python_input_nodes(&mut self) {
        let elapsed_ns = self.elapsed_ns;
        for node in self
            .nodes
            .iter_mut()
            .filter(|node| node.online && node.is_python_scheduled())
        {
            node.run_pending_python_input(elapsed_ns);
        }
    }

    fn mark_input_pending(&mut self, node: u32) {
        let Some(node) = self.nodes.get_mut(node as usize) else {
            return;
        };
        node.mark_input_pending();
    }

    fn run_periodic_can_source(&mut self, source_index: usize) -> bool {
        let Some(source) = self.components.periodic_can_sources.get(source_index) else {
            return false;
        };
        let source_node = source.node();
        let source_bus = source.bus();
        if !self.node_online(source_node) {
            return false;
        }
        let Some(event) =
            self.components.periodic_can_sources[source_index].emit_if_due(self.elapsed_ns)
        else {
            return false;
        };

        let mut input_pending_nodes = Vec::new();
        if let Some(group_index) = self
            .networks
            .can
            .fanout_indexes
            .get(&(source_node, CanEndpoint::new(source_bus)))
            .copied()
        {
            let record_index = self.networks.can.fanouts[group_index].record_index;
            for sink in &self.networks.can.fanouts[group_index].sinks {
                let accepted = unsafe { (sink.sink_send_many)(sink.sink_bus, &event.packet, 1) };
                if accepted > 0 {
                    input_pending_nodes.push(sink.sink_node);
                }
            }
            self.networks.can.record_at(record_index, event);
        } else {
            self.record_can_event(source_node, source_bus, event);
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
        true
    }

    fn run_native_can_sources(&mut self) -> bool {
        let mut routed = false;
        let mut input_pending_nodes = Vec::new();
        while let Some(record) = self.networks.can.native_source_events.pop_front() {
            if !self.node_online(record.source_node) {
                continue;
            }
            routed = true;
            if let Some(group_index) = self
                .networks
                .can
                .fanout_indexes
                .get(&(record.source_node, CanEndpoint::new(record.bus)))
                .copied()
            {
                let record_index = self.networks.can.fanouts[group_index].record_index;
                for sink in &self.networks.can.fanouts[group_index].sinks {
                    let accepted =
                        unsafe { (sink.sink_send_many)(sink.sink_bus, &record.event.packet, 1) };
                    if accepted > 0 {
                        input_pending_nodes.push(sink.sink_node);
                    }
                }
                self.networks.can.record_at(record_index, record.event);
            } else {
                self.record_can_event(record.source_node, record.bus, record.event);
            }
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
        routed
    }

    fn run_can_fanout(&mut self, group_index: usize) -> bool {
        let Some(group) = self.networks.can.fanouts.get(group_index) else {
            return false;
        };
        let source_node = group.source_node;
        let source_bus = group.endpoint.bus();
        let record_index = group.record_index;
        let source_tx_count = group.source_tx_count;
        let source_recv_events = group.source_recv_events;
        let sink_count = group.sinks.len();

        if !self.node_online(source_node) {
            return false;
        }

        let pending = unsafe { source_tx_count(source_bus) };
        if pending == 0 {
            return false;
        }

        let mut events = vec![CanEvent::default(); pending as usize];
        let count = unsafe { source_recv_events(source_bus, events.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return false;
        }
        events.truncate(count);

        let packets: Vec<CanPacket> = events.iter().map(|event| event.packet).collect();
        let mut input_pending_nodes = Vec::new();
        if !packets.is_empty() {
            for sink_index in 0..sink_count {
                let sink = self.networks.can.fanouts[group_index].sinks[sink_index];
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

        for event in events {
            self.networks.can.record_at(record_index, event);
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
        true
    }

    fn run_timer_fanout(&mut self, group_index: usize) -> bool {
        let Some(group) = self.dataflows.timer_fanouts.get(group_index) else {
            return false;
        };
        let source_node = group.source_node;
        let interface = group.interface;
        let port = group.port;
        let channel = group.channel;
        let source_count = group.source_count;
        let source_recv_many = group.source_recv_many;

        if !self.node_online(source_node) {
            return false;
        }

        let pending = unsafe { source_count(port, channel) };
        if pending == 0 {
            return false;
        }

        let mut events = vec![TimerChannelEvent::default(); pending as usize];
        let count = unsafe { source_recv_many(port, channel, events.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return false;
        }
        events.truncate(count);
        self.update_timer_scaled_scalar_source(source_node, interface, port, channel, &events);

        let mut input_pending_nodes = Vec::new();
        let sink_count = self.dataflows.timer_fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.dataflows.timer_fanouts[group_index].sinks[sink_index];
            let accepted = unsafe {
                (sink.sink_send_many)(events.as_ptr(), events.len().min(u32::MAX as usize) as u32)
            };
            if accepted > 0 {
                input_pending_nodes.push(sink.sink_node);
            }
        }

        for event in events {
            self.record_timer_event(source_node, interface, port, channel, event);
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
        true
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
            .components
            .timer_scaled_scalar_timer_indexes
            .get(&key)
            .map(|indexes| indexes.len())
            .unwrap_or(0);
        for index_position in 0..index_count {
            let Some(index) = self
                .components
                .timer_scaled_scalar_timer_indexes
                .get(&key)
                .and_then(|indexes| indexes.get(index_position))
                .copied()
            else {
                continue;
            };
            if let Some(source) = self.components.timer_scaled_scalar_sources.get_mut(index) {
                source.update_timer(events);
            }
        }
    }

    fn route_native_scalar_event(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        self.record_scalar_event(source_node, route_id, event);
        self.update_scalar_state_sinks(source_node, route_id, event);
        self.update_scalar_sinks(source_node, route_id, event);
        self.mark_dataflow_edge_ready(scalar_edge(source_node, route_id));
    }

    fn update_scalar_state_sinks(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        let Some(group_index) = self
            .dataflows
            .scalar_fanout_indexes
            .get(&(source_node, route_id))
            .copied()
        else {
            return;
        };
        let sink_count = self.dataflows.scalar_fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.dataflows.scalar_fanouts[group_index].sinks[sink_index];
            let ClusterScalarSink::State { route_id } = sink.sink else {
                continue;
            };
            self.dataflows
                .scalar_states
                .insert((sink.sink_node, route_id), event.value);
            self.update_timer_scaled_scalar_scale(sink.sink_node, route_id, event.value);
            self.mark_dataflow_edge_ready(scalar_edge(sink.sink_node, route_id));
            self.mark_input_pending(sink.sink_node);
        }
    }

    fn update_scalar_sinks(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        let Some(group_index) = self
            .dataflows
            .scalar_fanout_indexes
            .get(&(source_node, route_id))
            .copied()
        else {
            return;
        };
        let sink_count = self.dataflows.scalar_fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.dataflows.scalar_fanouts[group_index].sinks[sink_index];
            if matches!(sink.sink, ClusterScalarSink::State { .. }) {
                continue;
            }
            let accepted = sink.sink.send_many(&[event]);
            if accepted > 0 {
                self.mark_input_pending(sink.sink_node);
            }
        }
    }

    fn run_spi_fanout(&mut self, group_index: usize) -> bool {
        let Some(group) = self.networks.spi.fanouts.get(group_index) else {
            return false;
        };
        let source_node = group.source_node;
        let device = group.endpoint.device();
        let source_count = group.source_count;
        let source_recv_many = group.source_recv_many;

        if !self.node_online(source_node) {
            return false;
        }

        let pending = unsafe { (source_count)(device) };
        if pending == 0 {
            return false;
        }

        let mut transactions = vec![SpiTransaction::default(); pending as usize];
        let count = unsafe { (source_recv_many)(device, transactions.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return false;
        }
        transactions.truncate(count);

        let mut input_pending_nodes = Vec::new();
        let sink_count = self.networks.spi.fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.networks.spi.fanouts[group_index].sinks[sink_index];
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

        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
        true
    }

    fn run_scalar_fanout(&mut self, group_index: usize) -> bool {
        let Some(group) = self.dataflows.scalar_fanouts.get(group_index) else {
            return false;
        };
        let source_node = group.source_node;
        let route_id = group.route_id;
        let source_count = group.source_count;
        let source_recv_many = group.source_recv_many;

        if !self.node_online(source_node)
            || self.native_scalar_source_registered(source_node, route_id)
        {
            return false;
        }
        let pending = unsafe { source_count() };
        if pending == 0 {
            return false;
        }

        let mut events = vec![ScalarEvent::default(); pending as usize];
        let count = unsafe { source_recv_many(events.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return false;
        }
        events.truncate(count);

        let mut input_pending_nodes = Vec::new();
        let sink_count = self.dataflows.scalar_fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.dataflows.scalar_fanouts[group_index].sinks[sink_index];
            let accepted = match sink.sink {
                ClusterScalarSink::State { route_id } => {
                    if let Some(event) = events.last() {
                        self.dataflows
                            .scalar_states
                            .insert((sink.sink_node, route_id), event.value);
                        self.update_timer_scaled_scalar_scale(
                            sink.sink_node,
                            route_id,
                            event.value,
                        );
                        self.mark_dataflow_edge_ready(scalar_edge(sink.sink_node, route_id));
                    }
                    events.len().min(u32::MAX as usize) as u32
                }
                _ => sink.sink.send_many(&events),
            };
            if accepted > 0 {
                input_pending_nodes.push(sink.sink_node);
            }
        }

        for event in events {
            self.record_scalar_event(source_node, route_id, event);
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }
        true
    }

    fn native_scalar_pending(&self, source_node: u32, route_id: u32) -> bool {
        if let Some(indexes) = self
            .components
            .dc_load_current_indexes
            .get(&(source_node, route_id))
        {
            if indexes.iter().copied().any(|index| {
                self.components
                    .dc_loads
                    .get(index)
                    .map(|load| load.has_pending_current())
                    .unwrap_or(false)
            }) {
                return true;
            }
        }
        if let Some(indexes) = self
            .components
            .battery_voltage_indexes
            .get(&(source_node, route_id))
        {
            if indexes.iter().copied().any(|index| {
                self.components
                    .battery_sources
                    .get(index)
                    .map(|source| source.has_pending_voltage())
                    .unwrap_or(false)
            }) {
                return true;
            }
        }
        false
    }

    fn native_scalar_events(&mut self, source_node: u32, route_id: u32) -> Vec<ScalarEvent> {
        let mut events = Vec::new();
        if let Some(indexes) = self
            .components
            .dc_load_current_indexes
            .get(&(source_node, route_id))
        {
            for index in indexes.iter().copied() {
                if let Some(load) = self.components.dc_loads.get_mut(index) {
                    if let Some(event) = load.take_current_event(self.elapsed_ns) {
                        events.push(event);
                    }
                }
            }
        }
        if let Some(indexes) = self
            .components
            .battery_voltage_indexes
            .get(&(source_node, route_id))
        {
            for index in indexes.iter().copied() {
                if let Some(source) = self.components.battery_sources.get_mut(index) {
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
        self.networks
            .can
            .latest_message(source_node, bus, message_id)
    }

    fn latest_can_bus_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        self.networks.can.latest_bus_event(source_node, bus)
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
        self.dataflows
            .timer_records
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

    fn latest_scalar_event(&self, source_node: u32, route_id: u32) -> Option<ScalarEvent> {
        self.dataflows
            .scalar_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.route_id == route_id)
            .map(|record| record.event)
    }
}

fn dataflow_run_battery_source(runtime: &mut ClusterRuntime, context: usize) -> bool {
    let Some(source) = runtime.components.battery_sources.get_mut(context) else {
        return false;
    };
    let source_node = source.node();
    let route_id = source.voltage_output_key().1;
    let event = source.take_voltage_event(runtime.elapsed_ns);
    if let Some(event) = event {
        runtime.route_native_scalar_event(source_node, route_id, event);
        return true;
    }
    false
}

fn dataflow_run_periodic_can_source(runtime: &mut ClusterRuntime, context: usize) -> bool {
    runtime.run_periodic_can_source(context)
}

fn dataflow_periodic_can_source_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    let Some(source) = runtime.components.periodic_can_sources.get(context) else {
        return false;
    };
    runtime.node_online(source.node()) && source.has_pending_event(runtime.elapsed_ns)
}

fn dataflow_run_native_can_sources(runtime: &mut ClusterRuntime, _context: usize) -> bool {
    runtime.run_native_can_sources()
}

fn dataflow_native_can_source_pending(runtime: &ClusterRuntime, _context: usize) -> bool {
    runtime
        .networks
        .can
        .native_source_events
        .iter()
        .any(|record| runtime.node_online(record.source_node))
}

fn dataflow_run_can_fanout(runtime: &mut ClusterRuntime, context: usize) -> bool {
    runtime.run_can_fanout(context)
}

fn dataflow_can_fanout_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    let Some(group) = runtime.networks.can.fanouts.get(context) else {
        return false;
    };
    runtime.node_online(group.source_node)
        && unsafe { (group.source_tx_count)(group.endpoint.bus()) } != 0
}

fn dataflow_run_timer_fanout(runtime: &mut ClusterRuntime, context: usize) -> bool {
    runtime.run_timer_fanout(context)
}

fn dataflow_timer_fanout_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    let Some(group) = runtime.dataflows.timer_fanouts.get(context) else {
        return false;
    };
    runtime.node_online(group.source_node)
        && unsafe { (group.source_count)(group.port, group.channel) } != 0
}

fn dataflow_run_spi_fanout(runtime: &mut ClusterRuntime, context: usize) -> bool {
    runtime.run_spi_fanout(context)
}

fn dataflow_spi_fanout_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    let Some(group) = runtime.networks.spi.fanouts.get(context) else {
        return false;
    };
    runtime.node_online(group.source_node)
        && unsafe { (group.source_count)(group.endpoint.device()) } != 0
}

fn dataflow_run_scalar_fanout(runtime: &mut ClusterRuntime, context: usize) -> bool {
    runtime.run_scalar_fanout(context)
}

fn dataflow_scalar_fanout_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    let Some(group) = runtime.dataflows.scalar_fanouts.get(context) else {
        return false;
    };
    runtime.node_online(group.source_node)
        && !runtime.native_scalar_source_registered(group.source_node, group.route_id)
        && unsafe { (group.source_count)() } != 0
}

fn dataflow_battery_source_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    runtime
        .components
        .battery_sources
        .get(context)
        .map(|source| source.has_pending_voltage())
        .unwrap_or(false)
}

fn dataflow_run_timer_scaled_scalar(runtime: &mut ClusterRuntime, context: usize) -> bool {
    let Some(source) = runtime
        .components
        .timer_scaled_scalar_sources
        .get_mut(context)
    else {
        return false;
    };
    let source_node = source.node();
    let route_id = source.route_id;
    let event = source.take_scalar_event(runtime.elapsed_ns);
    if let Some(event) = event {
        runtime.route_native_scalar_event(source_node, route_id, event);
        return true;
    }
    false
}

fn dataflow_run_dc_load(runtime: &mut ClusterRuntime, context: usize) -> bool {
    let Some(load) = runtime.components.dc_loads.get(context) else {
        return false;
    };
    let source_node = load.node();
    let route_id = load.current_output_key().1;
    let voltage_input_key = load.voltage_input_key();
    let voltage_event = runtime.latest_scalar_event(voltage_input_key.0, voltage_input_key.1);
    let Some(load) = runtime.components.dc_loads.get_mut(context) else {
        return false;
    };
    if let Some(event) = voltage_event {
        load.update_voltage_event(event);
    }
    load.run_until(runtime.elapsed_ns);
    let event = load.take_current_event(runtime.elapsed_ns);
    if let Some(event) = event {
        runtime.route_native_scalar_event(source_node, route_id, event);
        return true;
    }
    false
}

fn dataflow_noop(_runtime: &mut ClusterRuntime, _context: usize) -> bool {
    false
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
pub extern "C" fn rig_cluster_add_scalar_transform_algorithm(
    owner_node: u32,
    sort_index: u32,
    input_route_id: u32,
    output_route_id: u32,
) -> bool {
    let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
    runtime.add_dataflow_algorithm(DataflowAlgorithm::transform(
        owner_node,
        (owner_node, 100, sort_index as usize),
        vec![scalar_edge(owner_node, input_route_id)],
        vec![scalar_edge(owner_node, output_route_id)],
        sort_index as usize,
        dataflow_noop,
    ))
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_compile_dataflow_graph() -> bool {
    CLUSTER_RUNTIME.lock().unwrap().compile_dataflow_graph()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_node(run_for: usize, reset: usize, online: bool) -> u32 {
    let Some(run_for) = (unsafe { function_pointer::<ClusterNodeRunForFn>(run_for) }) else {
        return u32::MAX;
    };
    let Some(reset) = (unsafe { function_pointer::<ClusterNodeResetFn>(reset) }) else {
        return u32::MAX;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_node(run_for, reset, online)
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

    CLUSTER_RUNTIME.lock().unwrap().add_timer_source(
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
    let Some(set_value) = (unsafe { function_pointer::<ClusterScalarSinkSetFn>(set_value) }) else {
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

    CLUSTER_RUNTIME.lock().unwrap().add_scalar_state_route(
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
    source_route_id: u32,
    sink_node: u32,
    sink_route_id: u32,
) -> bool {
    CLUSTER_RUNTIME.lock().unwrap().add_dc_load_voltage_route(
        source_node,
        source_route_id,
        sink_node,
        sink_route_id,
    )
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
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_timer_scaled_scalar_source(
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
pub extern "C" fn rig_cluster_run_for(duration_ns: u64, max_step_ns: u64, route: usize) {
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
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns);
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
        let (delta_ns, elapsed_ns, matched) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns);
            let elapsed_ns = runtime.elapsed_ns;
            let matched = route.is_none()
                && runtime.latest_can_signal_eq(
                    source_node,
                    bus,
                    message_id,
                    signal_name,
                    expected,
                    tolerance,
                );
            (delta_ns, elapsed_ns, matched)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }
        if matched {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }

        if let Some(route) = route {
            if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);

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
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_index_eq(
    timeout_ns: u64,
    max_step_ns: u64,
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
        let (delta_ns, elapsed_ns, matched) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns);
            let elapsed_ns = runtime.elapsed_ns;
            let matched = route.is_none()
                && runtime.latest_can_signal_eq(
                    source_node,
                    bus,
                    message_id,
                    signal_name,
                    expected,
                    tolerance,
                );
            (delta_ns, elapsed_ns, matched)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }
        if matched {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }

        if let Some(route) = route {
            if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);

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
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_index_cmp(
    timeout_ns: u64,
    max_step_ns: u64,
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
        let (delta_ns, elapsed_ns, matched) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns);
            let elapsed_ns = runtime.elapsed_ns;
            let matched = route.is_none()
                && runtime.latest_can_signal_cmp(
                    source_node,
                    bus,
                    message_id,
                    signal_name,
                    expected,
                    tolerance,
                    comparison,
                );
            (delta_ns, elapsed_ns, matched)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }
        if matched {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }

        if let Some(route) = route {
            if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);

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
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_can_signal_comparisons(
    timeout_ns: u64,
    max_step_ns: u64,
    route: usize,
    source_node: u32,
    comparisons: *const CanSignalComparison,
    comparison_count: u32,
) -> u64 {
    if comparisons.is_null() {
        return u64::MAX;
    }
    let comparisons = unsafe { std::slice::from_raw_parts(comparisons, comparison_count as usize) };
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
        let (delta_ns, elapsed_ns, matched) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns);
            let elapsed_ns = runtime.elapsed_ns;
            let matched =
                route.is_none() && runtime.can_signal_comparisons_match(source_node, comparisons);
            (delta_ns, elapsed_ns, matched)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }
        if matched {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }

        if let Some(route) = route {
            if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);

                if CLUSTER_RUNTIME
                    .lock()
                    .unwrap()
                    .can_signal_comparisons_match(source_node, comparisons)
                {
                    return elapsed_ns.saturating_sub(current_elapsed_ns);
                }
            }
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
