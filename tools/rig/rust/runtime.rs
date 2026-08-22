// Standalone, backend-neutral Rig runtime.
//
// The runtime owns nodes, dataflow algorithms, edge readiness, waits, and
// simulation time. A backend can add generic dataflow algorithms through
// `RigBackend`, but this type has no knowledge of CAN, firmware, or any
// repository-specific peripheral.

use super::algorithms::{self, RuntimeAlgorithms};
use super::dataflow::{DataflowAlgorithm, DataflowEdgeKey, DataflowRuntime, DataflowWait};
use super::node::{
    RigNode, RigNodeResetFn, RigNodeRunForFn, RigNodeScheduler, RigPythonScheduledFn,
};
use super::scalar::{
    self, ScalarCountFn, ScalarEvent, ScalarInterface, ScalarRecvManyFn, ScalarRoute, ScalarSink,
    ScalarSinkSetFn,
};
use super::scheduler::{self, RigScheduler};

// Keep this re-export available to the test module when this file is included
// by a firmware runtime module as well as when it is compiled standalone.
#[allow(unused_imports)]
pub(crate) use super::dataflow;

/// A generic extension point for dataflow providers.
pub trait RigBackend: Default {
    fn reset(&mut self) {}
    fn reset_node(&mut self, _node: u32) {}
    /// Notify the backend when a generic dataflow wait is canceled.
    ///
    /// Backends may own ingress registrations associated with a wait.  The
    /// default keeps the standalone runtime backend-neutral while allowing a
    /// backend to release those registrations at the same lifecycle point as
    /// the scheduler.
    fn cancel_dataflow_wait(&mut self, _wait: DataflowWait) {}
    fn append_algorithm_specs(&self, _specs: &mut Vec<DataflowAlgorithm>) {}
    fn scalar_state_ready(&mut self, _node: u32, _route_id: u32, _value: f32) {}
    fn scalar_interface(&self) -> &ScalarInterface;
    fn scalar_interface_mut(&mut self) -> &mut ScalarInterface;
}

/// Empty backend used by the standalone Rig runtime.
#[derive(Default)]
pub struct NoBackend {
    scalar: ScalarInterface,
}

impl RigBackend for NoBackend {
    fn scalar_interface(&self) -> &ScalarInterface {
        &self.scalar
    }
    fn scalar_interface_mut(&mut self) -> &mut ScalarInterface {
        &mut self.scalar
    }
}

/// Generic Rig runtime parameterized by an optional backend extension.
pub struct RigRuntime<B: RigBackend = NoBackend> {
    pub(crate) nodes: Vec<RigNode>,
    pub(crate) algorithms: RuntimeAlgorithms,
    pub(crate) scheduler: RigScheduler,
    pub(crate) backend: B,
    pub(crate) elapsed_ns: u64,
}

impl<B: RigBackend + 'static> std::ops::Deref for RigRuntime<B> {
    type Target = B;

    fn deref(&self) -> &Self::Target {
        &self.backend
    }
}

impl<B: RigBackend + 'static> std::ops::DerefMut for RigRuntime<B> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.backend
    }
}

impl<B: RigBackend + 'static> Default for RigRuntime<B> {
    fn default() -> Self {
        Self::new(B::default())
    }
}

