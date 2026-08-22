// Generic periodic scalar sources owned by Rig.
//
// A scalar source is a dataflow producer. It has no knowledge of a peripheral,
// firmware backend, or model; consumers provide only the node, route, period,
// and value reader.

use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor, DataflowRuntime};
use super::scalar::{self, ScalarEvent};

pub type ScalarSourceReader = fn() -> f32;

#[derive(Clone, Copy)]
struct PeriodicScalarSource {
    node: u32,
    route_id: u32,
    period_ns: u64,
    last_emit_ns: u64,
    reader: ScalarSourceReader,
}

impl PeriodicScalarSource {
    fn due_at_ns(&self) -> u64 {
        self.last_emit_ns.saturating_add(self.period_ns)
    }

    fn has_pending_event(&self, elapsed_ns: u64) -> bool {
        elapsed_ns.saturating_sub(self.last_emit_ns) >= self.period_ns
    }

    fn emit_if_due(&mut self, elapsed_ns: u64) -> Option<ScalarEvent> {
        if !self.has_pending_event(elapsed_ns) {
            return None;
        }
        self.last_emit_ns = elapsed_ns;
        Some(ScalarEvent {
            value: (self.reader)(),
            timestamp_ns: elapsed_ns,
        })
    }
}

#[derive(Default)]
struct PeriodicScalarSources {
    sources: Vec<PeriodicScalarSource>,
}

impl PeriodicScalarSources {
    fn reset(&mut self) {
        self.sources.clear();
    }
}

static PERIODIC_SCALAR_SOURCES: LazyLock<Mutex<PeriodicScalarSources>> =
    LazyLock::new(|| Mutex::new(PeriodicScalarSources::default()));

/// Add a periodic scalar producer to any Rig dataflow runtime.
pub fn add_periodic_scalar_source(
    runtime: &mut dyn DataflowRuntime,
    node: u32,
    route_id: u32,
    period_ns: u64,
    reader: ScalarSourceReader,
) -> bool {
    if !runtime.node_exists(node) || route_id == 0 || period_ns == 0 {
        return false;
    }
    let mut sources = PERIODIC_SCALAR_SOURCES.lock().unwrap();
    if sources
        .sources
        .iter()
        .any(|source| source.node == node && source.route_id == route_id)
    {
        return true;
    }
    let source = PeriodicScalarSource {
        node,
        route_id,
        period_ns,
        last_emit_ns: runtime.elapsed_ns(),
        reader,
    };
    let source_index = sources.sources.len();
    sources.sources.push(source);
    drop(sources);

    let registered = algorithms::register_algorithm(
        runtime,
        DataflowAlgorithm::periodic_source(
            node,
            (node, 1, source_index),
            vec![scalar::edge(node, route_id)],
            Arc::new(PeriodicScalarSourceAlgorithm { source_index }),
            period_ns,
            source.due_at_ns(),
        )
        .with_runtime_reset(reset_periodic_scalar_sources)
        .with_scalar_source(node, route_id, source_index, take_periodic_scalar_events),
    );
    if !registered {
        PERIODIC_SCALAR_SOURCES.lock().unwrap().sources.pop();
    }
    registered
}

fn reset_periodic_scalar_sources() {
    PERIODIC_SCALAR_SOURCES.lock().unwrap().reset();
}

fn take_periodic_scalar_events(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    PERIODIC_SCALAR_SOURCES
        .lock()
        .unwrap()
        .sources
        .get_mut(context)
        .and_then(|source| source.emit_if_due(elapsed_ns))
        .into_iter()
        .collect()
}

struct PeriodicScalarSourceAlgorithm {
    source_index: usize,
}

impl DataflowAlgorithmExecutor for PeriodicScalarSourceAlgorithm {
    fn pending(&self, runtime: &dyn DataflowRuntime) -> bool {
        PERIODIC_SCALAR_SOURCES
            .lock()
            .unwrap()
            .sources
            .get(self.source_index)
            .is_some_and(|source| {
                runtime.node_online(source.node) && source.has_pending_event(runtime.elapsed_ns())
            })
    }

    fn run(&self, runtime: &mut dyn DataflowRuntime) -> bool {
        let event = take_periodic_scalar_events(self.source_index, runtime.elapsed_ns())
            .into_iter()
            .next();
        let Some(event) = event else {
            return false;
        };
        let sources = PERIODIC_SCALAR_SOURCES.lock().unwrap();
        let Some(source) = sources.sources.get(self.source_index) else {
            return false;
        };
        scalar::route_native_event(runtime, source.node, source.route_id, event);
        true
    }
}

#[cfg(test)]
mod tests {
    use super::super::runtime::{NoBackend, RigRuntime};
    use super::*;

    fn read_value() -> f32 {
        12.5
    }

    #[test]
    fn periodic_scalar_sources_are_generic_rig_algorithms() {
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);

        assert!(add_periodic_scalar_source(
            &mut runtime,
            node,
            7,
            10,
            read_value,
        ));
        assert_eq!(runtime.run_for_ns(9, 100), 9);
        assert!(runtime.latest_scalar_event(node, 7).is_none());
        assert_eq!(runtime.run_for_ns(1, 100), 1);
        assert_eq!(runtime.latest_scalar_event(node, 7).unwrap().value, 12.5);
    }

    #[test]
    fn periodic_scalar_sources_reject_zero_periods() {
        let mut runtime = RigRuntime::<NoBackend>::default();
        let node = runtime.add_rust_runtime_model_node(true);
        assert!(!add_periodic_scalar_source(
            &mut runtime,
            node,
            7,
            0,
            read_value,
        ));
    }
}
