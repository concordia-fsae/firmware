use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::cluster::{self, ClusterRuntime};
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor};
use super::registry::RuntimeInterfaces;
use super::scalar::{self, ScalarEvent};

static BATTERY_SOURCES: LazyLock<Mutex<Vec<BatterySourceModel>>> =
    LazyLock::new(|| Mutex::new(Vec::new()));

#[derive(Clone, Copy)]
pub struct BatterySourceModel {
    node: u32,
    voltage_route_id: u32,
    open_circuit_voltage: f32,
    internal_resistance_ohms: f32,
    capacity_amp_hours: f32,
    total_current_amps: f32,
    output_voltage: f32,
    pending_voltage: bool,
}

impl BatterySourceModel {
    pub fn new(
        node: u32,
        voltage_route_id: u32,
        voltage: f32,
        internal_resistance_ohms: f32,
        capacity_amp_hours: f32,
    ) -> Self {
        Self {
            node,
            voltage_route_id,
            open_circuit_voltage: voltage,
            internal_resistance_ohms,
            capacity_amp_hours,
            total_current_amps: 0.0,
            output_voltage: voltage,
            pending_voltage: true,
        }
    }

    pub fn node(&self) -> u32 {
        self.node
    }

    pub fn voltage_output_key(&self) -> (u32, u32) {
        (self.node, self.voltage_route_id)
    }

    pub fn reset(&mut self) {
        self.total_current_amps = 0.0;
        self.output_voltage = self.open_circuit_voltage;
        self.pending_voltage = true;
    }

    pub fn config_matches(&self, node: u32, voltage_route_id: u32) -> bool {
        self.node == node && self.voltage_route_id == voltage_route_id
    }

    pub fn config_equals(
        &self,
        node: u32,
        voltage_route_id: u32,
        voltage: f32,
        internal_resistance_ohms: f32,
        capacity_amp_hours: f32,
    ) -> bool {
        self.config_matches(node, voltage_route_id)
            && self.open_circuit_voltage == voltage
            && self.internal_resistance_ohms == internal_resistance_ohms
            && self.capacity_amp_hours == capacity_amp_hours
    }

    pub fn has_pending_voltage(&self) -> bool {
        self.pending_voltage
    }

    pub fn total_current_changed(&self, current_amps: f32) -> bool {
        self.total_current_amps != current_amps.max(0.0)
    }

    pub fn take_voltage_event(&mut self, elapsed_ns: u64) -> Option<ScalarEvent> {
        if !self.pending_voltage {
            return None;
        }
        self.pending_voltage = false;
        Some(ScalarEvent {
            value: self.output_voltage.max(0.0),
            timestamp_ns: elapsed_ns,
        })
    }

    pub fn update_load_current(&mut self, current_amps: f32) -> bool {
        self.total_current_amps = current_amps.max(0.0);
        self.output_voltage = (self.open_circuit_voltage
            - self.total_current_amps * self.internal_resistance_ohms)
            .max(0.0);
        if self.capacity_amp_hours.is_finite() {
            // Capacity state is intentionally reserved for the next-order battery model.
        }
        self.pending_voltage = true;
        true
    }
}

fn reset_runtime() {
    BATTERY_SOURCES.lock().unwrap().clear();
}

fn reset_source(context: usize, _elapsed_ns: u64) {
    if let Some(source) = BATTERY_SOURCES.lock().unwrap().get_mut(context) {
        source.reset();
    }
}

fn take_source_events(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    BATTERY_SOURCES
        .lock()
        .unwrap()
        .get_mut(context)
        .and_then(|source| source.take_voltage_event(elapsed_ns))
        .into_iter()
        .collect()
}

struct BatterySourceAlgorithm {
    source_index: usize,
}

impl DataflowAlgorithmExecutor for BatterySourceAlgorithm {
    fn polls_pending(&self) -> bool {
        true
    }

    fn pending(&self, runtime: &ClusterRuntime) -> bool {
        battery_source_pending(runtime, self.source_index)
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        run_battery_source(runtime, self.source_index)
    }
}

fn run_battery_source(runtime: &mut ClusterRuntime, context: usize) -> bool {
    let current_amps = {
        let Some(source) = BATTERY_SOURCES.lock().unwrap().get(context).copied() else {
            return false;
        };
        runtime
            .scalar_state_input_values(source.node())
            .into_iter()
            .map(|(_, current)| current.max(0.0))
            .sum()
    };

    let mut sources = BATTERY_SOURCES.lock().unwrap();
    let Some(source) = sources.get_mut(context) else {
        return false;
    };
    source.update_load_current(current_amps);
    let source_node = source.node();
    let route_id = source.voltage_output_key().1;
    let event = source.take_voltage_event(runtime.elapsed_ns);
    drop(sources);

    if let Some(event) = event {
        scalar::route_native_event(runtime, source_node, route_id, event);
        return true;
    }
    false
}

fn battery_source_pending(runtime: &ClusterRuntime, context: usize) -> bool {
    let Some(source) = BATTERY_SOURCES.lock().unwrap().get(context).copied() else {
        return false;
    };
    if source.has_pending_voltage() {
        return true;
    }
    let current_amps: f32 = runtime
        .scalar_state_input_values(source.node())
        .into_iter()
        .map(|(_, current)| current.max(0.0))
        .sum();
    source.total_current_changed(current_amps)
}

fn register_source(runtime: &mut ClusterRuntime, source: BatterySourceModel) -> bool {
    if !runtime.node_exists(source.node()) {
        return false;
    }
    let mut sources = BATTERY_SOURCES.lock().unwrap();
    if let Some(index) = sources
        .iter()
        .position(|existing| existing.config_matches(source.node(), source.voltage_output_key().1))
    {
        if sources[index].config_equals(
            source.node(),
            source.voltage_output_key().1,
            source.open_circuit_voltage,
            source.internal_resistance_ohms,
            source.capacity_amp_hours,
        ) {
            return true;
        }
        sources[index] = source;
        drop(sources);
        runtime.scheduler.mark_dirty();
        return true;
    }
    sources.push(source);
    let context = sources.len() - 1;
    let node = source.node();
    let route_id = source.voltage_output_key().1;
    drop(sources);

    algorithms::register_runtime_reset(runtime, reset_runtime);
    algorithms::register_node_reset(runtime, node, context, reset_source);
    algorithms::register_native_scalar_source(runtime, node, route_id, context, take_source_events);
    algorithms::register_algorithm(
        runtime,
        DataflowAlgorithm::source(
            node,
            (node, 5, context),
            vec![RuntimeInterfaces::scalar_edge(node, route_id)],
            Arc::new(BatterySourceAlgorithm {
                source_index: context,
            }),
        ),
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_battery_source(
    node: u32,
    voltage_route_id: u32,
    voltage: f32,
    internal_resistance_ohms: f32,
    capacity_amp_hours: f32,
) -> bool {
    if !voltage.is_finite()
        || voltage < 0.0
        || !internal_resistance_ohms.is_finite()
        || internal_resistance_ohms < 0.0
        || capacity_amp_hours <= 0.0
    {
        return false;
    }
    cluster::with_runtime(|runtime| {
        register_source(
            runtime,
            BatterySourceModel::new(
                node,
                voltage_route_id,
                voltage,
                internal_resistance_ohms,
                capacity_amp_hours,
            ),
        )
    })
}
