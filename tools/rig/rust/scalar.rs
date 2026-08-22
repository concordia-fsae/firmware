use std::collections::{HashMap, VecDeque};
use std::sync::Arc;

pub use super::dataflow::ScalarEvent;
use super::dataflow::{
    DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowChannel, DataflowRuntime,
    NativeScalarReceiveFn,
};
use super::interfaces::{
    InterfaceCaller, InterfaceDataflow, InterfaceEndpoint, InterfaceImplementation,
};

pub type ScalarCountFn = unsafe extern "C" fn() -> u32;
pub type ScalarRecvManyFn = unsafe extern "C" fn(*mut ScalarEvent, u32) -> u32;
pub type ScalarSendManyFn = unsafe extern "C" fn(*const ScalarEvent, u32) -> u32;
pub type ScalarSinkSetFn = unsafe extern "C" fn(i32, f32);

#[derive(Clone, Copy)]
pub enum ScalarSink {
    SendMany(ScalarSendManyFn),
    State {
        route_id: u32,
        sink_id: i32,
        value_scale: f32,
        set_value: Option<ScalarSinkSetFn>,
    },
    Native {
        sink_id: i32,
        value_scale: f32,
        set_value: ScalarSinkSetFn,
    },
    Algorithm {
        context: usize,
        route_id: u32,
        receive: NativeScalarReceiveFn,
    },
}

impl ScalarSink {
    pub(super) fn same_target(&self, other: Self) -> bool {
        match (*self, other) {
            (Self::SendMany(_), Self::SendMany(_)) => true,
            (
                Self::State {
                    route_id,
                    sink_id,
                    value_scale,
                    ..
                },
                Self::State {
                    route_id: other_route_id,
                    sink_id: other_sink_id,
                    value_scale: other_value_scale,
                    ..
                },
            ) => {
                route_id == other_route_id
                    && sink_id == other_sink_id
                    && value_scale == other_value_scale
            }
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
            (
                Self::Algorithm {
                    context,
                    route_id,
                    receive,
                },
                Self::Algorithm {
                    context: other_context,
                    route_id: other_route_id,
                    receive: other_receive,
                },
            ) => {
                context == other_context
                    && route_id == other_route_id
                    && receive as usize == other_receive as usize
            }
            _ => false,
        }
    }

    pub(super) fn send_many(self, events: &[ScalarEvent]) -> u32 {
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
                let Some(event) = events.last() else {
                    return 0;
                };
                unsafe { set_value(sink_id, event.value * value_scale) };
                events.len().min(u32::MAX as usize) as u32
            }
            Self::Algorithm {
                context, receive, ..
            } => events
                .iter()
                .filter(|event| receive(context, **event))
                .count()
                .min(u32::MAX as usize) as u32,
        }
    }
}

#[derive(Clone, Copy)]
pub struct ScalarRoute {
    pub source_node: u32,
    pub route_id: u32,
    pub source_count: ScalarCountFn,
    pub source_recv_many: ScalarRecvManyFn,
    pub sink_node: u32,
    pub sink: ScalarSink,
}

#[derive(Clone, Copy)]
pub(super) struct ScalarSinkRoute {
    pub(super) sink_node: u32,
    pub(super) sink: ScalarSink,
}

#[derive(Clone, Copy)]
pub(super) struct ScalarRecord {
    pub(super) source_node: u32,
    pub(super) route_id: u32,
    pub(super) event: ScalarEvent,
}

pub(super) struct ScalarInterfaceFanout {
    pub(super) source_node: u32,
    pub(super) route_id: u32,
    pub(super) source_count: ScalarCountFn,
    pub(super) source_recv_many: ScalarRecvManyFn,
    pub(super) sinks: Vec<ScalarSinkRoute>,
}

