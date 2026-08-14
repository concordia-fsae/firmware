use std::collections::{HashMap, VecDeque};
use std::sync::Arc;

use super::cluster::{
    ClusterRuntime, ClusterScalarCountFn, ClusterScalarRecvManyFn, ClusterScalarSendManyFn,
    ClusterScalarSinkSetFn,
};
use super::algorithms::NativeScalarReceiveFn;
use super::dataflow::{
    DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowChannel, DataflowEdgeKey,
    DataflowEvent,
};
use super::interfaces::{InterfaceCaller, InterfaceDataflow, InterfaceEndpoint, InterfaceImplementation};
use super::registry::RuntimeInterfaces;
use super::scheduler;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ScalarEvent {
    pub value: f32,
    pub timestamp_ns: u64,
}

impl DataflowEvent for ScalarEvent {}

#[derive(Clone, Copy)]
pub(super) enum ClusterScalarSink {
    SendMany(ClusterScalarSendManyFn),
    State {
        route_id: u32,
    },
    Native {
        sink_id: i32,
        value_scale: f32,
        set_value: ClusterScalarSinkSetFn,
    },
    Algorithm {
        context: usize,
        route_id: u32,
        receive: NativeScalarReceiveFn,
    },
}

impl ClusterScalarSink {
    pub(super) fn same_target(&self, other: Self) -> bool {
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
            (
                Self::Algorithm { context, route_id, receive },
                Self::Algorithm {
                    context: other_context,
                    route_id: other_route_id,
                    receive: other_receive,
                },
            ) => context == other_context
                && route_id == other_route_id
                && receive as usize == other_receive as usize,
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
            Self::Algorithm { context, receive, .. } => events
                .iter()
                .filter(|event| receive(context, **event))
                .count()
                .min(u32::MAX as usize) as u32,
        }
    }
}

#[derive(Clone, Copy)]
pub(super) struct ClusterScalarRoute {
    pub(super) source_node: u32,
    pub(super) route_id: u32,
    pub(super) source_count: ClusterScalarCountFn,
    pub(super) source_recv_many: ClusterScalarRecvManyFn,
    pub(super) sink_node: u32,
    pub(super) sink: ClusterScalarSink,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterScalarSinkRoute {
    pub(super) sink_node: u32,
    pub(super) sink: ClusterScalarSink,
}

#[derive(Clone, Copy)]
pub(super) struct ClusterScalarRecord {
    pub(super) source_node: u32,
    pub(super) route_id: u32,
    pub(super) event: ScalarEvent,
}

pub(super) struct ScalarInterfaceFanout {
    pub(super) source_node: u32,
    pub(super) route_id: u32,
    pub(super) source_count: ClusterScalarCountFn,
    pub(super) source_recv_many: ClusterScalarRecvManyFn,
    pub(super) sinks: Vec<ClusterScalarSinkRoute>,
}

pub(super) struct ScalarRouteResult {
    pub(super) ready_states: Vec<(u32, u32, f32)>,
    pub(super) ready_edges: Vec<(u32, u32)>,
    pub(super) input_pending_nodes: Vec<u32>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub(super) struct ScalarEndpoint {
    route_id: u32,
}

impl ScalarEndpoint {
    pub(super) fn new(route_id: u32) -> Self {
        Self { route_id }
    }
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
pub(super) struct ScalarInterface {
    pub(super) fanout_indexes: HashMap<(u32, u32), usize>,
    pub(super) fanouts: Vec<ScalarInterfaceFanout>,
    pub(super) state_route_count: usize,
    pub(super) states: HashMap<(u32, u32), f32>,
    pub(super) state_inputs: HashMap<u32, Vec<u32>>,
    pub(super) records: VecDeque<ClusterScalarRecord>,
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
    fn reset(&mut self) { self.reset(); }
    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        self.append_algorithm_specs(specs);
    }
}

impl InterfaceDataflow<ScalarEvent> for ScalarInterface {
    type Endpoint = ScalarEndpoint;
}

impl ScalarInterface {
    pub(super) fn reset(&mut self) {
        self.reset_interface();
    }

    pub(super) fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
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

