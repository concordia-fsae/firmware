use super::cluster::ScalarEvent;

#[derive(Clone, Copy)]
pub struct DcLoadModel {
    node: u32,
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
        (self.node, self.voltage_route_id)
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

    pub fn config_matches(
        &self,
        node: u32,
        voltage_route_id: u32,
        current_route_id: u32,
    ) -> bool {
        self.node == node
            && self.voltage_route_id == voltage_route_id
            && self.current_route_id == current_route_id
    }

    pub fn next_step_ns(&self, elapsed_ns: u64, max_step_ns: u64) -> u64 {
        if self.scheduler_period_ns == 0 {
            return max_step_ns;
        }
        let elapsed_in_period = elapsed_ns.saturating_sub(self.last_update_ns);
        if elapsed_in_period >= self.scheduler_period_ns {
            return self.scheduler_period_ns.min(max_step_ns);
        }
        (self.scheduler_period_ns - elapsed_in_period).min(max_step_ns)
    }

    pub fn update_voltage(&mut self, events: &[ScalarEvent]) {
        if let Some(event) = events.last() {
            self.update_voltage_event(*event);
        }
    }

    pub fn update_voltage_event(&mut self, event: ScalarEvent) {
        self.input_voltage = event.value.max(0.0);
        self.voltage_dirty = true;
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
