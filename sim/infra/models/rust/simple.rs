use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::can::{CanEvent, CanPacket};
use super::cluster::ClusterRuntime;
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor};
use super::registry::RuntimeInterfaces;
use super::scheduler;
use super::scalar::{self, ScalarEvent};

#[derive(Clone, Copy)]
pub struct PeriodicCanSource {
    node: u32,
    bus: u8,
    period_ns: u64,
    last_emit_ns: u64,
    packet: CanPacket,
}

impl PeriodicCanSource {
    pub fn new(node: u32, bus: u8, period_ns: u64, packet: CanPacket) -> Self {
        Self {
            node,
            bus,
            period_ns,
            last_emit_ns: 0,
            packet,
        }
    }

    pub fn node(&self) -> u32 {
        self.node
    }

    pub fn bus(&self) -> u8 {
        self.bus
    }

    pub fn period_ns(&self) -> u64 {
        self.period_ns
    }

    pub fn due_at_ns(&self) -> u64 {
        self.last_emit_ns.saturating_add(self.period_ns)
    }

    pub fn has_pending_event(&self, elapsed_ns: u64) -> bool {
        elapsed_ns.saturating_sub(self.last_emit_ns) >= self.period_ns
    }

    pub fn update_packet(&mut self, packet: CanPacket) {
        self.packet = packet;
    }

    pub fn emit_if_due(&mut self, elapsed_ns: u64) -> Option<CanEvent> {
        if elapsed_ns.saturating_sub(self.last_emit_ns) < self.period_ns {
            return None;
        }
        self.last_emit_ns = elapsed_ns;
        Some(CanEvent {
            bus: self.bus,
            timestamp_ns: elapsed_ns,
            packet: self.packet,
        })
    }
}

#[derive(Default)]
struct PeriodicCanSources {
    sources: Vec<PeriodicCanSource>,
}

impl PeriodicCanSources {
    fn reset(&mut self) {
        self.sources.clear();
    }
}

static PERIODIC_CAN_SOURCES: LazyLock<Mutex<PeriodicCanSources>> =
    LazyLock::new(|| Mutex::new(PeriodicCanSources::default()));

pub(super) type ScalarSourceReader = fn() -> f32;

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

pub(super) fn add_periodic_scalar_source(
    runtime: &mut ClusterRuntime,
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
        last_emit_ns: runtime.elapsed_ns,
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
            vec![RuntimeInterfaces::scalar_edge(node, route_id)],
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
    fn polls_pending(&self) -> bool {
        true
    }

    fn pending(&self, runtime: &ClusterRuntime) -> bool {
        PERIODIC_SCALAR_SOURCES
            .lock()
            .unwrap()
            .sources
            .get(self.source_index)
            .is_some_and(|source| {
                runtime.node_online(source.node) && source.has_pending_event(runtime.elapsed_ns)
            })
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        let event = take_periodic_scalar_events(self.source_index, runtime.elapsed_ns)
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

pub(super) fn add_periodic_can_source(
    runtime: &mut ClusterRuntime,
    node: u32,
    bus: u8,
    period_ns: u64,
    packet: CanPacket,
) -> u32 {
    if !runtime.node_exists(node) || period_ns == 0 {
        return u32::MAX;
    }
    let mut sources = PERIODIC_CAN_SOURCES.lock().unwrap();
    let handle = sources.sources.len();
    sources
        .sources
        .push(PeriodicCanSource::new(node, bus, period_ns, packet));
    let source = sources.sources[handle];
    drop(sources);

    if !algorithms::register_algorithm(
        runtime,
        DataflowAlgorithm::periodic_source(
            source.node(),
            (source.node(), 0, handle),
            vec![RuntimeInterfaces::can_edge(source.node(), source.bus())],
            Arc::new(PeriodicCanSourceAlgorithm {
                source_index: handle,
            }),
            source.period_ns(),
            source.due_at_ns(),
        )
        .with_runtime_reset(reset_periodic_can_sources),
    ) {
        PERIODIC_CAN_SOURCES.lock().unwrap().sources.pop();
        return u32::MAX;
    }
    handle as u32
}

pub(super) fn update_periodic_can_source(
    _runtime: &mut ClusterRuntime,
    handle: u32,
    packet: CanPacket,
) -> bool {
    let mut sources = PERIODIC_CAN_SOURCES.lock().unwrap();
    let Some(source) = sources.sources.get_mut(handle as usize) else {
        return false;
    };
    source.update_packet(packet);
    true
}

fn reset_periodic_can_sources() {
    PERIODIC_CAN_SOURCES.lock().unwrap().reset();
}

struct PeriodicCanSourceAlgorithm {
    source_index: usize,
}

impl DataflowAlgorithmExecutor for PeriodicCanSourceAlgorithm {
    fn polls_pending(&self) -> bool {
        true
    }

    fn pending(&self, runtime: &ClusterRuntime) -> bool {
        let sources = PERIODIC_CAN_SOURCES.lock().unwrap();
        let Some(source) = sources.sources.get(self.source_index) else {
            return false;
        };
        runtime.node_online(source.node()) && source.has_pending_event(runtime.elapsed_ns)
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        let mut sources = PERIODIC_CAN_SOURCES.lock().unwrap();
        let Some(source) = sources.sources.get_mut(self.source_index) else {
            return false;
        };
        let source_node = source.node();
        let source_bus = source.bus();
        if !runtime.node_online(source_node) {
            return false;
        }
        let Some(event) = source.emit_if_due(runtime.elapsed_ns) else {
            return false;
        };
        drop(sources);

        let input_pending_nodes =
            runtime
                .interfaces
                .can
                .route_event(source_node, source_bus, event);
        for sink_node in input_pending_nodes {
            scheduler::mark_input_pending(runtime, sink_node);
        }
        true
    }
}
