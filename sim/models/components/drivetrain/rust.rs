use std::sync::Arc;
use std::sync::{LazyLock, Mutex};

use super::algorithms;
use super::cluster::{self, ClusterRuntime};
use super::dataflow::{DataflowAlgorithm, DataflowAlgorithmExecutor};
use super::registry::RuntimeInterfaces;
use super::scalar::{self, ScalarEvent};

static DRIVETRAINS: LazyLock<Mutex<Vec<Drivetrain>>> = LazyLock::new(|| Mutex::new(Vec::new()));

#[derive(Clone, Copy)]
struct Drivetrain {
    node: u32,
    voltage_route_id: u32,
    torque_request_route_id: u32,
    torque_output_route_id: u32,
    current_output_route_id: u32,
    max_torque_nm: f32,
    torque_constant_nm_per_amp: f32,
    efficiency: f32,
    max_power_w: f32,
    period_ns: u64,
    voltage: f32,
    torque_request: f32,
    mechanical_torque: f32,
    current_draw: f32,
    voltage_dirty: bool,
    torque_dirty: bool,
    pending_torque: bool,
    pending_current: bool,
}

impl Drivetrain {
    fn reset(&mut self, _elapsed_ns: u64) {
        self.voltage = 0.0;
        self.torque_request = 0.0;
        self.mechanical_torque = 0.0;
        self.current_draw = 0.0;
        self.voltage_dirty = false;
        self.torque_dirty = false;
        self.pending_torque = false;
        self.pending_current = false;
    }

    fn update_voltage(&mut self, event: ScalarEvent) {
        self.voltage = event.value.max(0.0);
        self.voltage_dirty = true;
    }

    fn update_torque_request(&mut self, event: ScalarEvent) {
        self.torque_request = event.value;
        self.torque_dirty = true;
    }

    fn run(&mut self, _elapsed_ns: u64) {
        if !self.voltage_dirty && !self.torque_dirty {
            return;
        }
        let requested = self.torque_request.clamp(-self.max_torque_nm, self.max_torque_nm);
        let voltage_limited_current = if self.max_power_w.is_infinite() || self.voltage <= 0.0 {
            f32::INFINITY
        } else {
            self.max_power_w / self.voltage
        };
        let requested_current = requested.abs() / (self.torque_constant_nm_per_amp * self.efficiency);
        let current = requested_current.min(voltage_limited_current);
        self.current_draw = current;
        self.mechanical_torque = requested.signum() * current * self.torque_constant_nm_per_amp * self.efficiency;
        self.voltage_dirty = false;
        self.torque_dirty = false;
        self.pending_torque = true;
        self.pending_current = true;
    }

    fn take_output(&mut self, torque: bool, elapsed_ns: u64) -> Option<ScalarEvent> {
        let pending = if torque { &mut self.pending_torque } else { &mut self.pending_current };
        if !*pending {
            return None;
        }
        *pending = false;
        Some(ScalarEvent {
            value: if torque { self.mechanical_torque } else { self.current_draw },
            timestamp_ns: elapsed_ns,
        })
    }
}

fn reset_runtime() {
    DRIVETRAINS.lock().unwrap().clear();
}

fn reset_drivetrain(context: usize, elapsed_ns: u64) {
    if let Some(drivetrain) = DRIVETRAINS.lock().unwrap().get_mut(context) {
        drivetrain.reset(elapsed_ns);
    }
}

fn take_output_events(context: usize, elapsed_ns: u64) -> Vec<ScalarEvent> {
    let torque = context & 1 == 0;
    let index = context / 2;
    DRIVETRAINS.lock().unwrap().get_mut(index)
        .and_then(|drivetrain| drivetrain.take_output(torque, elapsed_ns))
        .into_iter().collect()
}

fn receive_voltage(context: usize, event: ScalarEvent) -> bool {
    DRIVETRAINS.lock().unwrap().get_mut(context)
        .map(|drivetrain| { drivetrain.update_voltage(event); true })
        .unwrap_or(false)
}

fn receive_torque_request(context: usize, event: ScalarEvent) -> bool {
    DRIVETRAINS.lock().unwrap().get_mut(context)
        .map(|drivetrain| { drivetrain.update_torque_request(event); true })
        .unwrap_or(false)
}

struct DrivetrainAlgorithm { index: usize }

