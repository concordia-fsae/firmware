use std::collections::HashMap;
use std::mem;

use super::cluster::ClusterRuntime;
use super::dataflow::{DataflowAlgorithm, DataflowEdgeKey, DataflowGraph, DataflowWait};
use super::registry::RuntimeInterface;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SchedulerCallbackContext {
    pub elapsed_ns: u64,
    pub delta_ns: u64,
}

#[cfg(test)]
mod tests {
    use std::sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        Arc,
    };

    use super::*;
    use super::super::dataflow::DataflowAlgorithmExecutor;

    struct TriggerExecutor {
        pending: Arc<AtomicBool>,
    }

    impl DataflowAlgorithmExecutor for TriggerExecutor {
        fn is_python_node(&self) -> bool {
            true
        }

        fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
            self.pending.store(true, Ordering::Relaxed);
            false
        }
    }

    struct PendingExecutor {
        pending: Arc<AtomicBool>,
        runs: Arc<AtomicUsize>,
    }

    impl DataflowAlgorithmExecutor for PendingExecutor {
        fn pending(&self, _runtime: &ClusterRuntime) -> bool {
            self.pending.load(Ordering::Relaxed)
        }

        fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
            self.runs.fetch_add(1, Ordering::Relaxed);
            self.pending.store(false, Ordering::Relaxed);
            false
        }
    }

    struct RearmingPythonExecutor {
        pending: Arc<AtomicBool>,
        runs: Arc<AtomicUsize>,
    }

    impl DataflowAlgorithmExecutor for RearmingPythonExecutor {
        fn pending(&self, _runtime: &ClusterRuntime) -> bool {
            self.pending.load(Ordering::Relaxed)
        }

        fn is_python_node(&self) -> bool {
            true
        }

        fn run(&self, _runtime: &mut ClusterRuntime) -> bool {
            self.runs.fetch_add(1, Ordering::Relaxed);
            // A buggy or re-entrant callback may leave its pending bit set.
            // The scheduler must still honor its once-per-generation guard.
            true
        }
    }

    #[test]
    fn callback_context_defaults_to_zero() {
        assert_eq!(SchedulerCallbackContext::default().elapsed_ns, 0);
        assert_eq!(SchedulerCallbackContext::default().delta_ns, 0);
    }

    #[test]
    fn dataflow_wait_is_completed_and_cancelled_by_the_scheduler() {
        let mut runtime = ClusterRuntime::default();
        let wait = runtime.begin_dataflow_wait();

        assert!(!runtime.dataflow_wait_matched(wait));
        complete_dataflow_wait(&mut runtime, wait);
        assert!(runtime.dataflow_wait_matched(wait));

        runtime.cancel_dataflow_wait(wait);
        assert!(!runtime.dataflow_wait_matched(wait));
    }

    #[test]
    fn empty_runtime_advances_by_the_requested_bounded_step() {
        let mut runtime = ClusterRuntime::default();

        assert_eq!(run_next_step(&mut runtime, 10, 3), 3);
        assert_eq!(runtime.elapsed_ns, 3);
        assert_eq!(run_next_step(&mut runtime, 2, 5), 2);
        assert_eq!(runtime.elapsed_ns, 5);
        assert_eq!(run_next_step(&mut runtime, 0, 5), 0);
    }

    #[test]
    fn pending_algorithm_is_polled_after_an_algorithm_runs_in_the_same_step() {
        let mut runtime = ClusterRuntime::default();
        runtime.add_rust_runtime_model_node(true);

        let pending = Arc::new(AtomicBool::new(false));
        let runs = Arc::new(AtomicUsize::new(0));
        assert!(add_dataflow_algorithm(
            &mut runtime,
            DataflowAlgorithm::periodic_source(
                0,
                (0, 0, 0),
                Vec::new(),
                Arc::new(TriggerExecutor {
                    pending: Arc::clone(&pending),
                }),
                1,
                1,
            ),
        ));
        assert!(add_dataflow_algorithm(
            &mut runtime,
            DataflowAlgorithm::source(
                0,
                (0, 0, 1),
                Vec::new(),
                Arc::new(PendingExecutor {
                    pending: Arc::clone(&pending),
                    runs: Arc::clone(&runs),
                }),
            ),
        ));

        assert_eq!(run_next_step(&mut runtime, 1, 1), 1);
        assert_eq!(runs.load(Ordering::Relaxed), 1);
    }

    #[test]
    fn rearming_python_algorithm_does_not_spin_scheduler() {
        let mut runtime = ClusterRuntime::default();
        runtime.add_rust_runtime_model_node(true);

        let pending = Arc::new(AtomicBool::new(true));
        let runs = Arc::new(AtomicUsize::new(0));
        assert!(add_dataflow_algorithm(
            &mut runtime,
            DataflowAlgorithm::source(
                0,
                (0, 0, 0),
                Vec::new(),
                Arc::new(RearmingPythonExecutor {
                    pending: Arc::clone(&pending),
                    runs: Arc::clone(&runs),
                }),
            ),
        ));

        assert_eq!(run_next_step(&mut runtime, 1, 1), 1);
        assert_eq!(runs.load(Ordering::Relaxed), 1);
    }
}

