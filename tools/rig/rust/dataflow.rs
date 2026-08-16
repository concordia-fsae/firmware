use std::any::TypeId;
use std::collections::{HashMap, HashSet, VecDeque};
use std::marker::PhantomData;
use std::sync::Arc;

use super::cluster::ClusterRuntime;
use super::scalar::ScalarEvent;

pub(super) trait DataflowEvent: Copy + Send + Sync + 'static {}

pub(super) type RuntimeResetFn = fn();
pub(super) type NodeResetFn = fn(usize, u64);
pub(super) type NativeScalarTakeFn = fn(usize, u64) -> Vec<ScalarEvent>;
pub(super) type NativeScalarReceiveFn = fn(usize, ScalarEvent) -> bool;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(super) enum DataflowSchedule {
    Polling,
    Event,
    Periodic { period_ns: u64, next_due_ns: u64 },
}

fn periodic_schedule(period_ns: u64, next_due_ns: u64) -> DataflowSchedule {
    assert!(
        period_ns != 0,
        "periodic dataflow schedule requires a positive period"
    );
    DataflowSchedule::Periodic {
        period_ns,
        next_due_ns,
    }
}

#[derive(Clone, Default)]
pub(super) struct DataflowAlgorithmLifecycle {
    pub(super) runtime_reset: Option<RuntimeResetFn>,
    pub(super) node_reset: Option<(u32, usize, NodeResetFn)>,
    pub(super) scalar_sources: Vec<(u32, u32, usize, NativeScalarTakeFn)>,
    pub(super) scalar_inputs: Vec<(u32, u32, usize, NativeScalarReceiveFn)>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub(super) struct DataflowChannel {
    pub(super) interface: i32,
    pub(super) port: i32,
    pub(super) channel: i32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(super) struct DataflowEdgeKey {
    pub(super) node: u32,
    data_type: TypeId,
    pub(super) channel: DataflowChannel,
}

/// A scheduler-owned subscription to an ingress edge or event queue.
///
/// A backend may create the subscription for any ingress source, but the
/// scheduler owns its completion state and the operation that waits on it.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(super) struct DataflowWait(pub(super) u64);

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

pub(super) trait DataflowAlgorithmExecutor: Send + Sync {
    fn pending(&self, _runtime: &ClusterRuntime) -> bool {
        false
    }

    fn is_python_node(&self) -> bool {
        false
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool;
}

#[derive(Clone)]
pub(super) struct DataflowAlgorithm {
    pub(super) owner_node: u32,
    pub(super) sort_key: (u32, u32, usize),
    pub(super) inputs: Vec<DataflowEdgeKey>,
    pub(super) outputs: Vec<DataflowEdgeKey>,
    pub(super) executor: Arc<dyn DataflowAlgorithmExecutor>,
    pub(super) schedule: DataflowSchedule,
    pub(super) lifecycle: DataflowAlgorithmLifecycle,
}

impl DataflowAlgorithm {
    pub(super) fn source(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        outputs: Vec<DataflowEdgeKey>,
        executor: Arc<dyn DataflowAlgorithmExecutor>,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs: Vec::new(),
            outputs,
            executor,
            schedule: DataflowSchedule::Polling,
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn periodic_source(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        outputs: Vec<DataflowEdgeKey>,
        executor: Arc<dyn DataflowAlgorithmExecutor>,
        period_ns: u64,
        next_due_ns: u64,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs: Vec::new(),
            outputs,
            executor,
            schedule: periodic_schedule(period_ns, next_due_ns),
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn event_transform(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        inputs: Vec<DataflowEdgeKey>,
        outputs: Vec<DataflowEdgeKey>,
        executor: Arc<dyn DataflowAlgorithmExecutor>,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs,
            outputs,
            executor,
            schedule: DataflowSchedule::Event,
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn event_sink(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        inputs: Vec<DataflowEdgeKey>,
        executor: Arc<dyn DataflowAlgorithmExecutor>,
    ) -> Self {
        Self::event_transform(owner_node, sort_key, inputs, Vec::new(), executor)
    }

    pub(super) fn periodic_transform(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        inputs: Vec<DataflowEdgeKey>,
        outputs: Vec<DataflowEdgeKey>,
        executor: Arc<dyn DataflowAlgorithmExecutor>,
        period_ns: u64,
        next_due_ns: u64,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs,
            outputs,
            executor,
            schedule: periodic_schedule(period_ns, next_due_ns),
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn python_periodic_node(
        owner_node: u32,
        sort_key: (u32, u32, usize),
        period_ns: u64,
        next_due_ns: u64,
    ) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs: Vec::new(),
            outputs: Vec::new(),
            executor: Arc::new(PythonNodeAlgorithm {
                node: owner_node,
                input_triggered: false,
            }),
            schedule: periodic_schedule(period_ns, next_due_ns),
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn python_input_node(owner_node: u32, sort_key: (u32, u32, usize)) -> Self {
        Self {
            owner_node,
            sort_key,
            inputs: Vec::new(),
            outputs: Vec::new(),
            executor: Arc::new(PythonNodeAlgorithm {
                node: owner_node,
                input_triggered: true,
            }),
            schedule: DataflowSchedule::Event,
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn with_runtime_reset(mut self, reset: RuntimeResetFn) -> Self {
        self.lifecycle.runtime_reset = Some(reset);
        self
    }

    pub(super) fn with_node_reset(
        mut self,
        node: u32,
        context: usize,
        reset: NodeResetFn,
    ) -> Self {
        self.lifecycle.node_reset = Some((node, context, reset));
        self
    }

    pub(super) fn with_scalar_source(
        mut self,
        node: u32,
        route_id: u32,
        context: usize,
        take: NativeScalarTakeFn,
    ) -> Self {
        self.lifecycle.scalar_sources.push((node, route_id, context, take));
        self
    }

    pub(super) fn with_scalar_input(
        mut self,
        node: u32,
        route_id: u32,
        context: usize,
        receive: NativeScalarReceiveFn,
    ) -> Self {
        self.lifecycle.scalar_inputs.push((node, route_id, context, receive));
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Mutex,
    };

    struct TestExecutor;

    impl DataflowAlgorithmExecutor for TestExecutor {
        fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
            true
        }
    }

    struct PendingTransformExecutor {
        runs: Arc<AtomicUsize>,
    }

    impl DataflowAlgorithmExecutor for PendingTransformExecutor {
        fn pending(&self, _runtime: &ClusterRuntime) -> bool {
            true
        }

        fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
            self.runs.fetch_add(1, Ordering::Relaxed);
            false
        }
    }

    #[test]
    fn edge_keys_include_node_type_and_channel() {
        let channel = DataflowChannel {
            interface: 1,
            port: 2,
            channel: 3,
        };
        let first = DataflowEdge::<ScalarEvent>::new(4, channel).key();
        let second = DataflowEdge::<ScalarEvent>::new(4, channel).key();
        let other_node = DataflowEdge::<ScalarEvent>::new(5, channel).key();

        assert_eq!(first, second);
        assert_ne!(first, other_node);
    }

    #[test]
    fn algorithm_constructors_preserve_shapes_and_schedule() {
        let input = DataflowEdge::<ScalarEvent>::new(
            1,
            DataflowChannel {
                interface: 0,
                port: 0,
                channel: 1,
            },
        )
        .key();
        let output = DataflowEdge::<ScalarEvent>::new(
            1,
            DataflowChannel {
                interface: 0,
                port: 0,
                channel: 2,
            },
        )
        .key();
        let algorithm = DataflowAlgorithm::periodic_transform(
            1,
            (1, 2, 3),
            vec![input],
            vec![output],
            Arc::new(TestExecutor),
            100,
            200,
        );

        assert_eq!(algorithm.owner_node, 1);
        assert_eq!(algorithm.sort_key, (1, 2, 3));
        assert_eq!(algorithm.inputs, vec![input]);
        assert_eq!(algorithm.outputs, vec![output]);
        assert_eq!(
            algorithm.schedule,
            DataflowSchedule::Periodic {
                period_ns: 100,
                next_due_ns: 200,
            }
        );

        let event_algorithm = DataflowAlgorithm::event_transform(
            1,
            (1, 2, 4),
            vec![input],
            vec![output],
            Arc::new(TestExecutor),
        );
        assert_eq!(event_algorithm.outputs, vec![output]);
        assert_eq!(event_algorithm.schedule, DataflowSchedule::Event);
    }

    #[test]
    #[should_panic(expected = "periodic dataflow schedule requires a positive period")]
    fn periodic_source_rejects_zero_period() {
        DataflowAlgorithm::periodic_source(
            u32::MAX,
            (0, 0, 0),
            Vec::new(),
            Arc::new(TestExecutor),
            0,
            0,
        );
    }

    #[test]
    #[should_panic(expected = "periodic dataflow schedule requires a positive period")]
    fn periodic_transform_rejects_zero_period() {
        DataflowAlgorithm::periodic_transform(
            u32::MAX,
            (0, 0, 0),
            Vec::new(),
            Vec::new(),
            Arc::new(TestExecutor),
            0,
            0,
        );
    }

    #[test]
    fn pending_event_transform_runs_from_an_input_edge_without_a_period() {
        let input = DataflowEdge::<ScalarEvent>::new(
            u32::MAX,
            DataflowChannel {
                interface: 1,
                port: 0,
                channel: 0,
            },
        )
        .key();
        let runs = Arc::new(AtomicUsize::new(0));
        let mut graph = DataflowGraph {
            algorithm_specs: vec![DataflowAlgorithm::event_sink(
                u32::MAX,
                (0, 0, 0),
                vec![input],
                Arc::new(PendingTransformExecutor {
                    runs: Arc::clone(&runs),
                }),
            )],
            ready_edges: std::collections::HashSet::from([input]),
            ..Default::default()
        };
        let runtime = ClusterRuntime::default();
        graph.rebuild(1, &runtime);
        let mut runtime = runtime;
        let mut ran_algorithms = vec![0; graph.algorithms.len()];

        graph.run_ready_algorithms(&mut runtime, &mut ran_algorithms, 1);

        assert_eq!(runs.load(Ordering::Relaxed), 1);
        assert_eq!(graph.algorithms[0].schedule, DataflowSchedule::Event);
    }

    struct OrderedExecutor {
        name: &'static str,
        order: Arc<Mutex<Vec<&'static str>>>,
    }

    impl DataflowAlgorithmExecutor for OrderedExecutor {
        fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
            self.order.lock().unwrap().push(self.name);
            true
        }
    }

    #[test]
    fn ready_sequential_graph_executes_every_stage_once_in_dependency_order() {
        let order = Arc::new(Mutex::new(Vec::new()));
        let channel = |channel| {
            DataflowEdge::<ScalarEvent>::new(
                u32::MAX,
                DataflowChannel {
                    interface: 0,
                    port: 0,
                    channel,
                },
            )
            .key()
        };
        let source_output = channel(1);
        let transform_output = channel(2);
        let executor = |name| {
            Arc::new(OrderedExecutor {
                name,
                order: Arc::clone(&order),
            }) as Arc<dyn DataflowAlgorithmExecutor>
        };

        let mut graph = DataflowGraph {
            algorithm_specs: vec![
                DataflowAlgorithm::source(u32::MAX, (0, 0, 0), vec![source_output], executor("source")),
                DataflowAlgorithm::event_transform(
                    u32::MAX,
                    (0, 0, 1),
                    vec![source_output],
                    vec![transform_output],
                    executor("transform"),
                ),
                DataflowAlgorithm::event_sink(
                    u32::MAX,
                    (0, 0, 2),
                    vec![transform_output],
                    executor("sink"),
                ),
            ],
            ..Default::default()
        };
        let mut runtime = ClusterRuntime::default();
        graph.rebuild(0, &runtime);

        // A source becomes ready from its model-owned event ingress. The queue
        // must then propagate readiness through each dependent stage.
        graph.enqueue_if_ready(0);
        let mut ran_algorithms = vec![0; graph.algorithms.len()];
        graph.run_queue(&mut runtime, &mut ran_algorithms, 1);

        assert_eq!(*order.lock().unwrap(), vec!["source", "transform", "sink"]);
        assert!(graph.queue.is_empty());
        assert!(ran_algorithms.iter().all(|generation| *generation == 1));
    }
}

struct PythonNodeAlgorithm {
    node: u32,
    input_triggered: bool,
}

impl DataflowAlgorithmExecutor for PythonNodeAlgorithm {
    fn pending(&self, runtime: &ClusterRuntime) -> bool {
        self.input_triggered && runtime.python_node_input_pending(self.node)
    }

    fn is_python_node(&self) -> bool {
        true
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        runtime.run_python_node_algorithm(self.node);
        false
    }
}

#[derive(Default)]
pub(super) struct DataflowGraph {
    pub(super) dirty: bool,
    pub(super) algorithm_specs: Vec<DataflowAlgorithm>,
    pub(super) algorithms: Vec<DataflowAlgorithm>,
    pub(super) edge_dependents: HashMap<DataflowEdgeKey, Vec<usize>>,
    pub(super) schedules_by_owner: Vec<Vec<usize>>,
    pub(super) input_algorithms_by_owner: Vec<Vec<usize>>,
    pub(super) polled_algorithms: Vec<usize>,
    pub(super) queue: VecDeque<usize>,
    pub(super) pending: Vec<bool>,
    pub(super) available_inputs: Vec<HashSet<DataflowEdgeKey>>,
    pub(super) ready_edges: HashSet<DataflowEdgeKey>,
    pub(super) configured_algorithms: Vec<DataflowAlgorithm>,
}

impl DataflowGraph {
    pub(super) fn reset(&mut self) {
        self.dirty = false;
        self.algorithm_specs.clear();
        self.algorithms.clear();
        self.edge_dependents.clear();
        self.schedules_by_owner.clear();
        self.input_algorithms_by_owner.clear();
        self.polled_algorithms.clear();
        self.queue.clear();
        self.pending.clear();
        self.available_inputs.clear();
        self.ready_edges.clear();
        self.configured_algorithms.clear();
    }

    pub(super) fn ordered_algorithms(algorithms: &[DataflowAlgorithm]) -> Vec<DataflowAlgorithm> {
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

    pub(super) fn rebuild(&mut self, node_count: usize, runtime: &ClusterRuntime) {
        self.algorithms = Self::ordered_algorithms(&self.algorithm_specs);
        self.edge_dependents.clear();
        self.schedules_by_owner = vec![Vec::new(); node_count];
        self.input_algorithms_by_owner = vec![Vec::new(); node_count];
        self.polled_algorithms.clear();
        self.queue.clear();
        self.pending = vec![false; self.algorithms.len()];
        self.available_inputs = vec![HashSet::new(); self.algorithms.len()];

        for index in 0..self.algorithms.len() {
            if self.algorithms[index].schedule == DataflowSchedule::Polling {
                self.polled_algorithms.push(index);
            }
            if matches!(self.algorithms[index].schedule, DataflowSchedule::Periodic { .. }) {
                let owner_node = self.algorithms[index].owner_node as usize;
                if let Some(schedules) = self.schedules_by_owner.get_mut(owner_node) {
                    schedules.push(index);
                }
            }
            if self.is_python_input_algorithm(index) {
                let owner_node = self.algorithms[index].owner_node as usize;
                if let Some(input_algorithms) = self.input_algorithms_by_owner.get_mut(owner_node) {
                    input_algorithms.push(index);
                }
            }
            let inputs = self.algorithms[index].inputs.clone();
            for input in inputs {
                if self.edge_available(input) {
                    if let Some(available) = self.available_inputs.get_mut(index) {
                        available.insert(input);
                    }
                }
                self.edge_dependents.entry(input).or_default().push(index);
            }
        }
        for indexes in self.edge_dependents.values_mut() {
            indexes.sort_by_key(|index| self.algorithms[*index].sort_key);
            indexes.dedup();
        }
        for index in 0..self.algorithms.len() {
            if self.algorithms[index].schedule == DataflowSchedule::Polling
                && self.pending_state(runtime, index)
            {
                self.enqueue_if_ready(index);
            }
            if self.algorithms[index].schedule == DataflowSchedule::Event
                && !self.algorithms[index].inputs.is_empty()
                && self.algorithm_inputs_ready(index)
            {
                self.enqueue_if_ready(index);
            }
        }
    }

    pub(super) fn run_ready_algorithms(
        &mut self,
        runtime: &mut ClusterRuntime,
        ran_algorithms: &mut [u64],
        generation: u64,
    ) {
        self.enqueue_pending_algorithms(runtime, ran_algorithms, generation);
        self.run_due_algorithms(runtime);
        self.run_queue(runtime, ran_algorithms, generation);
    }

    pub(super) fn run_queued_algorithms(
        &mut self,
        runtime: &mut ClusterRuntime,
        ran_algorithms: &mut [u64],
        generation: u64,
    ) {
        self.run_queue(runtime, ran_algorithms, generation);
    }

    pub(super) fn mark_edge_ready(&mut self, key: DataflowEdgeKey) {
        self.ready_edges.insert(key);
        let Some(indexes) = self.edge_dependents.get(&key).cloned() else {
            return;
        };
        for index in indexes {
            if let Some(available) = self.available_inputs.get_mut(index) {
                available.insert(key);
            }
            self.enqueue_if_ready(index);
        }
    }

    pub(super) fn mark_owner_input_pending(&mut self, owner_node: u32) {
        let Some(indexes) = self
            .input_algorithms_by_owner
            .get(owner_node as usize)
            .cloned()
        else {
            return;
        };
        for index in indexes {
            self.enqueue_if_ready(index);
        }
    }

    fn algorithm_inputs_ready(&self, index: usize) -> bool {
        let Some(algorithm) = self.algorithms.get(index) else {
            return false;
        };
        if algorithm.inputs.is_empty() {
            return true;
        }
        let Some(available) = self.available_inputs.get(index) else {
            return false;
        };
        algorithm
            .inputs
            .iter()
            .all(|input| available.contains(input))
    }

    fn edge_available(&self, edge: DataflowEdgeKey) -> bool {
        self.ready_edges.contains(&edge)
    }

    fn pending_state(&self, runtime: &ClusterRuntime, index: usize) -> bool {
        let Some(algorithm) = self.algorithms.get(index) else {
            return false;
        };
        algorithm.executor.pending(runtime)
    }

    fn is_python_input_algorithm(&self, index: usize) -> bool {
        let Some(algorithm) = self.algorithms.get(index) else {
            return false;
        };
        algorithm.executor.is_python_node()
            && algorithm.schedule == DataflowSchedule::Event
            && algorithm.inputs.is_empty()
    }

    pub(super) fn enqueue_pending_algorithms(
        &mut self,
        runtime: &ClusterRuntime,
        ran_algorithms: &[u64],
        generation: u64,
    ) {
        for position in 0..self.polled_algorithms.len() {
            let index = self.polled_algorithms[position];
            if ran_algorithms.get(index).copied() == Some(generation) {
                continue;
            }
            if self.pending_state(runtime, index) {
                self.enqueue_if_ready(index);
            }
        }
    }

    fn enqueue(&mut self, index: usize) {
        if index >= self.algorithms.len() {
            return;
        }
        if !self.pending.get(index).copied().unwrap_or(false) {
            if let Some(pending) = self.pending.get_mut(index) {
                *pending = true;
            }
            self.queue.push_back(index);
        }
    }

    fn enqueue_if_ready(&mut self, index: usize) {
        if self.algorithm_inputs_ready(index) {
            self.enqueue(index);
        }
    }

    fn run_due_algorithms(&mut self, runtime: &ClusterRuntime) {
        for node_index in 0..self.schedules_by_owner.len() {
            if !runtime.node_online(node_index as u32) {
                continue;
            }
            for position in 0..self.schedules_by_owner[node_index].len() {
                let index = self.schedules_by_owner[node_index][position];
                let Some(algorithm) = self.algorithms.get(index) else {
                    continue;
                };
                let DataflowSchedule::Periodic {
                    period_ns,
                    next_due_ns,
                } = algorithm.schedule else {
                    continue;
                };
                assert!(
                    period_ns != 0,
                    "periodic dataflow algorithms require a positive period"
                );
                if next_due_ns > runtime.elapsed_ns {
                    continue;
                }
                self.enqueue_if_ready(index);
                if let Some(algorithm) = self.algorithms.get_mut(index) {
                    if let DataflowSchedule::Periodic { next_due_ns, .. } =
                        &mut algorithm.schedule
                    {
                        while *next_due_ns <= runtime.elapsed_ns {
                            *next_due_ns = next_due_ns.saturating_add(period_ns);
                        }
                    }
                }
            }
        }
    }

    fn run_queue(
        &mut self,
        runtime: &mut ClusterRuntime,
        ran_algorithms: &mut [u64],
        generation: u64,
    ) {
        let mut queue_pops = 0usize;
        while let Some(index) = self.queue.pop_front() {
            queue_pops += 1;
            assert!(
                queue_pops <= self.algorithms.len().saturating_add(1),
                "dataflow algorithm queue did not converge ({} algorithms, repeating index {})",
                self.algorithms.len(),
                index,
            );
            let Some(pending) = self.pending.get_mut(index) else {
                continue;
            };
            if !*pending {
                continue;
            }
            *pending = false;
            let Some(algorithm) = self.algorithms.get(index) else {
                continue;
            };
            if ran_algorithms.get(index).copied() == Some(generation) {
                continue;
            }
            ran_algorithms[index] = generation;
            let owner_node = algorithm.owner_node;
            if owner_node != u32::MAX && !runtime.node_online(owner_node) {
                continue;
            }
            let produced_outputs = algorithm.executor.run(runtime);
            if produced_outputs {
                let output_count = self.algorithms[index].outputs.len();
                for output_position in 0..output_count {
                    let output = self.algorithms[index].outputs[output_position];
                    self.mark_edge_ready(output);
                }
            }
        }
    }
}
