use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::can::{CanEvent, CanPacket};
use super::cluster::ClusterRuntime;
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor};
use super::registry::RuntimeInterfaces;
use super::scheduler;

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
    algorithms::register_runtime_reset(runtime, reset_periodic_can_sources);
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
        ),
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