#[derive(Default)]
pub(super) struct ClusterScheduler {
    graph: DataflowGraph,
    deferred_ready_edges: Vec<DataflowEdgeKey>,
    deferred_input_pending_nodes: Vec<u32>,
    ran_algorithms: Vec<u64>,
    run_generation: u64,
    next_wait_id: u64,
    waits: HashMap<DataflowWait, bool>,
}

impl ClusterScheduler {
    pub(super) fn reset(&mut self) {
        self.graph.reset();
        self.deferred_ready_edges.clear();
        self.deferred_input_pending_nodes.clear();
        self.ran_algorithms.clear();
        self.run_generation = 0;
        self.next_wait_id = 0;
        self.waits.clear();
    }

    pub(super) fn mark_dirty(&mut self) {
        self.graph.dirty = true;
    }

    pub(super) fn add_initial_ready_edge(&mut self, key: DataflowEdgeKey) {
        self.graph.ready_edges.insert(key);
    }

    pub(super) fn begin_dataflow_wait(&mut self) -> DataflowWait {
        self.next_wait_id = self.next_wait_id.wrapping_add(1).max(1);
        let wait = DataflowWait(self.next_wait_id);
        assert!(self.waits.insert(wait, false).is_none());
        wait
    }

    pub(super) fn complete_dataflow_wait(&mut self, wait: DataflowWait) {
        if let Some(matched) = self.waits.get_mut(&wait) {
            *matched = true;
        }
    }

    pub(super) fn dataflow_wait_matched(&self, wait: DataflowWait) -> bool {
        self.waits.get(&wait).copied().unwrap_or(false)
    }

    pub(super) fn cancel_dataflow_wait(&mut self, wait: DataflowWait) {
        self.waits.remove(&wait);
    }

    fn begin_run(&mut self, algorithm_count: usize) -> u64 {
        self.run_generation = self.run_generation.wrapping_add(1).max(1);
        if self.ran_algorithms.len() != algorithm_count {
            self.ran_algorithms.resize(algorithm_count, 0);
        }
        if self.run_generation == 1 {
            self.ran_algorithms.fill(0);
        }
        self.run_generation
    }
}

pub(super) fn complete_dataflow_wait(runtime: &mut ClusterRuntime, wait: DataflowWait) {
    runtime.scheduler.complete_dataflow_wait(wait);
}

pub(super) fn compile_dataflow_graph(runtime: &mut ClusterRuntime) -> bool {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        rebuild_dataflow_graph(runtime);
        runtime.scheduler.graph.dirty = false;
    }))
    .is_ok()
}

pub(super) fn add_dataflow_algorithm(
    runtime: &mut ClusterRuntime,
    algorithm: DataflowAlgorithm,
) -> bool {
    if algorithm.owner_node != u32::MAX
        && runtime.nodes.get(algorithm.owner_node as usize).is_none()
    {
        return false;
    }
    runtime
        .scheduler
        .graph
        .configured_algorithms
        .push(algorithm);
    runtime.scheduler.mark_dirty();
    true
}

pub(super) fn run_next_step(
    runtime: &mut ClusterRuntime,
    remaining_ns: u64,
    max_step_ns: u64,
) -> u64 {
    if remaining_ns == 0 || max_step_ns == 0 {
        return 0;
    }

    ensure_dataflow_graph(runtime);

    let delta_ns = remaining_ns.min(max_step_ns);
    if delta_ns == 0 {
        return 0;
    }

    for node in runtime
        .nodes
        .iter_mut()
        .filter(|node| node.online && node.needs_run_step())
    {
        node.run_for(delta_ns);
    }

    runtime.elapsed_ns = runtime.elapsed_ns.saturating_add(delta_ns);
    run_dataflow_graph(runtime);
    delta_ns
}

