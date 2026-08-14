use super::cluster::ScalarEvent;

#[derive(Clone, Copy)]
pub struct BatterySourceModel {
    node: u32,
    voltage_route_id: u32,
    open_circuit_voltage: f32,
    internal_resistance_ohms: f32,
    capacity_amp_hours: f32,
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

    #[allow(dead_code)]
    pub fn update_load_current(&mut self, current_amps: f32) {
        let current = current_amps.max(0.0);
        self.output_voltage =
            (self.open_circuit_voltage - current * self.internal_resistance_ohms).max(0.0);
        if self.capacity_amp_hours.is_finite() {
            // Capacity state is intentionally reserved for the next-order battery model.
        }
        self.pending_voltage = true;
    }
}
