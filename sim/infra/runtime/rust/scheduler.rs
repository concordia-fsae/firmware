use std::mem;

use super::cluster::ClusterRuntime;
use super::dataflow::{DataflowAlgorithm, DataflowEdgeKey, DataflowGraph};
use super::registry::RuntimeInterface;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SchedulerCallbackContext {
    pub elapsed_ns: u64,
    pub delta_ns: u64,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn callback_context_defaults_to_zero() {
        assert_eq!(SchedulerCallbackContext::default().elapsed_ns, 0);
        assert_eq!(SchedulerCallbackContext::default().delta_ns, 0);
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
}

#[derive(Default)]
pub(super) struct ClusterScheduler {
    graph: DataflowGraph,
    deferred_ready_edges: Vec<DataflowEdgeKey>,
    deferred_input_pending_nodes: Vec<u32>,
    ran_algorithms: Vec<u64>,
    run_generation: u64,
}

impl ClusterScheduler {
    pub(super) fn reset(&mut self) {
        self.graph.reset();
        self.deferred_ready_edges.clear();
        self.deferred_input_pending_nodes.clear();
        self.ran_algorithms.clear();
        self.run_generation = 0;
    }

    pub(super) fn mark_dirty(&mut self) {
        self.graph.dirty = true;
    }

    pub(super) fn add_initial_ready_edge(&mut self, key: DataflowEdgeKey) {
        self.graph.ready_edges.insert(key);
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
    loop {
        let ready_edges = mem::take(&mut runtime.scheduler.deferred_ready_edges);
        let input_pending_nodes = mem::take(&mut runtime.scheduler.deferred_input_pending_nodes);
        if ready_edges.is_empty() && input_pending_nodes.is_empty() {
            break;
        }
        for edge in ready_edges {
            graph.mark_edge_ready(edge);
        }
        for node in input_pending_nodes {
            graph.mark_owner_input_pending(node);
        }
        graph.run_queued_algorithms(runtime, &mut ran_algorithms, generation);
    }
    runtime.scheduler.ran_algorithms = ran_algorithms;
    runtime.scheduler.graph = graph;
}