impl<B: RigBackend + 'static> RigRuntime<B> {
    pub fn new(backend: B) -> Self {
        Self {
            nodes: Vec::new(),
            algorithms: RuntimeAlgorithms::default(),
            scheduler: RigScheduler::default(),
            backend,
            elapsed_ns: 0,
        }
    }

    pub fn backend(&self) -> &B {
        &self.backend
    }

    pub fn backend_mut(&mut self) -> &mut B {
        &mut self.backend
    }

    pub fn reset(&mut self) {
        self.nodes.clear();
        self.algorithms.reset();
        self.scheduler.reset();
        self.backend.reset();
        self.elapsed_ns = 0;
    }

    pub fn add_node(
        &mut self,
        run_for: RigNodeRunForFn,
        reset: RigNodeResetFn,
        online: bool,
    ) -> u32 {
        self.nodes.push(RigNode::external(run_for, reset, online));
        self.scheduler.mark_dirty();
        (self.nodes.len() - 1) as u32
    }

    pub fn add_python_node(
        &mut self,
        scheduled: Option<RigPythonScheduledFn>,
        reset: RigNodeResetFn,
        period_ns: u64,
        online: bool,
    ) -> u32 {
        self.nodes
            .push(RigNode::python(scheduled, reset, period_ns, online));
        self.scheduler.mark_dirty();
        (self.nodes.len() - 1) as u32
    }

    pub fn add_rust_runtime_model_node(&mut self, online: bool) -> u32 {
        self.nodes.push(RigNode::rust_runtime_model(online));
        self.scheduler.mark_dirty();
        (self.nodes.len() - 1) as u32
    }

    pub fn set_node_online(&mut self, node: u32, online: bool) -> bool {
        let Some(element) = self.nodes.get_mut(node as usize) else {
            return false;
        };
        if element.online && !online {
            if let Some(reset) = element.reset {
                unsafe { reset() };
            }
            element.elapsed_ns = 0;
            if let RigNodeScheduler::Python { input_pending, .. } = &mut element.scheduler {
                *input_pending = false;
            }
        }
        if !element.online && online {
            if let RigNodeScheduler::Python { input_pending, .. } = &mut element.scheduler {
                *input_pending = false;
            }
        }
        element.online = online;
        if !online {
            self.algorithms.reset_node_models(node, self.elapsed_ns);
            self.backend.reset_node(node);
        }
        self.scheduler.mark_dirty();
        true
    }

    pub fn register_algorithm(&mut self, algorithm: DataflowAlgorithm) -> bool {
        algorithms::register_algorithm(self, algorithm)
    }

    pub fn mark_edge_ready(&mut self, edge: DataflowEdgeKey) {
        scheduler::mark_dataflow_edge_ready(self, edge);
    }

    pub fn mark_input_pending(&mut self, node: u32) {
        scheduler::mark_input_pending(self, node);
    }

    pub fn begin_dataflow_wait(&mut self) -> DataflowWait {
        self.scheduler.begin_dataflow_wait()
    }

    pub fn dataflow_wait_matched(&self, wait: DataflowWait) -> bool {
        self.scheduler.dataflow_wait_matched(wait)
    }

    pub fn cancel_dataflow_wait(&mut self, wait: DataflowWait) {
        self.backend.cancel_dataflow_wait(wait);
        self.scheduler.cancel_dataflow_wait(wait);
    }

    pub fn run_for_ns(&mut self, duration_ns: u64, max_step_ns: u64) -> u64 {
        let mut remaining_ns = duration_ns;
        let mut advanced_ns = 0;
        while remaining_ns != 0 {
            let delta = scheduler::run_next_step(self, remaining_ns, max_step_ns);
            if delta == 0 {
                break;
            }
            remaining_ns -= delta;
            advanced_ns += delta;
        }
        advanced_ns
    }

    pub fn elapsed_ns(&self) -> u64 {
        self.elapsed_ns
    }

    pub fn node_count(&self) -> usize {
        self.nodes.len()
    }

    pub fn node_exists(&self, node: u32) -> bool {
        self.nodes.get(node as usize).is_some()
    }

    pub fn node_online(&self, node: u32) -> bool {
        self.nodes
            .get(node as usize)
            .is_some_and(|node| node.online)
    }

    pub fn online_nodes(&self) -> Vec<bool> {
        self.nodes.iter().map(|node| node.online).collect()
    }

    pub fn node_elapsed_ns(&self, node: u32) -> u64 {
        self.nodes
            .get(node as usize)
            .map(|node| node.elapsed_ns)
            .unwrap_or(0)
    }

    pub fn node_elapsed_ns_many(&self, out: &mut [u64]) -> u32 {
        let count = self.nodes.len().min(out.len()).min(u32::MAX as usize);
        for (slot, node) in out.iter_mut().zip(self.nodes.iter()).take(count) {
            *slot = node.elapsed_ns;
        }
        count as u32
    }

    pub fn scalar_event(value: f32, timestamp_ns: u64) -> ScalarEvent {
        ScalarEvent {
            value,
            timestamp_ns,
        }
    }

    pub fn scalar_interface(&self) -> &ScalarInterface {
        self.backend.scalar_interface()
    }

    pub fn scalar_interface_mut(&mut self) -> &mut ScalarInterface {
        self.backend.scalar_interface_mut()
    }

    pub fn register_scalar_route(&mut self, route: ScalarRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.scalar_interface_mut().register_route(route);
        self.scheduler.mark_dirty();
        true
    }

    pub fn add_scalar_state_input(&mut self, node: u32, route_id: u32) {
        self.scalar_interface_mut().add_state_input(node, route_id);
    }

    pub fn scalar_state_input_values(&self, node: u32) -> Vec<(u32, f32)> {
        self.scalar_interface().state_input_values(node)
    }

    pub fn latest_scalar_event(&self, source_node: u32, route_id: u32) -> Option<ScalarEvent> {
        self.scalar_interface().latest(source_node, route_id)
    }

    pub fn add_scalar_state_sink(&mut self, node: u32, route_id: u32, initial_value: f32) -> bool {
        if self.nodes.get(node as usize).is_none() || !initial_value.is_finite() {
            return false;
        }
        self.add_scalar_state_input(node, route_id);
        self.scalar_interface_mut()
            .set_state(node, route_id, initial_value);
        self.scheduler
            .add_initial_ready_edge(scalar::edge(node, route_id));
        true
    }

    pub fn add_scalar_state_route(
        &mut self,
        source_node: u32,
        route_id: u32,
        source_count: ScalarCountFn,
        source_recv_many: ScalarRecvManyFn,
        sink_node: u32,
        sink_route_id: u32,
        sink_id: i32,
        value_scale: f32,
        set_value: Option<ScalarSinkSetFn>,
    ) -> bool {
        self.add_scalar_state_input(sink_node, sink_route_id);
        if !self.register_scalar_route(ScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ScalarSink::State {
                route_id: sink_route_id,
                sink_id,
                value_scale,
                set_value,
            },
        }) {
            return false;
        }

        let elapsed_ns = self.elapsed_ns;
        let events = self
            .algorithms
            .take_native_scalar_events(source_node, route_id, elapsed_ns);
        if let Some(event) = events.last() {
            self.scalar_interface_mut()
                .set_state(sink_node, sink_route_id, event.value);
            self.backend
                .scalar_state_ready(sink_node, sink_route_id, event.value);
            self.scalar_interface_mut().record(
                sink_node,
                sink_route_id,
                ScalarEvent {
                    value: event.value,
                    timestamp_ns: elapsed_ns,
                },
            );
            self.scheduler
                .add_initial_ready_edge(scalar::edge(sink_node, sink_route_id));
        }
        for event in events {
            self.scalar_interface_mut()
                .record(source_node, route_id, event);
        }
        self.scheduler.mark_dirty();
        true
    }

    pub fn add_scalar_input_route(
        &mut self,
        source_node: u32,
        route_id: u32,
        source_count: ScalarCountFn,
        source_recv_many: ScalarRecvManyFn,
        sink_node: u32,
        sink_route_id: u32,
    ) -> bool {
        let Some((context, receive)) = self
            .algorithms
            .native_scalar_input(sink_node, sink_route_id)
        else {
            return false;
        };
        self.register_scalar_route(ScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ScalarSink::Algorithm {
                context,
                route_id: sink_route_id,
                receive,
            },
        })
    }

    pub fn route_scalar_event(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        let result = self
            .backend
            .scalar_interface_mut()
            .route_event(source_node, route_id, event);
        self.apply_scalar_route_result(result);
    }

    fn apply_scalar_route_result(&mut self, result: scalar::ScalarRouteResult) {
        let mut state_ready = Vec::new();
        let mut edge_ready = Vec::new();
        let mut input_pending = Vec::new();
        scalar::apply_route_result(
            result,
            |node, route_id, value| state_ready.push((node, route_id, value)),
            |node, route_id| edge_ready.push((node, route_id)),
            |node| input_pending.push(node),
        );
        for (node, route_id, value) in state_ready {
            self.backend.scalar_state_ready(node, route_id, value);
            self.mark_scalar_edge_ready(node, route_id);
        }
        for (node, route_id) in edge_ready {
            self.mark_scalar_edge_ready(node, route_id);
        }
        for node in input_pending {
            self.mark_scalar_input_pending(node);
        }
    }
}