    pub(super) fn add_state_input(&mut self, node: u32, route_id: u32) {
        let inputs = self.state_inputs.entry(node).or_default();
        if !inputs.contains(&route_id) {
            inputs.push(route_id);
            inputs.sort_unstable();
        }
    }

    pub(super) fn state_input_values(&self, node: u32) -> Vec<(u32, f32)> {
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

    pub(super) fn record(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        self.records.push_back(ClusterScalarRecord {
            source_node,
            route_id,
            event,
        });
    }

    pub(super) fn latest(&self, source_node: u32, route_id: u32) -> Option<ScalarEvent> {
        self.records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.route_id == route_id)
            .map(|record| record.event)
    }

    pub(super) fn route_exists(
        &self,
        source_node: u32,
        route_id: u32,
        sink_node: u32,
        sink: ClusterScalarSink,
    ) -> bool {
        let Some(group_index) = self.fanout_indexes.get(&(source_node, route_id)).copied() else {
            return false;
        };
        self.fanouts[group_index]
            .sinks
            .iter()
            .any(|existing| existing.sink_node == sink_node && existing.sink.same_target(sink))
    }

    pub(super) fn upsert_fanout(&mut self, route: ClusterScalarRoute) {
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
        self.fanouts[group_index]
            .sinks
            .push(ClusterScalarSinkRoute {
                sink_node: route.sink_node,
                sink: route.sink,
            });
    }

    pub(super) fn route_event(
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
                ClusterScalarSink::State {
                    route_id: sink_route_id,
                } => {
                    self.states
                        .insert((sink.sink_node, sink_route_id), event.value);
                    result
                        .ready_states
                        .push((sink.sink_node, sink_route_id, event.value));
                    1
                }
                ClusterScalarSink::Algorithm { route_id, .. } => {
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

pub(super) fn test_scalar_transform_algorithm(
    owner_node: u32,
    sort_index: u32,
    input_route_id: u32,
    output_route_id: u32,
) -> DataflowAlgorithm {
    DataflowAlgorithm::transform(
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
    fn polls_pending(&self) -> bool {
        true
    }

    fn pending(&self, runtime: &ClusterRuntime) -> bool {
        let native_scalar_sources = runtime.algorithms.native_scalar_source_keys();
        runtime.interfaces.scalar_fanout_pending(
            self.group_index,
            |source_node| runtime.node_online(source_node),
            |source_node, route_id| native_scalar_sources.contains(&(source_node, route_id)),
        )
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        run_scalar_fanout(runtime, self.group_index)
    }
}

fn run_scalar_fanout(runtime: &mut ClusterRuntime, group_index: usize) -> bool {
    let online_nodes = runtime.online_nodes();
    let native_scalar_sources = runtime.algorithms.native_scalar_source_keys();
    let Some(result) = runtime.interfaces.scalar_route_fanout(
        group_index,
        |node| online_nodes.get(node as usize).copied().unwrap_or(false),
        |source_node, route_id| native_scalar_sources.contains(&(source_node, route_id)),
    ) else {
        return false;
    };
    apply_route_result(runtime, result);
    true
}

struct NoopAlgorithm;

impl DataflowAlgorithmExecutor for NoopAlgorithm {
    fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
        false
    }
}

pub(super) fn route_native_event(
    runtime: &mut ClusterRuntime,
    source_node: u32,
    route_id: u32,
    event: ScalarEvent,
) {
    let result = runtime
        .interfaces
        .scalar
        .route_event(source_node, route_id, event);
    apply_route_result(runtime, result);
}

pub(super) fn apply_route_result(runtime: &mut ClusterRuntime, result: ScalarRouteResult) {
    for (node, route_id, value) in result.ready_states {
        runtime
            .interfaces
            .timer
            .update_scaled_scalar_scale(node, route_id, value);
        scheduler::mark_dataflow_edge_ready(
            runtime,
            RuntimeInterfaces::scalar_edge(node, route_id),
        );
    }
    for (node, route_id) in result.ready_edges {
        scheduler::mark_dataflow_edge_ready(runtime, RuntimeInterfaces::scalar_edge(node, route_id));
    }
    for sink_node in result.input_pending_nodes {
        scheduler::mark_input_pending(runtime, sink_node);
    }
}