pub struct ScalarRouteResult {
    pub ready_states: Vec<(u32, u32, f32)>,
    pub ready_edges: Vec<(u32, u32)>,
    pub input_pending_nodes: Vec<u32>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct ScalarEndpoint {
    route_id: u32,
}

impl ScalarEndpoint {
    pub const fn new(route_id: u32) -> Self {
        Self { route_id }
    }

    pub const fn route_id(self) -> u32 {
        self.route_id
    }

    pub fn edge(self, node: u32) -> super::dataflow::DataflowEdgeKey {
        <ScalarInterface as InterfaceDataflow<ScalarEvent>>::edge(node, self)
    }
}

pub fn edge(node: u32, route_id: u32) -> super::dataflow::DataflowEdgeKey {
    ScalarEndpoint::new(route_id).edge(node)
}

impl InterfaceEndpoint for ScalarEndpoint {
    fn dataflow_channel(self) -> DataflowChannel {
        DataflowChannel {
            interface: self.route_id as i32,
            ..Default::default()
        }
    }
}

/// Scalar value runtime interface: scalar fanout, state inputs, records, and latest samples.
#[derive(Default)]
pub struct ScalarInterface {
    pub(super) fanout_indexes: HashMap<(u32, u32), usize>,
    pub(super) fanouts: Vec<ScalarInterfaceFanout>,
    pub(super) state_route_count: usize,
    pub(super) states: HashMap<(u32, u32), f32>,
    pub(super) state_inputs: HashMap<u32, Vec<u32>>,
    pub(super) records: VecDeque<ScalarRecord>,
}

impl InterfaceImplementation for ScalarInterface {
    fn reset_interface(&mut self) {
        self.fanout_indexes.clear();
        self.fanouts.clear();
        self.state_route_count = 0;
        self.states.clear();
        self.state_inputs.clear();
        self.records.clear();
    }
}

impl InterfaceCaller for ScalarInterface {
    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        for (index, group) in self.fanouts.iter().enumerate() {
            specs.push(DataflowAlgorithm::source(
                group.source_node,
                (group.source_node, 4, index),
                vec![<Self as InterfaceDataflow<ScalarEvent>>::edge(
                    group.source_node,
                    ScalarEndpoint::new(group.route_id),
                )],
                Arc::new(ScalarFanoutAlgorithm { group_index: index }),
            ));
        }
    }
}

impl InterfaceDataflow<ScalarEvent> for ScalarInterface {
    type Endpoint = ScalarEndpoint;
}

impl ScalarInterface {
    pub fn set_state(&mut self, node: u32, route_id: u32, value: f32) {
        self.states.insert((node, route_id), value);
    }

    pub fn register_route(&mut self, route: ScalarRoute) {
        if matches!(route.sink, ScalarSink::State { .. })
            && !self.route_exists(
                route.source_node,
                route.route_id,
                route.sink_node,
                route.sink,
            )
        {
            self.state_route_count += 1;
        }
        self.upsert_fanout(route);
    }

    pub fn add_state_input(&mut self, node: u32, route_id: u32) {
        let inputs = self.state_inputs.entry(node).or_default();
        if !inputs.contains(&route_id) {
            inputs.push(route_id);
            inputs.sort_unstable();
        }
    }

    pub fn state_input_values(&self, node: u32) -> Vec<(u32, f32)> {
        self.state_inputs
            .get(&node)
            .into_iter()
            .flat_map(|inputs| inputs.iter().copied())
            .filter_map(|route_id| {
                self.states
                    .get(&(node, route_id))
                    .copied()
                    .map(|value| (route_id, value))
            })
            .collect()
    }

    pub fn state_value(&self, node: u32, route_id: u32) -> f32 {
        self.states.get(&(node, route_id)).copied().unwrap_or(0.0)
    }

    pub fn record(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        self.records.push_back(ScalarRecord {
            source_node,
            route_id,
            event,
        });
    }

