use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::cluster::{self, ClusterRuntime};
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor};
use super::registry::RuntimeInterfaces;
use super::scalar::{self, ScalarEvent};

static DC_LOADS: LazyLock<Mutex<Vec<DcLoadModel>>> = LazyLock::new(|| Mutex::new(Vec::new()));

#[derive(Clone, Copy)]
pub struct DcLoadModel {
    node: u32,
    voltage_node: u32,
    voltage_route_id: u32,
    current_route_id: u32,
    resistance_ohms: f32,
    inductance_henrys: f32,
    capacitance_farads: f32,
    scheduler_period_ns: u64,
    input_voltage: f32,
    output_current: f32,
    inductor_current: f32,
    previous_voltage: f32,
    last_update_ns: u64,
    voltage_dirty: bool,
    pending_current: bool,
}

impl DcLoadModel {
    pub fn new(
        node: u32,
        voltage_route_id: u32,
        current_route_id: u32,
        resistance_ohms: f32,
        inductance_henrys: f32,
        capacitance_farads: f32,
        scheduler_period_ns: u64,
        elapsed_ns: u64,
    ) -> Self {
        Self {
            node,
            voltage_node: node,
            voltage_route_id,
            current_route_id,
            resistance_ohms,
            inductance_henrys,
            capacitance_farads,
            scheduler_period_ns,
            input_voltage: 0.0,
            output_current: 0.0,
            inductor_current: 0.0,
            previous_voltage: 0.0,
            last_update_ns: elapsed_ns,
            voltage_dirty: false,
            pending_current: false,
        }
    }

    pub fn node(&self) -> u32 {
        self.node
    }

    pub fn voltage_input_key(&self) -> (u32, u32) {
        (self.voltage_node, self.voltage_route_id)
    }

    pub fn current_output_key(&self) -> (u32, u32) {
        (self.node, self.current_route_id)
    }

    pub fn reset(&mut self, elapsed_ns: u64) {
        self.input_voltage = 0.0;
        self.output_current = 0.0;
        self.inductor_current = 0.0;
        self.previous_voltage = 0.0;
        self.last_update_ns = elapsed_ns;
        self.voltage_dirty = false;
        self.pending_current = false;
    }

    pub fn output_matches(&self, node: u32, current_route_id: u32) -> bool {
        self.node == node && self.current_route_id == current_route_id
    }

    pub fn config_equals(
        &self,
        node: u32,
        current_route_id: u32,
        resistance_ohms: f32,
        inductance_henrys: f32,
        capacitance_farads: f32,
        scheduler_period_ns: u64,
    ) -> bool {
        self.output_matches(node, current_route_id)
            && self.resistance_ohms == resistance_ohms
            && self.inductance_henrys == inductance_henrys
            && self.capacitance_farads == capacitance_farads
            && self.scheduler_period_ns == scheduler_period_ns
    }

    pub fn scheduler_period_ns(&self) -> u64 {
        self.scheduler_period_ns
    }

    pub fn update_voltage_event(&mut self, event: ScalarEvent) {
        self.input_voltage = event.value.max(0.0);
        self.voltage_dirty = true;
    }

    pub fn has_pending_current(&self) -> bool {
        self.pending_current
    }

    pub fn has_voltage_update(&self) -> bool {
        self.voltage_dirty
    }

    pub fn run_until(&mut self, elapsed_ns: u64) {
        let elapsed_since_update_ns = elapsed_ns.saturating_sub(self.last_update_ns);
        if self.scheduler_period_ns == 0 {
            if !self.voltage_dirty {
                return;
            }
        } else if elapsed_since_update_ns < self.scheduler_period_ns {
            return;
        }
        let dt_seconds = elapsed_since_update_ns as f32 / 1_000_000_000.0;
        if dt_seconds <= 0.0 && component_present(self.capacitance_farads) {
            return;
        }

        let mut current = 0.0;
        if component_present(self.resistance_ohms) {
            current += self.input_voltage / self.resistance_ohms;
        }
        if component_present(self.inductance_henrys) {
            self.inductor_current += (self.input_voltage / self.inductance_henrys) * dt_seconds;
            current += self.inductor_current;
        }
        if component_present(self.capacitance_farads) {
            current += self.capacitance_farads
                * ((self.input_voltage - self.previous_voltage) / dt_seconds);
        }
        self.output_current = current;
        self.previous_voltage = self.input_voltage;
        self.last_update_ns = elapsed_ns;
        self.voltage_dirty = false;
        self.pending_current = true;
    }

    pub fn take_current_event(&mut self, elapsed_ns: u64) -> Option<ScalarEvent> {
        if !self.pending_current {
            return None;
        }
        self.pending_current = false;
        Some(ScalarEvent {
            value: self.output_current,
            timestamp_ns: elapsed_ns,
        })
    }
}

fn component_present(value: f32) -> bool {
    value.is_finite() && value > 0.0
}

fn reset_runtime() {
    DC_LOADS.lock().unwrap().clear();
}