impl<B: RigBackend + 'static> DataflowRuntime for RigRuntime<B> {
    fn as_any(&self) -> &dyn std::any::Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn std::any::Any {
        self
    }

    fn scheduler(&self) -> &RigScheduler {
        &self.scheduler
    }

    fn scheduler_mut(&mut self) -> &mut RigScheduler {
        &mut self.scheduler
    }

    fn node_count(&self) -> usize {
        self.nodes.len()
    }

    fn node_exists(&self, node: u32) -> bool {
        self.nodes.get(node as usize).is_some()
    }

    fn node_online(&self, node: u32) -> bool {
        self.node_online(node)
    }

    fn python_node_input_pending(&self, node: u32) -> bool {
        self.nodes
            .get(node as usize)
            .is_some_and(RigNode::python_input_pending)
    }

    fn run_python_node_algorithm(&mut self, node: u32) {
        if let Some(node) = self.nodes.get_mut(node as usize) {
            node.run_python_algorithm(self.elapsed_ns);
        }
    }

    fn mark_node_input_pending(&mut self, node: u32) {
        if let Some(node) = self.nodes.get_mut(node as usize) {
            node.mark_input_pending();
        }
    }

    fn run_external_nodes(&mut self, delta_ns: u64) {
        for node in self
            .nodes
            .iter_mut()
            .filter(|node| node.online && node.needs_run_step())
        {
            node.run_for(delta_ns);
        }
    }

    fn advance_time(&mut self, delta_ns: u64) {
        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
    }

    fn elapsed_ns(&self) -> u64 {
        self.elapsed_ns
    }

    fn python_period_ns(&self, node: u32) -> Option<u64> {
        self.nodes
            .get(node as usize)
            .and_then(RigNode::python_period_ns)
    }

    fn has_python_input_callback(&self, node: u32) -> bool {
        self.nodes
            .get(node as usize)
            .is_some_and(RigNode::has_python_input_callback)
    }

    fn append_backend_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        self.backend.append_algorithm_specs(specs);
    }

    fn runtime_algorithms(&self) -> &RuntimeAlgorithms {
        &self.algorithms
    }

    fn runtime_algorithms_mut(&mut self) -> &mut RuntimeAlgorithms {
        &mut self.algorithms
    }

    fn mark_scheduler_dirty(&mut self) {
        self.scheduler.mark_dirty();
    }

    fn scalar_source_pending(&self, group_index: usize) -> bool {
        self.backend.scalar_interface().fanout_pending(
            group_index,
            |node| self.node_online(node),
            |node, route_id| self.algorithms.has_native_scalar_source(node, route_id),
        )
    }

    fn run_scalar_fanout(&mut self, group_index: usize) -> bool {
        let online_nodes: Vec<bool> = self.nodes.iter().map(|node| node.online).collect();
        let result = {
            let online = |node| online_nodes.get(node as usize).copied().unwrap_or(false);
            let skip = |node, route_id| self.algorithms.has_native_scalar_source(node, route_id);
            self.backend
                .scalar_interface_mut()
                .route_fanout(group_index, online, skip)
        };
        let Some(result) = result else {
            return false;
        };
        self.apply_scalar_route_result(result);
        true
    }

    fn mark_scalar_edge_ready(&mut self, node: u32, route_id: u32) {
        scheduler::mark_dataflow_edge_ready(self, scalar::ScalarEndpoint::new(route_id).edge(node));
    }

    fn mark_scalar_input_pending(&mut self, node: u32) {
        scheduler::mark_input_pending(self, node);
    }

    fn route_scalar_event(&mut self, source_node: u32, route_id: u32, event: ScalarEvent) {
        RigRuntime::route_scalar_event(self, source_node, route_id, event);
    }
}

