use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::cluster::{self, ClusterRuntime};
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor};
use super::registry::RuntimeInterfaces;
use super::scalar::{self, ScalarEvent};

const BATTERY_UPDATE_PERIOD_NS: u64 = 10_000_000;

static BATTERY_SOURCES: LazyLock<Mutex<Vec<BatterySourceModel>>> =
    LazyLock::new(|| Mutex::new(Vec::new()));

#[derive(Clone, Copy)]
pub struct BatterySourceModel {
    node: u32,
    voltage_route_id: u32,
    contactor_state_route_id: u32,
    open_circuit_voltage: f32,
    internal_resistance_ohms: f32,
    capacity_amp_hours: f32,
    rc1_resistance_ohms: f32,
    rc1_capacitance_farads: f32,
    rc2_resistance_ohms: f32,
    rc2_capacitance_farads: f32,
    total_current_amps: f32,
    output_voltage: f32,
    rc1_voltage: f32,
    rc2_voltage: f32,
    last_update_ns: u64,
    pending_voltage: bool,
}

impl BatterySourceModel {
    pub fn new(
        node: u32,
        voltage_route_id: u32,
        contactor_state_route_id: u32,
        voltage: f32,
        internal_resistance_ohms: f32,
        capacity_amp_hours: f32,
        rc1_resistance_ohms: f32,
        rc1_capacitance_farads: f32,
        rc2_resistance_ohms: f32,
        rc2_capacitance_farads: f32,
    ) -> Self {
        Self {
            node,
            voltage_route_id,
            contactor_state_route_id,
            open_circuit_voltage: voltage,
            internal_resistance_ohms,
            capacity_amp_hours,
            rc1_resistance_ohms,
            rc1_capacitance_farads,
            rc2_resistance_ohms,
            rc2_capacitance_farads,
            total_current_amps: 0.0,
            output_voltage: voltage,
            rc1_voltage: 0.0,
            rc2_voltage: 0.0,
            last_update_ns: 0,
            pending_voltage: true,
        }
    }

    pub fn node(&self) -> u32 {
        self.node
    }

    pub fn voltage_output_key(&self) -> (u32, u32) {
        (self.node, self.voltage_route_id)
    }

    pub fn reset(&mut self, elapsed_ns: u64) {
        self.total_current_amps = 0.0;
        self.output_voltage = self.open_circuit_voltage;
        self.rc1_voltage = 0.0;
        self.rc2_voltage = 0.0;
        self.last_update_ns = elapsed_ns;
        self.pending_voltage = true;
    }

    pub fn config_matches(&self, node: u32, voltage_route_id: u32) -> bool {
        self.node == node && self.voltage_route_id == voltage_route_id
    }

    pub fn config_equals(
        &self,
        node: u32,
        voltage_route_id: u32,
        contactor_state_route_id: u32,
        voltage: f32,
        internal_resistance_ohms: f32,
        capacity_amp_hours: f32,
        rc1_resistance_ohms: f32,
        rc1_capacitance_farads: f32,
        rc2_resistance_ohms: f32,
        rc2_capacitance_farads: f32,
    ) -> bool {
        self.config_matches(node, voltage_route_id)
            && self.open_circuit_voltage == voltage
            && self.contactor_state_route_id == contactor_state_route_id
            && self.internal_resistance_ohms == internal_resistance_ohms
            && self.capacity_amp_hours == capacity_amp_hours
            && self.rc1_resistance_ohms == rc1_resistance_ohms
            && self.rc1_capacitance_farads == rc1_capacitance_farads
            && self.rc2_resistance_ohms == rc2_resistance_ohms
            && self.rc2_capacitance_farads == rc2_capacitance_farads
    }

    pub fn take_voltage_event(&mut self, elapsed_ns: u64, enabled: bool) -> Option<ScalarEvent> {
        if !self.pending_voltage {
            return None;
        }
        self.pending_voltage = false;
        Some(ScalarEvent {
            value: if enabled { self.output_voltage.max(0.0) } else { 0.0 },
            timestamp_ns: elapsed_ns,
        })
    }

    pub fn update_load_current(&mut self, current_amps: f32, elapsed_ns: u64) -> bool {
        let current_amps = current_amps.max(0.0);
        let elapsed_ns = elapsed_ns.saturating_sub(self.last_update_ns);
        let dt_seconds = elapsed_ns as f32 / 1_000_000_000.0;
        self.rc1_voltage = rc_branch_voltage(
            self.rc1_voltage,
            current_amps,
            self.rc1_resistance_ohms,
            self.rc1_capacitance_farads,
            dt_seconds,
        );
        self.rc2_voltage = rc_branch_voltage(
            self.rc2_voltage,
            current_amps,
            self.rc2_resistance_ohms,
            self.rc2_capacitance_farads,
            dt_seconds,
        );
        self.total_current_amps = current_amps;
        self.output_voltage = (self.open_circuit_voltage
            - self.total_current_amps * self.internal_resistance_ohms
            - self.rc1_voltage
            - self.rc2_voltage)
            .max(0.0);
        self.last_update_ns = self.last_update_ns.saturating_add(elapsed_ns);
        if self.capacity_amp_hours.is_finite() {
            // Capacity state is intentionally reserved for the next-order battery model.
        }
        self.pending_voltage = true;
        true
    }
}

