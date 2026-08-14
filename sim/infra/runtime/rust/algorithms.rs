use std::collections::HashSet;

use super::cluster::ClusterRuntime;
use super::dataflow::{
    DataflowAlgorithm, NativeScalarReceiveFn, NativeScalarTakeFn, NodeResetFn, RuntimeResetFn,
};
use super::scalar::ScalarEvent;

#[derive(Clone, Copy)]
struct NodeReset {
    node: u32,
    context: usize,
    reset: NodeResetFn,
}

#[derive(Clone, Copy)]
struct NativeScalarSource {
    node: u32,
    route_id: u32,
    context: usize,
    take: NativeScalarTakeFn,
}

#[derive(Clone, Copy)]
struct NativeScalarInput {
    node: u32,
    route_id: u32,
    context: usize,
    receive: NativeScalarReceiveFn,
}

#[derive(Default)]
pub(super) struct RuntimeAlgorithms {
    algorithms: Vec<DataflowAlgorithm>,
    native_scalar_sources: Vec<NativeScalarSource>,
    native_scalar_inputs: Vec<NativeScalarInput>,
    node_resets: Vec<NodeReset>,
    runtime_resets: Vec<RuntimeResetFn>,
}

impl RuntimeAlgorithms {
    pub(super) fn reset(&mut self) {
        for reset in &self.runtime_resets {
            reset();
        }
        self.algorithms.clear();
        self.native_scalar_sources.clear();
        self.native_scalar_inputs.clear();
        self.node_resets.clear();
        self.runtime_resets.clear();
    }

    pub(super) fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        specs.extend(self.algorithms.iter().cloned());
    }

    pub(super) fn native_scalar_source_registered(&self, source_node: u32, route_id: u32) -> bool {
        self.native_scalar_sources
            .iter()
            .any(|source| source.node == source_node && source.route_id == route_id)
    }

    pub(super) fn native_scalar_source_keys(&self) -> HashSet<(u32, u32)> {
        self.native_scalar_sources
            .iter()
            .map(|source| (source.node, source.route_id))
            .collect()
    }

    pub(super) fn take_native_scalar_events(
        &mut self,
        source_node: u32,
        route_id: u32,
        elapsed_ns: u64,
    ) -> Vec<ScalarEvent> {
        let mut events = Vec::new();
        for source in self
            .native_scalar_sources
            .iter()
            .filter(|source| source.node == source_node && source.route_id == route_id)
        {
            events.extend((source.take)(source.context, elapsed_ns));
        }
        events
    }

    pub(super) fn reset_node_models(&self, node: u32, elapsed_ns: u64) {
        for reset in self.node_resets.iter().filter(|reset| reset.node == node) {
            (reset.reset)(reset.context, elapsed_ns);
        }
    }

    pub(super) fn native_scalar_input(
        &self,
        node: u32,
        route_id: u32,
    ) -> Option<(usize, NativeScalarReceiveFn)> {
        self.native_scalar_inputs
            .iter()
            .find(|input| input.node == node && input.route_id == route_id)
            .map(|input| (input.context, input.receive))
    }
}

pub(super) fn register_runtime_reset(runtime: &mut ClusterRuntime, reset: RuntimeResetFn) {
    if runtime
        .algorithms
        .runtime_resets
        .iter()
        .any(|existing| *existing as usize == reset as usize)
    {
        return;
    }
    runtime.algorithms.runtime_resets.push(reset);
}

pub(super) fn register_node_reset(
    runtime: &mut ClusterRuntime,
    node: u32,
    context: usize,
    reset: NodeResetFn,
) -> bool {
    if !runtime.node_exists(node) {
        return false;
    }
    if runtime.algorithms.node_resets.iter().any(|existing| {
        existing.node == node
            && existing.context == context
            && existing.reset as usize == reset as usize
    }) {
        return true;
    }
    runtime.algorithms.node_resets.push(NodeReset {
        node,
        context,
        reset,
    });
    true
}

pub(super) fn register_algorithm(
    runtime: &mut ClusterRuntime,
    algorithm: DataflowAlgorithm,
) -> bool {
    if algorithm.owner_node != u32::MAX && !runtime.node_exists(algorithm.owner_node) {
        return false;
    }
    if !register_algorithm_lifecycle(runtime, &algorithm) {
        return false;
    }
    runtime.algorithms.algorithms.push(algorithm);
    runtime.scheduler.mark_dirty();
    true
}

pub(super) fn replace_algorithm(
    runtime: &mut ClusterRuntime,
    algorithm: DataflowAlgorithm,
) -> bool {
    if algorithm.owner_node != u32::MAX && !runtime.node_exists(algorithm.owner_node) {
        return false;
    }
    if !register_algorithm_lifecycle(runtime, &algorithm) {
        return false;
    }
    runtime.algorithms.algorithms.retain(|existing| {
        existing.owner_node != algorithm.owner_node || existing.sort_key != algorithm.sort_key
    });
    runtime.algorithms.algorithms.push(algorithm);
    runtime.scheduler.mark_dirty();
    true
}

fn register_algorithm_lifecycle(
    runtime: &mut ClusterRuntime,
    algorithm: &DataflowAlgorithm,
) -> bool {
    let lifecycle = algorithm.lifecycle;
    if let Some(reset) = lifecycle.runtime_reset {
        register_runtime_reset(runtime, reset);
    }
    if let Some((node, context, reset)) = lifecycle.node_reset {
        if !register_node_reset(runtime, node, context, reset) {
            return false;
        }
    }
    if let Some((node, route_id, context, take)) = lifecycle.scalar_source {
        if !register_native_scalar_source(runtime, node, route_id, context, take) {
            return false;
        }
    }
    if let Some((node, route_id, context, receive)) = lifecycle.scalar_input {
        if !register_native_scalar_input(runtime, node, route_id, context, receive) {
            return false;
        }
    }
    true
}

pub(super) fn register_native_scalar_source(
    runtime: &mut ClusterRuntime,
    node: u32,
    route_id: u32,
    context: usize,
    take: NativeScalarTakeFn,
) -> bool {
    if !runtime.node_exists(node) {
        return false;
    }
    if runtime
        .algorithms
        .native_scalar_sources
        .iter()
        .any(|source| {
            source.node == node && source.route_id == route_id && source.context == context
        })
    {
        return true;
    }
    runtime
        .algorithms
        .native_scalar_sources
        .push(NativeScalarSource {
            node,
            route_id,
            context,
            take,
        });
    true
}

pub(super) fn register_native_scalar_input(
    runtime: &mut ClusterRuntime,
    node: u32,
    route_id: u32,
    context: usize,
    receive: NativeScalarReceiveFn,
) -> bool {
    if !runtime.node_exists(node) {
        return false;
    }
    if runtime.algorithms.native_scalar_inputs.iter().any(|input| {
        input.node == node && input.route_id == route_id && input.context == context
    }) {
        return true;
    }
    runtime.algorithms.native_scalar_inputs.push(NativeScalarInput {
        node,
        route_id,
        context,
        receive,
    });
    true
}
