use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::cluster::{
    ClusterRuntime, ClusterTimerCountFn, ClusterTimerRecvManyFn, ClusterTimerSendManyFn,
};
use super::scalar::ScalarEvent;
use super::dataflow::{
    DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowChannel,
    DataflowEvent,
};
use super::datapath::{DataPath, DataPathEvent};
use super::interfaces::{InterfaceCaller, InterfaceDataflow, InterfaceEndpoint, InterfaceImplementation};
use super::registry::RuntimeInterfaces;
use super::scalar;
use super::scheduler;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TimerChannel {
    pub port: i32,
    pub channel: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TimerChannelEvent {
    pub port: i32,
    pub channel: i32,
    pub value: f32,
    pub timestamp_ns: u64,
}

impl DataflowEvent for TimerChannelEvent {}

#[derive(Clone, Copy)]
pub(super) struct ClusterTimerRoute {
    pub(super) source_node: u32,
    pub(super) interface: u16,
    pub(super) port: i32,
    pub(super) channel: i32,
    pub(super) source_count: ClusterTimerCountFn,
    pub(super) source_recv_many: ClusterTimerRecvManyFn,
    pub(super) sink_node: u32,
    pub(super) sink_send_many: ClusterTimerSendManyFn,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterTimerSink {
    pub(super) sink_node: u32,
    pub(super) sink_send_many: ClusterTimerSendManyFn,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterTimerRecord {
    pub(super) source_node: u32,
    pub(super) interface: u16,
    pub(super) port: i32,
    pub(super) channel: i32,
    pub(super) event: TimerChannelEvent,
}

pub(super) struct TimerInterfaceFanout {
    pub(super) source_node: u32,
    pub(super) interface: u16,
    pub(super) port: i32,
    pub(super) channel: i32,
    pub(super) source_count: ClusterTimerCountFn,
    pub(super) source_recv_many: ClusterTimerRecvManyFn,
    pub(super) sinks: Vec<ClusterTimerSink>,
}

pub(super) struct TimerRouteResult {
    pub(super) source_node: u32,
    pub(super) interface: u16,
    pub(super) port: i32,
    pub(super) channel: i32,
    pub(super) events: Vec<TimerChannelEvent>,
    pub(super) input_pending_nodes: Vec<u32>,
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

    fn timer_input_key(&self) -> (u32, u16, i32, i32) {
        (
            self.node,
            self.timer_interface,
            self.timer_port,
            self.timer_channel,
        )
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
        self.node == node
            && self.route_id == route_id
            && self.timer_interface == timer_interface
            && self.timer_port == timer_port
            && self.timer_channel == timer_channel
            && self.scale_route_id == scale_route_id
            && self.scale == scale
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

/// Timer event runtime interface: per timer port/channel event fanout and latest samples.
#[derive(Default)]
pub(super) struct TimerInterface {
    pub(super) fanout_indexes: std::collections::HashMap<(u32, u16, i32, i32), usize>,
    pub(super) fanouts: Vec<TimerInterfaceFanout>,
    records: std::collections::VecDeque<ClusterTimerRecord>,
    scaled_scalar_sources: Vec<TimerScaledScalarSource>,
    scaled_scalar_timer_indexes: std::collections::HashMap<(u32, u16, i32, i32), Vec<usize>>,
}

impl InterfaceImplementation for TimerInterface {
    fn reset_interface(&mut self) {
        self.fanout_indexes.clear();
        self.fanouts.clear();
        self.records.clear();
        self.scaled_scalar_sources.clear();
        self.scaled_scalar_timer_indexes.clear();
    }
}

impl InterfaceCaller for TimerInterface {
    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        for (index, group) in self.fanouts.iter().enumerate() {
            specs.push(DataflowAlgorithm::source(
                group.source_node,
                (group.source_node, 2, index),
                vec![<Self as InterfaceDataflow<TimerChannelEvent>>::edge(
                    group.source_node,
                    TimerEndpoint::new(group.interface, group.port, group.channel),
                )],
                Arc::new(TimerFanoutAlgorithm { group_index: index }),
            ));
        }
        for (index, source) in self.scaled_scalar_sources.iter().enumerate() {
            let (node, interface, port, channel) = source.timer_input_key();
            let mut inputs = vec![RuntimeInterfaces::timer_edge(
                node, interface, port, channel,
            )];
            if source.scale_route_id != 0 {
                inputs.push(RuntimeInterfaces::scalar_edge(
                    source.node,
                    source.scale_route_id,
                ));
            }
            specs.push(DataflowAlgorithm::transform(
                source.node,
                (source.node, 6, index),
                inputs,
                vec![RuntimeInterfaces::scalar_edge(source.node, source.route_id)],
                Arc::new(TimerScaledScalarAlgorithm {
                    source_index: index,
                }),
            ));
        }
}
}

impl InterfaceDataflow<TimerChannelEvent> for TimerInterface {
    type Endpoint = TimerEndpoint;
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub(super) struct TimerEndpoint {
    interface: u16,
    port: i32,
    channel: i32,
}

impl TimerEndpoint {
    pub(super) fn new(interface: u16, port: i32, channel: i32) -> Self {
        Self {
            interface,
            port,
            channel,
        }
    }
}

impl InterfaceEndpoint for TimerEndpoint {
    fn dataflow_channel(self) -> DataflowChannel {
        DataflowChannel {
            interface: self.interface as i32,
            port: self.port,
            channel: self.channel,
        }
    }
}

impl TimerInterface {
    pub(super) fn reset_node_models(&mut self, node: u32) {
        for source in self
            .scaled_scalar_sources
            .iter_mut()
            .filter(|source| source.node == node)
        {
            source.reset();
        }
    }

    pub(super) fn add_scaled_scalar_source(
        &mut self,
        node: u32,
        route_id: u32,
        timer_interface: u16,
        timer_port: i32,
        timer_channel: i32,
        scale_route_id: u32,
        scale_value: f32,
        scale: f32,
        offset: f32,
    ) -> bool {
        if self.scaled_scalar_sources.iter().any(|source| {
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
        }) {
            return false;
        }
        self.scaled_scalar_sources
            .retain(|source| !source.output_matches(node, route_id));
        self.scaled_scalar_sources
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
        self.rebuild_scaled_scalar_indexes();
        true
    }

    pub(super) fn update_scaled_scalar_source(
        &mut self,
        sink_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
        events: &[TimerChannelEvent],
    ) {
        let key = (sink_node, interface, port, channel);
        let indexes = self
            .scaled_scalar_timer_indexes
            .get(&key)
            .cloned()
            .unwrap_or_default();
        for index in indexes {
            if let Some(source) = self.scaled_scalar_sources.get_mut(index) {
                source.update_timer(events);
            }
        }
    }

    pub(super) fn update_scaled_scalar_scale(
        &mut self,
        node: u32,
        scale_route_id: u32,
        scale_value: f32,
    ) {
        for source in self
            .scaled_scalar_sources
            .iter_mut()
            .filter(|source| source.node == node)
        {
            source.set_scale_value(scale_route_id, scale_value);
        }
    }

    pub(super) fn take_scaled_scalar_event(
        &mut self,
        index: usize,
        elapsed_ns: u64,
    ) -> Option<(u32, u32, ScalarEvent)> {
        let source = self.scaled_scalar_sources.get_mut(index)?;
        Some((source.node, source.route_id, source.take_scalar_event(elapsed_ns)?))
    }

    fn rebuild_scaled_scalar_indexes(&mut self) {
        self.scaled_scalar_timer_indexes.clear();
        for (index, source) in self.scaled_scalar_sources.iter().enumerate() {
            self.scaled_scalar_timer_indexes
                .entry(source.timer_input_key())
                .or_default()
                .push(index);
        }
    }

    pub(super) fn upsert_fanout(&mut self, route: ClusterTimerRoute) {
        let key = (
            route.source_node,
            route.interface,
            route.port,
            route.channel,
        );
        let group_index = *self.fanout_indexes.entry(key).or_insert_with(|| {
            self.fanouts.push(TimerInterfaceFanout {
                source_node: route.source_node,
                interface: route.interface,
                port: route.port,
                channel: route.channel,
                source_count: route.source_count,
                source_recv_many: route.source_recv_many,
                sinks: Vec::new(),
            });
            self.fanouts.len() - 1
        });
        if route.sink_node == u32::MAX {
            return;
        }
        if self.fanouts[group_index]
            .sinks
            .iter()
            .any(|sink| sink.sink_node == route.sink_node)
        {
            return;
        }
        self.fanouts[group_index].sinks.push(ClusterTimerSink {
            sink_node: route.sink_node,
            sink_send_many: route.sink_send_many,
        });
    }

    pub(super) fn record(
        &mut self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
        event: TimerChannelEvent,
    ) {
        self.records.push_back(ClusterTimerRecord {
            source_node,
            interface,
            port,
            channel,
            event,
        });
    }

    pub(super) fn latest(
        &self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
    ) -> Option<TimerChannelEvent> {
        self.records
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

    pub(super) fn fanout_pending(
        &self,
        group_index: usize,
        mut source_online: impl FnMut(u32) -> bool,
    ) -> bool {
        let Some(group) = self.fanouts.get(group_index) else {
            return false;
        };
        source_online(group.source_node)
            && unsafe { (group.source_count)(group.port, group.channel) } != 0
    }

    pub(super) fn route_fanout(
        &mut self,
        group_index: usize,
        mut source_online: impl FnMut(u32) -> bool,
    ) -> Option<TimerRouteResult> {
        let group = self.fanouts.get(group_index)?;
        let source_node = group.source_node;
        let interface = group.interface;
        let port = group.port;
        let channel = group.channel;
        let source_count = group.source_count;
        let source_recv_many = group.source_recv_many;

        if !source_online(source_node) {
            return None;
        }

        let pending = unsafe { source_count(port, channel) };
        if pending == 0 {
            return None;
        }

        let mut events = vec![TimerChannelEvent::default(); pending as usize];
        let count = unsafe { source_recv_many(port, channel, events.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return None;
        }
        events.truncate(count);

        let mut input_pending_nodes = Vec::new();
        let sink_count = self.fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.fanouts[group_index].sinks[sink_index];
            let accepted = unsafe {
                (sink.sink_send_many)(events.as_ptr(), events.len().min(u32::MAX as usize) as u32)
            };
            if accepted > 0 {
                input_pending_nodes.push(sink.sink_node);
            }
        }

        for event in events.iter().copied() {
            self.record(source_node, interface, port, channel, event);
        }
        Some(TimerRouteResult {
            source_node,
            interface,
            port,
            channel,
            events,
            input_pending_nodes,
        })
    }
}

struct TimerFanoutAlgorithm {
    group_index: usize,
}

impl DataflowAlgorithmExecutor for TimerFanoutAlgorithm {
    fn polls_pending(&self) -> bool {
        true
    }

    fn pending(&self, runtime: &ClusterRuntime) -> bool {
        runtime
            .interfaces
            .timer_fanout_pending(self.group_index, |source_node| {
                runtime.node_online(source_node)
            })
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        run_timer_fanout(runtime, self.group_index)
    }
}

fn run_timer_fanout(runtime: &mut ClusterRuntime, group_index: usize) -> bool {
    let online_nodes = runtime.online_nodes();
    let Some(result) = runtime
        .interfaces
        .timer_route_fanout(group_index, |node| online_node(&online_nodes, node))
    else {
        return false;
    };
    runtime.interfaces.update_scaled_scalar_source(&result);
    for sink_node in result.input_pending_nodes {
        scheduler::mark_input_pending(runtime, sink_node);
    }
    true
}

struct TimerScaledScalarAlgorithm {
    source_index: usize,
}

impl DataflowAlgorithmExecutor for TimerScaledScalarAlgorithm {
    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        run_timer_scaled_scalar(runtime, self.source_index)
    }
}

fn run_timer_scaled_scalar(runtime: &mut ClusterRuntime, source_index: usize) -> bool {
    let Some((source_node, route_id, event)) = runtime
        .interfaces
        .take_scaled_scalar_event(source_index, runtime.elapsed_ns)
    else {
        return false;
    };
    scalar::route_native_event(runtime, source_node, route_id, event);
    true
}

pub(super) fn add_scaled_scalar_source(
    runtime: &mut ClusterRuntime,
    node: u32,
    route_id: u32,
    timer_interface: u16,
    timer_port: i32,
    timer_channel: i32,
    scale_route_id: u32,
    scale: f32,
    offset: f32,
) -> bool {
    if !runtime.node_exists(node) || !scale.is_finite() || !offset.is_finite() {
        return false;
    }
    let scale_value = if scale_route_id == 0 {
        1.0
    } else {
        runtime.interfaces.scalar_state(node, scale_route_id)
    };
    runtime.interfaces.add_scaled_scalar_source(
        node,
        route_id,
        timer_interface,
        timer_port,
        timer_channel,
        scale_route_id,
        scale_value,
        scale,
        offset,
    );
    runtime.scheduler.mark_dirty();
    true
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
    super::cluster::with_runtime(|runtime| {
        add_scaled_scalar_source(
            runtime,
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
}

fn online_node(online_nodes: &[bool], node: u32) -> bool {
    online_nodes.get(node as usize).copied().unwrap_or(false)
}

impl TimerChannelEvent {
    fn timer_channel(&self) -> TimerChannel {
        TimerChannel {
            port: self.port,
            channel: self.channel,
        }
    }
}

impl DataPathEvent for TimerChannelEvent {
    type Channel = TimerChannel;

    fn channel(&self) -> Self::Channel {
        self.timer_channel()
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TimerCaptureChannel {
    pub channel: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TimerCaptureEvent {
    pub channel: i32,
    pub value: f32,
    pub timestamp_ns: u64,
}

impl DataflowEvent for TimerCaptureEvent {}

impl TimerCaptureEvent {
    fn timer_channel(&self) -> TimerCaptureChannel {
        TimerCaptureChannel {
            channel: self.channel,
        }
    }
}

impl DataPathEvent for TimerCaptureEvent {
    type Channel = TimerCaptureChannel;

    fn channel(&self) -> Self::Channel {
        self.timer_channel()
    }
}

#[derive(Debug)]
struct TimerPeripheral {
    channel: TimerChannel,
    duty_inputs: DataPath<TimerChannelEvent>,
    duty_outputs: DataPath<TimerChannelEvent>,
    frequency_inputs: DataPath<TimerChannelEvent>,
    frequency_outputs: DataPath<TimerChannelEvent>,
}

impl TimerPeripheral {
    fn new(channel: TimerChannel) -> Self {
        Self {
            channel,
            duty_inputs: DataPath::new(channel),
            duty_outputs: DataPath::new(channel),
            frequency_inputs: DataPath::new(channel),
            frequency_outputs: DataPath::new(channel),
        }
    }

    fn reset(&mut self) {
        self.duty_inputs.clear();
        self.duty_outputs.clear();
        self.frequency_inputs.clear();
        self.frequency_outputs.clear();
    }
}

#[derive(Debug)]
struct TimerCapturePeripheral {
    capture_inputs: DataPath<TimerCaptureEvent>,
}

impl TimerCapturePeripheral {
    fn new(channel: TimerCaptureChannel) -> Self {
        Self {
            capture_inputs: DataPath::new(channel),
        }
    }

    fn reset(&mut self) {
        self.capture_inputs.clear();
    }
}

#[derive(Debug, Default)]
struct TimerModel {
    peripherals: Vec<TimerPeripheral>,
    capture_peripherals: Vec<TimerCapturePeripheral>,
}

impl TimerModel {
    fn reset(&mut self) {
        for peripheral in &mut self.peripherals {
            peripheral.reset();
        }
        for peripheral in &mut self.capture_peripherals {
            peripheral.reset();
        }
    }

    fn peripheral(&mut self, channel: TimerChannel) -> &mut TimerPeripheral {
        if let Some(index) = self
            .peripherals
            .iter()
            .position(|peripheral| peripheral.channel == channel)
        {
            return &mut self.peripherals[index];
        }

        self.peripherals.push(TimerPeripheral::new(channel));
        self.peripherals.last_mut().unwrap()
    }

    fn capture_peripheral(&mut self, channel: TimerCaptureChannel) -> &mut TimerCapturePeripheral {
        if let Some(index) = self
            .capture_peripherals
            .iter()
            .position(|peripheral| peripheral.capture_inputs.channel() == channel)
        {
            return &mut self.capture_peripherals[index];
        }

        self.capture_peripherals
            .push(TimerCapturePeripheral::new(channel));
        self.capture_peripherals.last_mut().unwrap()
    }
}

static TIMER_MODEL: LazyLock<Mutex<TimerModel>> =
    LazyLock::new(|| Mutex::new(TimerModel::default()));

pub fn reset() {
    TIMER_MODEL.lock().unwrap().reset();
}

pub fn configure_channel(port: i32, channel: i32) {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel });
}

pub fn push_duty_input(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .duty_inputs
        .push(event)
}

pub fn push_duty_inputs(events: &[TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let mut count = 0;
    for event in events {
        if timer
            .peripheral(event.timer_channel())
            .duty_inputs
            .push(*event)
        {
            count += 1;
        }
    }
    count
}

pub fn latest_duty_input(port: i32, channel: i32) -> Option<f32> {
    let mut timer = TIMER_MODEL.lock().unwrap();
    timer
        .peripheral(TimerChannel { port, channel })
        .duty_inputs
        .latest()
        .map(|event| event.value)
}

pub fn push_frequency_input(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .frequency_inputs
        .push(event)
}

pub fn push_frequency_inputs(events: &[TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let mut count = 0;
    for event in events {
        if timer
            .peripheral(event.timer_channel())
            .frequency_inputs
            .push(*event)
        {
            count += 1;
        }
    }
    count
}

pub fn latest_frequency_input(port: i32, channel: i32) -> Option<f32> {
    let mut timer = TIMER_MODEL.lock().unwrap();
    timer
        .peripheral(TimerChannel { port, channel })
        .frequency_inputs
        .latest()
        .map(|event| event.value)
}

pub fn push_capture_input(event: TimerCaptureEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .capture_peripheral(event.timer_channel())
        .capture_inputs
        .push(event)
}

pub fn latest_capture_input(channel: i32) -> Option<f32> {
    let mut timer = TIMER_MODEL.lock().unwrap();
    timer
        .capture_peripheral(TimerCaptureChannel { channel })
        .capture_inputs
        .latest()
        .map(|event| event.value)
}

pub fn push_duty_output(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .duty_outputs
        .push(event)
}

pub fn pop_duty_output(port: i32, channel: i32) -> Option<TimerChannelEvent> {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .duty_outputs
        .pop()
}

pub fn pop_duty_outputs(port: i32, channel: i32, out: &mut [TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let output = &mut timer
        .peripheral(TimerChannel { port, channel })
        .duty_outputs;
    let mut count = 0;
    for slot in out.iter_mut() {
        let Some(event) = output.pop() else {
            break;
        };
        *slot = event;
        count += 1;
    }
    count
}

pub fn duty_output_count(port: i32, channel: i32) -> u32 {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .duty_outputs
        .count()
}

pub fn push_frequency_output(event: TimerChannelEvent) -> bool {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(event.timer_channel())
        .frequency_outputs
        .push(event)
}

pub fn pop_frequency_output(port: i32, channel: i32) -> Option<TimerChannelEvent> {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .frequency_outputs
        .pop()
}

pub fn pop_frequency_outputs(port: i32, channel: i32, out: &mut [TimerChannelEvent]) -> u32 {
    let mut timer = TIMER_MODEL.lock().unwrap();
    let output = &mut timer
        .peripheral(TimerChannel { port, channel })
        .frequency_outputs;
    let mut count = 0;
    for slot in out.iter_mut() {
        let Some(event) = output.pop() else {
            break;
        };
        *slot = event;
        count += 1;
    }
    count
}

pub fn frequency_output_count(port: i32, channel: i32) -> u32 {
    TIMER_MODEL
        .lock()
        .unwrap()
        .peripheral(TimerChannel { port, channel })
        .frequency_outputs
        .count()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_duty_input(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_duty_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_duty_inputs(
    events: *const TimerChannelEvent,
    count: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts(events, count as usize) };
    push_duty_inputs(events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_latest_duty_input(
    port: i32,
    channel: i32,
    value: *mut f32,
) -> bool {
    if value.is_null() {
        return false;
    }
    match latest_duty_input(port, channel) {
        Some(next) => {
            unsafe { *value = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_frequency_input(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_frequency_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_frequency_inputs(
    events: *const TimerChannelEvent,
    count: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts(events, count as usize) };
    push_frequency_inputs(events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_latest_frequency_input(
    port: i32,
    channel: i32,
    value: *mut f32,
) -> bool {
    if value.is_null() {
        return false;
    }
    match latest_frequency_input(port, channel) {
        Some(next) => {
            unsafe { *value = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_capture_input(event: *const TimerCaptureEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_capture_input(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_latest_capture_input(channel: i32, value: *mut f32) -> bool {
    if value.is_null() {
        return false;
    }
    match latest_capture_input(channel) {
        Some(next) => {
            unsafe { *value = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_duty_output(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_duty_output(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_duty_output(
    port: i32,
    channel: i32,
    event: *mut TimerChannelEvent,
) -> bool {
    if event.is_null() {
        return false;
    }
    match pop_duty_output(port, channel) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_duty_outputs(
    port: i32,
    channel: i32,
    events: *mut TimerChannelEvent,
    capacity: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts_mut(events, capacity as usize) };
    pop_duty_outputs(port, channel, events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_duty_output_count(port: i32, channel: i32) -> u32 {
    duty_output_count(port, channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_push_frequency_output(event: *const TimerChannelEvent) -> bool {
    if event.is_null() {
        return false;
    }
    push_frequency_output(unsafe { *event })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_frequency_output(
    port: i32,
    channel: i32,
    event: *mut TimerChannelEvent,
) -> bool {
    if event.is_null() {
        return false;
    }
    match pop_frequency_output(port, channel) {
        Some(next) => {
            unsafe { *event = next };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_pop_frequency_outputs(
    port: i32,
    channel: i32,
    events: *mut TimerChannelEvent,
    capacity: u32,
) -> u32 {
    if events.is_null() {
        return 0;
    }
    let events = unsafe { std::slice::from_raw_parts_mut(events, capacity as usize) };
    pop_frequency_outputs(port, channel, events)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_timer_frequency_output_count(port: i32, channel: i32) -> u32 {
    frequency_output_count(port, channel)
}