    pub fn latest(&self, source_node: u32, route_id: u32) -> Option<ScalarEvent> {
        self.records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.route_id == route_id)
            .map(|record| record.event)
    }

    pub fn route_exists(
        &self,
        source_node: u32,
        route_id: u32,
        sink_node: u32,
        sink: ScalarSink,
    ) -> bool {
        let Some(group_index) = self.fanout_indexes.get(&(source_node, route_id)).copied() else {
            return false;
        };
        self.fanouts[group_index]
            .sinks
            .iter()
            .any(|existing| existing.sink_node == sink_node && existing.sink.same_target(sink))
    }

    pub fn upsert_fanout(&mut self, route: ScalarRoute) {
        let key = (route.source_node, route.route_id);
        let group_index = *self.fanout_indexes.entry(key).or_insert_with(|| {
            self.fanouts.push(ScalarInterfaceFanout {
                source_node: route.source_node,
                route_id: route.route_id,
                source_count: route.source_count,
                source_recv_many: route.source_recv_many,
                sinks: Vec::new(),
            });
            self.fanouts.len() - 1
        });
        if self.fanouts[group_index].sinks.iter().any(|existing| {
            existing.sink_node == route.sink_node && existing.sink.same_target(route.sink)
        }) {
            return;
        }
        self.fanouts[group_index].sinks.push(ScalarSinkRoute {
            sink_node: route.sink_node,
            sink: route.sink,
        });
    }

    pub fn route_event(
        &mut self,
        source_node: u32,
        route_id: u32,
        event: ScalarEvent,
    ) -> ScalarRouteResult {
        self.record(source_node, route_id, event);
        let mut result = ScalarRouteResult {
            ready_states: Vec::new(),
            ready_edges: Vec::new(),
            input_pending_nodes: Vec::new(),
        };
        let Some(group_index) = self.fanout_indexes.get(&(source_node, route_id)).copied() else {
            return result;
        };

        let sink_count = self.fanouts[group_index].sinks.len();
        for sink_index in 0..sink_count {
            let sink = self.fanouts[group_index].sinks[sink_index];
            let accepted = match sink.sink {
                ScalarSink::State {
                    route_id: sink_route_id,
                    set_value,
                    sink_id,
                    value_scale,
                } => {
                    self.states
                        .insert((sink.sink_node, sink_route_id), event.value);
                    if sink_id >= 0 {
                        if let Some(set_value) = set_value {
                            unsafe { set_value(sink_id, event.value * value_scale) };
                        }
                    }
                    result
                        .ready_states
                        .push((sink.sink_node, sink_route_id, event.value));
                    1
                }
                ScalarSink::Algorithm { route_id, .. } => {
                    result.ready_edges.push((sink.sink_node, route_id));
                    sink.sink.send_many(&[event])
                }
                _ => sink.sink.send_many(&[event]),
            };
            if accepted > 0 {
                result.input_pending_nodes.push(sink.sink_node);
            }
        }
        result
    }

    pub(super) fn fanout_pending(
        &self,
        group_index: usize,
        mut source_online: impl FnMut(u32) -> bool,
        mut skip_native_source: impl FnMut(u32, u32) -> bool,
    ) -> bool {
        let Some(group) = self.fanouts.get(group_index) else {
            return false;
        };
        source_online(group.source_node)
            && !skip_native_source(group.source_node, group.route_id)
            && unsafe { (group.source_count)() } != 0
    }

    pub(super) fn route_fanout(
        &mut self,
        group_index: usize,
        mut source_online: impl FnMut(u32) -> bool,
        mut skip_native_source: impl FnMut(u32, u32) -> bool,
    ) -> Option<ScalarRouteResult> {
        let group = self.fanouts.get(group_index)?;
        let source_node = group.source_node;
        let route_id = group.route_id;
        let source_count = group.source_count;
        let source_recv_many = group.source_recv_many;

        if !source_online(source_node) || skip_native_source(source_node, route_id) {
            return None;
        }
        let pending = unsafe { source_count() };
        if pending == 0 {
            return None;
        }

        let mut events = vec![ScalarEvent::default(); pending as usize];
        let count = unsafe { source_recv_many(events.as_mut_ptr(), pending) };
        let count = count.min(pending) as usize;
        if count == 0 {
            return None;
        }
        events.truncate(count);

        let mut merged = ScalarRouteResult {
            ready_states: Vec::new(),
            ready_edges: Vec::new(),
            input_pending_nodes: Vec::new(),
        };
        for event in events {
            let result = self.route_event(source_node, route_id, event);
            merged.ready_states.extend(result.ready_states);
            merged.ready_edges.extend(result.ready_edges);
            merged
                .input_pending_nodes
                .extend(result.input_pending_nodes);
        }
        Some(merged)
    }
}

