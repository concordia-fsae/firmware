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

#[derive(Clone, Copy, Default)]
pub(super) struct DataflowAlgorithmLifecycle {
    pub(super) runtime_reset: Option<RuntimeResetFn>,
    pub(super) node_reset: Option<(u32, usize, NodeResetFn)>,
    pub(super) scalar_source: Option<(u32, u32, usize, NativeScalarTakeFn)>,
    pub(super) scalar_input: Option<(u32, u32, usize, NativeScalarReceiveFn)>,
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
    fn polls_pending(&self) -> bool {
        false
    }

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
    pub(super) period_ns: u64,
    pub(super) next_due_ns: u64,
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
            period_ns: 0,
            next_due_ns: 0,
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
            period_ns,
            next_due_ns,
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
    }

    pub(super) fn transform(
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
            period_ns: 0,
            next_due_ns: 0,
            lifecycle: DataflowAlgorithmLifecycle::default(),
        }
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
            period_ns,
            next_due_ns,
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
            period_ns,
            next_due_ns,
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
            period_ns: 0,
            next_due_ns: 0,
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
        self.lifecycle.scalar_source = Some((node, route_id, context, take));
        self
    }

    pub(super) fn with_scalar_input(
        mut self,
        node: u32,
        route_id: u32,
        context: usize,
        receive: NativeScalarReceiveFn,
    ) -> Self {
        self.lifecycle.scalar_input = Some((node, route_id, context, receive));
        self
    }
}

struct PythonNodeAlgorithm {
    node: u32,
    input_triggered: bool,
}

impl DataflowAlgorithmExecutor for PythonNodeAlgorithm {
    fn polls_pending(&self) -> bool {
        self.input_triggered
    }

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
            if self.algorithms[index].executor.polls_pending()
                && (self.algorithms[index].inputs.is_empty()
                    || self.algorithms[index].outputs.is_empty())
            {
                self.polled_algorithms.push(index);
            }
            if self.algorithms[index].period_ns != 0 {
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
            if self.pending_state(runtime, index) {
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
        self.enqueue_pending_algorithms(runtime);
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
            && algorithm.executor.polls_pending()
            && algorithm.period_ns == 0
    }

    fn enqueue_pending_algorithms(&mut self, runtime: &ClusterRuntime) {
        for position in 0..self.polled_algorithms.len() {
            let index = self.polled_algorithms[position];
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
                if algorithm.period_ns == 0 || algorithm.next_due_ns > runtime.elapsed_ns {
                    continue;
                }
                let period_ns = algorithm.period_ns;
                self.enqueue_if_ready(index);
                if let Some(algorithm) = self.algorithms.get_mut(index) {
                    while algorithm.next_due_ns <= runtime.elapsed_ns {
                        algorithm.next_due_ns = algorithm.next_due_ns.saturating_add(period_ns);
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
        while let Some(index) = self.queue.pop_front() {
            let Some(pending) = self.pending.get_mut(index) else {
                continue;
            };
            if !*pending {
                continue;
            }
            *pending = false;
            if ran_algorithms.get(index).copied() == Some(generation) {
                continue;
            }
            let Some(algorithm) = self.algorithms.get(index) else {
                continue;
            };
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