fn reset_load(context: usize, elapsed_ns: u64) {
    if let Some(load) = DC_LOADS.lock().unwrap().get_mut(context) {
        load.reset(elapsed_ns);
    }
}

fn take_load_events(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    DC_LOADS
        .lock()
        .unwrap()
        .get_mut(context)
        .and_then(|load| load.take_current_event(elapsed_ns))
        .into_iter()
        .collect()
}

fn receive_load_voltage(context: usize, event: ScalarEvent) -> bool {
    let mut loads = DC_LOADS.lock().unwrap();
    let Some(load) = loads.get_mut(context) else {
        return false;
    };
    load.update_voltage_event(event);
    true
}

fn load_algorithm(context: usize, load: DcLoadModel, elapsed_ns: u64) -> DataflowAlgorithm {
    let node = load.node();
    let current_route_id = load.current_output_key().1;
    let voltage_input_key = load.voltage_input_key();
    let period_ns = load.scheduler_period_ns();
    DataflowAlgorithm::periodic_transform(
        node,
        (node, 7, context),
        vec![RuntimeInterfaces::scalar_edge(
            voltage_input_key.0,
            voltage_input_key.1,
        )],
        vec![RuntimeInterfaces::scalar_edge(node, current_route_id)],
        Arc::new(DcLoadAlgorithm {
            load_index: context,
        }),
        period_ns,
        elapsed_ns.saturating_add(period_ns),
    )
}

struct DcLoadAlgorithm {
    load_index: usize,
}

impl DataflowAlgorithmExecutor for DcLoadAlgorithm {
    fn polls_pending(&self) -> bool {
        true
    }

    fn pending(&self, _runtime: &ClusterRuntime) -> bool {
        DC_LOADS
            .lock()
            .unwrap()
            .get(self.load_index)
            .is_some_and(DcLoadModel::has_voltage_update)
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        run_dc_load(runtime, self.load_index)
    }
}

fn run_dc_load(runtime: &mut ClusterRuntime, context: usize) -> bool {
    let mut loads = DC_LOADS.lock().unwrap();
    let Some(load) = loads.get_mut(context) else {
        return false;
    };
    load.run_until(runtime.elapsed_ns);
    let source_node = load.node();
    let route_id = load.current_output_key().1;
    let event = load.take_current_event(runtime.elapsed_ns);
    drop(loads);

    if let Some(event) = event {
        scalar::route_native_event(runtime, source_node, route_id, event);
        return true;
    }
    false
}

fn register_load(runtime: &mut ClusterRuntime, load: DcLoadModel) -> bool {
    if !runtime.node_exists(load.node()) {
        return false;
    }
    let mut loads = DC_LOADS.lock().unwrap();
    if let Some(index) = loads
        .iter()
        .position(|existing| existing.output_matches(load.node(), load.current_output_key().1))
    {
        if loads[index].config_equals(
            load.node(),
            load.current_output_key().1,
            load.resistance_ohms,
            load.inductance_henrys,
            load.capacitance_farads,
            load.scheduler_period_ns,
        ) {
            return true;
        }
        loads[index] = load;
        drop(loads);
        return algorithms::replace_algorithm(
            runtime,
            load_algorithm(index, load, runtime.elapsed_ns),
        );
    }

    loads.push(load);
    let context = loads.len() - 1;
    let node = load.node();
    let current_route_id = load.current_output_key().1;
    drop(loads);

    algorithms::register_runtime_reset(runtime, reset_runtime);
    algorithms::register_node_reset(runtime, node, context, reset_load);
    algorithms::register_native_scalar_source(
        runtime,
        node,
        current_route_id,
        context,
        take_load_events,
    );
    algorithms::register_native_scalar_input(
        runtime,
        node,
        load.voltage_route_id,
        context,
        receive_load_voltage,
    );
    algorithms::register_algorithm(runtime, load_algorithm(context, load, runtime.elapsed_ns))
}

pub(super) fn add_dc_load(
    runtime: &mut ClusterRuntime,
    node: u32,
    voltage_route_id: u32,
    current_route_id: u32,
    resistance_ohms: f32,
    inductance_henrys: f32,
    capacitance_farads: f32,
    scheduler_period_ns: u64,
) -> bool {
    register_load(
        runtime,
        DcLoadModel::new(
            node,
            voltage_route_id,
            current_route_id,
            resistance_ohms,
            inductance_henrys,
            capacitance_farads,
            scheduler_period_ns,
            runtime.elapsed_ns,
        ),
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_dc_load(
    node: u32,
    voltage_route_id: u32,
    current_route_id: u32,
    resistance_ohms: f32,
    inductance_henrys: f32,
    capacitance_farads: f32,
    scheduler_period_ns: u64,
) -> bool {
    cluster::with_runtime(|runtime| {
        add_dc_load(
            runtime,
            node,
            voltage_route_id,
            current_route_id,
            resistance_ohms,
            inductance_henrys,
            capacitance_farads,
            scheduler_period_ns,
        )
    })
}