impl DataflowAlgorithmExecutor for DrivetrainAlgorithm {
    fn polls_pending(&self) -> bool { true }

    fn pending(&self, _runtime: &ClusterRuntime) -> bool {
        DRIVETRAINS.lock().unwrap().get(self.index)
            .is_some_and(|drivetrain| drivetrain.voltage_dirty || drivetrain.torque_dirty)
    }

    fn run(&self, runtime: &mut ClusterRuntime) -> bool {
        let mut drivetrains = DRIVETRAINS.lock().unwrap();
        let Some(drivetrain) = drivetrains.get_mut(self.index) else { return false };
        drivetrain.run(runtime.elapsed_ns);
        let node = drivetrain.node;
        let torque_route = drivetrain.torque_output_route_id;
        let current_route = drivetrain.current_output_route_id;
        let torque = drivetrain.take_output(true, runtime.elapsed_ns);
        let current = drivetrain.take_output(false, runtime.elapsed_ns);
        drop(drivetrains);
        if let Some(event) = torque { scalar::route_native_event(runtime, node, torque_route, event); }
        if let Some(event) = current { scalar::route_native_event(runtime, node, current_route, event); }
        torque.is_some() || current.is_some()
    }
}

fn register(runtime: &mut ClusterRuntime, drivetrain: Drivetrain) -> bool {
    if !runtime.node_exists(drivetrain.node) { return false; }
    let mut drivetrains = DRIVETRAINS.lock().unwrap();
    let index = drivetrains.len();
    drivetrains.push(drivetrain);
    drop(drivetrains);
    let algorithm = DataflowAlgorithm::periodic_transform(
        drivetrain.node,
        (drivetrain.node, 9, index),
        vec![
            RuntimeInterfaces::scalar_edge(drivetrain.node, drivetrain.voltage_route_id),
            RuntimeInterfaces::scalar_edge(drivetrain.node, drivetrain.torque_request_route_id),
        ],
        vec![
            RuntimeInterfaces::scalar_edge(drivetrain.node, drivetrain.torque_output_route_id),
            RuntimeInterfaces::scalar_edge(drivetrain.node, drivetrain.current_output_route_id),
        ],
        Arc::new(DrivetrainAlgorithm { index }),
        drivetrain.period_ns,
        runtime.elapsed_ns.saturating_add(drivetrain.period_ns),
    )
    .with_runtime_reset(reset_runtime)
    .with_node_reset(drivetrain.node, index, reset_drivetrain)
    .with_scalar_input(drivetrain.node, drivetrain.voltage_route_id, index, receive_voltage)
    .with_scalar_input(drivetrain.node, drivetrain.torque_request_route_id, index, receive_torque_request)
    .with_scalar_source(drivetrain.node, drivetrain.torque_output_route_id, index * 2, take_output_events)
    .with_scalar_source(drivetrain.node, drivetrain.current_output_route_id, index * 2 + 1, take_output_events);
    if algorithms::register_algorithm(runtime, algorithm) { true } else {
        DRIVETRAINS.lock().unwrap().pop();
        false
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_register_drivetrain(
    node: u32, voltage_route_id: u32, torque_request_route_id: u32,
    torque_output_route_id: u32, current_output_route_id: u32,
    max_torque_nm: f32, torque_constant_nm_per_amp: f32, efficiency: f32,
    max_power_w: f32, period_ns: u64,
) -> bool {
    if !max_torque_nm.is_finite() || max_torque_nm <= 0.0
        || !torque_constant_nm_per_amp.is_finite() || torque_constant_nm_per_amp <= 0.0
        || !efficiency.is_finite() || efficiency <= 0.0 || efficiency > 1.0
        || max_power_w.is_nan() || max_power_w <= 0.0 || period_ns == 0 { return false; }
    cluster::with_runtime(|runtime| register(runtime, Drivetrain {
        node, voltage_route_id, torque_request_route_id, torque_output_route_id,
        current_output_route_id, max_torque_nm, torque_constant_nm_per_amp,
        efficiency, max_power_w, period_ns, voltage: 0.0, torque_request: 0.0,
        mechanical_torque: 0.0, current_draw: 0.0, voltage_dirty: false,
        torque_dirty: false, pending_torque: false, pending_current: false,
    }))
}