pub fn test_scalar_transform_algorithm(
    owner_node: u32,
    sort_index: u32,
    input_route_id: u32,
    output_route_id: u32,
) -> DataflowAlgorithm {
    DataflowAlgorithm::event_transform(
        owner_node,
        (owner_node, 100, sort_index as usize),
        vec![<ScalarInterface as InterfaceDataflow<ScalarEvent>>::edge(
            owner_node,
            ScalarEndpoint::new(input_route_id),
        )],
        vec![<ScalarInterface as InterfaceDataflow<ScalarEvent>>::edge(
            owner_node,
            ScalarEndpoint::new(output_route_id),
        )],
        Arc::new(NoopAlgorithm),
    )
}

struct ScalarFanoutAlgorithm {
    group_index: usize,
}

impl DataflowAlgorithmExecutor for ScalarFanoutAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        runtime.scalar_source_pending(self.group_index)
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        runtime.run_scalar_fanout(self.group_index)
    }
}

struct NoopAlgorithm;

impl DataflowAlgorithmExecutor for NoopAlgorithm {
    fn run(&self, _runtime: &mut dyn DataflowRuntime) -> bool {
        false
    }
}

/// Deliver an ingress scalar event through the backend's Rig runtime.
pub fn route_native_event(
    runtime: &mut dyn DataflowRuntime,
    source_node: u32,
    route_id: u32,
    event: ScalarEvent,
) {
    runtime.route_scalar_event(source_node, route_id, event);
}

pub fn apply_route_result(
    result: ScalarRouteResult,
    mut state_ready: impl FnMut(u32, u32, f32),
    mut edge_ready: impl FnMut(u32, u32),
    mut input_pending: impl FnMut(u32),
) {
    for (node, route_id, value) in result.ready_states {
        state_ready(node, route_id, value);
        edge_ready(node, route_id);
    }
    for (node, route_id) in result.ready_edges {
        edge_ready(node, route_id);
    }
    for sink_node in result.input_pending_nodes {
        input_pending(sink_node);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn no_scalar_count() -> u32 {
        0
    }

    unsafe extern "C" fn no_scalar_recv(_events: *mut ScalarEvent, _capacity: u32) -> u32 {
        0
    }

    #[test]
    fn scalar_interface_routes_events_and_records_latest_value() {
        let mut interface = ScalarInterface::default();
        interface.add_state_input(2, 7);
        interface.upsert_fanout(ScalarRoute {
            source_node: 1,
            route_id: 3,
            source_count: no_scalar_count,
            source_recv_many: no_scalar_recv,
            sink_node: 2,
            sink: ScalarSink::State {
                route_id: 7,
                sink_id: -1,
                value_scale: 1.0,
                set_value: None,
            },
        });

        let result = interface.route_event(
            1,
            3,
            ScalarEvent {
                value: 12.5,
                timestamp_ns: 42,
            },
        );

        assert_eq!(result.ready_states, vec![(2, 7, 12.5)]);
        assert_eq!(result.input_pending_nodes, vec![2]);
        assert_eq!(interface.latest(1, 3).unwrap().value, 12.5);
        assert_eq!(interface.state_input_values(2), vec![(7, 12.5)]);
    }
}