pub(super) fn mark_dataflow_edge_ready(runtime: &mut ClusterRuntime, key: DataflowEdgeKey) {
    ensure_dataflow_graph(runtime);
    if runtime.scheduler.graph.algorithms.is_empty() {
        runtime.scheduler.deferred_ready_edges.push(key);
        return;
    }
    runtime.scheduler.graph.mark_edge_ready(key);
}

pub(super) fn mark_input_pending(runtime: &mut ClusterRuntime, node: u32) {
    let Some(cluster_node) = runtime.nodes.get_mut(node as usize) else {
        return;
    };
    cluster_node.mark_input_pending();
    if runtime.scheduler.graph.algorithms.is_empty() {
        runtime.scheduler.deferred_input_pending_nodes.push(node);
        return;
    }
    runtime.scheduler.graph.mark_owner_input_pending(node);
}

fn ensure_dataflow_graph(runtime: &mut ClusterRuntime) {
    if !runtime.scheduler.graph.dirty {
        return;
    }
    rebuild_dataflow_graph(runtime);
    runtime.scheduler.graph.dirty = false;
}

fn rebuild_algorithm_specs(runtime: &mut ClusterRuntime) {
    runtime.scheduler.graph.algorithm_specs.clear();
    for (index, node) in runtime.nodes.iter().enumerate() {
        let node_index = index as u32;
        if let Some(period_ns) = node.python_period_ns() {
            runtime
                .scheduler
                .graph
                .algorithm_specs
                .push(DataflowAlgorithm::python_periodic_node(
                    node_index,
                    (node_index, 0, 0),
                    period_ns,
                    runtime.elapsed_ns.saturating_add(period_ns),
                ));
        }
        if node.has_python_input_callback() {
            runtime
                .scheduler
                .graph
                .algorithm_specs
                .push(DataflowAlgorithm::python_input_node(
                    node_index,
                    (node_index, 0, 1),
                ));
        }
    }
    runtime
        .interfaces
        .append_algorithm_specs(&mut runtime.scheduler.graph.algorithm_specs);
    runtime
        .algorithms
        .append_algorithm_specs(&mut runtime.scheduler.graph.algorithm_specs);
    let configured = runtime.scheduler.graph.configured_algorithms.clone();
    runtime.scheduler.graph.algorithm_specs.extend(configured);
}

fn rebuild_dataflow_graph(runtime: &mut ClusterRuntime) {
    rebuild_algorithm_specs(runtime);
    let mut graph = mem::take(&mut runtime.scheduler.graph);
    graph.rebuild(runtime.nodes.len(), runtime);
    runtime.scheduler.graph = graph;
}

fn run_dataflow_graph(runtime: &mut ClusterRuntime) {
    let generation = runtime
        .scheduler
        .begin_run(runtime.scheduler.graph.algorithms.len());
    let mut graph = mem::take(&mut runtime.scheduler.graph);
    let mut ran_algorithms = mem::take(&mut runtime.scheduler.ran_algorithms);
    graph.run_ready_algorithms(runtime, &mut ran_algorithms, generation);
    let mut propagation_passes = 0usize;
    loop {
        propagation_passes += 1;
        assert!(
            propagation_passes <= graph.algorithms.len().saturating_add(1),
            "dataflow scheduler did not converge in one step ({} algorithms, queue length {})",
            graph.algorithms.len(),
            graph.queue.len(),
        );
        let ready_edges = mem::take(&mut runtime.scheduler.deferred_ready_edges);
        let input_pending_nodes = mem::take(&mut runtime.scheduler.deferred_input_pending_nodes);
        for edge in ready_edges {
            graph.mark_edge_ready(edge);
        }
        for node in input_pending_nodes {
            graph.mark_owner_input_pending(node);
        }
        // Python model callbacks can enqueue scalar output events while an
        // input algorithm is running. Poll those source algorithms again so
        // the event propagates through the Rust graph in the same scheduler
        // step instead of waiting for the next clock tick.
        graph.enqueue_pending_algorithms(runtime, &ran_algorithms, generation);
        if graph.queue.is_empty() {
            break;
        }
        graph.run_queued_algorithms(runtime, &mut ran_algorithms, generation);
    }
    runtime.scheduler.ran_algorithms = ran_algorithms;
    runtime.scheduler.graph = graph;
}