fn rc_branch_voltage(
    previous_voltage: f32,
    current_amps: f32,
    resistance_ohms: f32,
    capacitance_farads: f32,
    dt_seconds: f32,
) -> f32 {
    if resistance_ohms <= 0.0 || capacitance_farads <= 0.0 || dt_seconds <= 0.0 {
        return previous_voltage;
    }
    let equilibrium_voltage = current_amps * resistance_ohms;
    let decay = (-dt_seconds / (resistance_ohms * capacitance_farads)).exp();
    equilibrium_voltage + (previous_voltage - equilibrium_voltage) * decay
}

fn reset_runtime() {
    BATTERY_SOURCES.lock().unwrap().clear();
}

fn reset_source(context: usize, elapsed_ns: u64) {
    if let Some(source) = BATTERY_SOURCES.lock().unwrap().get_mut(context) {
        source.reset(elapsed_ns);
    }
}

fn take_source_events(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    BATTERY_SOURCES
        .lock()
        .unwrap()
        .get_mut(context)
        .and_then(|source| source.take_voltage_event(elapsed_ns, true))
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

    fn pending(&self, _runtime: &ClusterRuntime) -> bool {
        BATTERY_SOURCES
            .lock()
            .unwrap()
            .get(self.source_index)
            .is_some_and(|source| source.pending_voltage)
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        run_battery_source(runtime, self.source_index)
    }
}

fn run_battery_source(runtime: &mut ClusterRuntime, context: usize) -> bool {
    let (current_amps, enabled) = {
        let Some(source) = BATTERY_SOURCES.lock().unwrap().get(context).copied() else {
            return false;
        };
        let enabled = source.contactor_state_route_id == 0
            || runtime
                .scalar_state_input_values(source.node())
                .into_iter()
                .find(|(route_id, _)| *route_id == source.contactor_state_route_id)
                .is_some_and(|(_, state)| state > 0.5);
        let current = runtime
            .scalar_state_input_values(source.node())
            .into_iter()
            .filter(|(route_id, _)| *route_id != source.contactor_state_route_id)
            .map(|(_, current)| current.max(0.0))
            .sum();
        (if enabled { current } else { 0.0 }, enabled)
    };

    let mut sources = BATTERY_SOURCES.lock().unwrap();
    let Some(source) = sources.get_mut(context) else {
        return false;
    };
    source.update_load_current(current_amps, runtime.elapsed_ns);
    let source_node = source.node();
    let route_id = source.voltage_output_key().1;
    let event = source.take_voltage_event(runtime.elapsed_ns, enabled);
    drop(sources);

    if let Some(event) = event {
        scalar::route_native_event(runtime, source_node, route_id, event);
        return true;
    }
    false
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
            source.contactor_state_route_id,
            source.open_circuit_voltage,
            source.internal_resistance_ohms,
            source.capacity_amp_hours,
            source.rc1_resistance_ohms,
            source.rc1_capacitance_farads,
            source.rc2_resistance_ohms,
            source.rc2_capacitance_farads,
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

    let algorithm = DataflowAlgorithm::periodic_source(
            node,
            (node, 5, context),
            vec![RuntimeInterfaces::scalar_edge(node, route_id)],
            Arc::new(BatterySourceAlgorithm {
                source_index: context,
            }),
            BATTERY_UPDATE_PERIOD_NS,
            runtime.elapsed_ns.saturating_add(BATTERY_UPDATE_PERIOD_NS),
        )
        .with_runtime_reset(reset_runtime)
        .with_node_reset(node, context, reset_source)
        .with_scalar_source(node, route_id, context, take_source_events);
    algorithms::register_algorithm(runtime, algorithm)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_register_battery_source(
    node: u32,
    voltage_route_id: u32,
    contactor_state_route_id: u32,
    voltage: f32,
    internal_resistance_ohms: f32,
    capacity_amp_hours: f32,
    rc1_resistance_ohms: f32,
    rc1_capacitance_farads: f32,
    rc2_resistance_ohms: f32,
    rc2_capacitance_farads: f32,
) -> bool {
    if !voltage.is_finite()
        || voltage < 0.0
        || !internal_resistance_ohms.is_finite()
        || internal_resistance_ohms < 0.0
        || capacity_amp_hours <= 0.0
        || !rc1_resistance_ohms.is_finite()
        || rc1_resistance_ohms < 0.0
        || !rc1_capacitance_farads.is_finite()
        || rc1_capacitance_farads < 0.0
        || !rc2_resistance_ohms.is_finite()
        || rc2_resistance_ohms < 0.0
        || !rc2_capacitance_farads.is_finite()
        || rc2_capacitance_farads < 0.0
    {
        return false;
    }
    cluster::with_runtime(|runtime| {
        register_source(
            runtime,
            BatterySourceModel::new(
                node,
                voltage_route_id,
                contactor_state_route_id,
                voltage,
                internal_resistance_ohms,
                capacity_amp_hours,
                rc1_resistance_ohms,
                rc1_capacitance_farads,
                rc2_resistance_ohms,
                rc2_capacitance_farads,
            ),
        )
    })
}