#[cfg(test)]
mod tests {
    use super::dataflow::{DataflowAlgorithmExecutor, DataflowChannel, DataflowEdge};
    use super::*;
    use std::sync::{
        Arc,
        atomic::{AtomicUsize, Ordering},
    };

    #[derive(Default)]
    struct TestBackend {
        scalar: ScalarInterface,
        reset_count: usize,
        canceled_wait: Option<DataflowWait>,
    }

    impl RigBackend for TestBackend {
        fn reset(&mut self) {
            self.reset_count += 1;
        }

        fn cancel_dataflow_wait(&mut self, wait: DataflowWait) {
            self.canceled_wait = Some(wait);
        }

        fn scalar_interface(&self) -> &ScalarInterface {
            &self.scalar
        }

        fn scalar_interface_mut(&mut self) -> &mut ScalarInterface {
            &mut self.scalar
        }
    }

    struct CountingExecutor(Arc<AtomicUsize>);

    impl DataflowAlgorithmExecutor for CountingExecutor {
        fn run(&self, _runtime: &mut dyn DataflowRuntime) -> bool {
            self.0.fetch_add(1, Ordering::Relaxed);
            true
        }
    }

    #[test]
    fn parameterized_runtime_runs_a_generic_event_graph() {
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);
        let edge = DataflowEdge::<ScalarEvent>::new(
            node,
            DataflowChannel {
                interface: 1,
                port: 2,
                channel: 3,
            },
        )
        .key();
        let runs = Arc::new(AtomicUsize::new(0));
        assert!(
            runtime.register_algorithm(DataflowAlgorithm::periodic_source(
                node,
                (0, 0, 0),
                vec![edge],
                Arc::new(CountingExecutor(Arc::clone(&runs))),
                1,
                1,
            ))
        );

        assert_eq!(runtime.run_for_ns(1, 1), 1);
        assert_eq!(runs.load(Ordering::Relaxed), 1);
        runtime.mark_edge_ready(edge);
        let wait = runtime.begin_dataflow_wait();
        assert!(!runtime.dataflow_wait_matched(wait));
    }

    #[test]
    fn backend_parameterization_keeps_firmware_capabilities_out_of_rig() {
        let mut runtime = RigRuntime::<TestBackend>::default();
        assert_eq!(runtime.backend().reset_count, 0);
        runtime.reset();
        assert_eq!(runtime.backend().reset_count, 1);
        assert!(std::ptr::eq(
            runtime.scalar_interface(),
            &runtime.backend().scalar,
        ));
    }

    #[test]
    fn canceling_a_wait_notifies_the_backend_lifecycle_hook() {
        let mut runtime = RigRuntime::<TestBackend>::default();
        let wait = runtime.begin_dataflow_wait();

        runtime.cancel_dataflow_wait(wait);

        assert_eq!(runtime.backend().canceled_wait, Some(wait));
        assert!(!runtime.dataflow_wait_matched(wait));
    }
}
