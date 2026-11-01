// Deterministic host backend for RTIC task semantics. Cortex-M/NVIC execution
// costs are intentionally outside this software-in-the-loop model.
use super::can::{self, CanPacket};
use super::model::ModelRuntime;

pub trait RticApplication {
    fn reset(&mut self);
    fn can_interrupt(&mut self, bus: u8, packet: CanPacket);
    fn timer_interrupt(&mut self);
    fn reset_requested(&self) -> bool {
        false
    }
}

pub struct RticController<A: RticApplication> {
    pub application: A,
    remainder_ns: u64,
    pub resets: u32,
}
impl<A: RticApplication> RticController<A> {
    pub fn new(application: A) -> Self {
        Self {
            application,
            remainder_ns: 0,
            resets: 0,
        }
    }
    fn can_interrupts(&mut self) {
        for bus in 0..can::bus_count() {
            let mut packet = CanPacket::default();
            while can::rig_runtime_can_pop_rx(bus, &mut packet) {
                self.application.can_interrupt(bus, packet);
            }
        }
    }
    pub fn set_digital(&self, channel: i32, state: bool) {
        super::io::set_digital(channel, state);
    }
}
impl<A: RticApplication> ModelRuntime for RticController<A> {
    unsafe fn reset_backend(&mut self) {
        super::spi::reset();
        super::timer::reset();
    }
    fn reset_runtime(&mut self) {
        can::reset();
        super::reset_host_time();
        self.remainder_ns = 0;
        self.resets = 0;
    }
    unsafe fn reset_scheduler(&mut self) {
        self.application.reset();
    }
    unsafe fn run_for_ns(&mut self, mut elapsed_ns: u64) {
        // CAN is the higher-priority task, including when time has not advanced.
        self.can_interrupts();
        while elapsed_ns > 0 {
            let delta = elapsed_ns.min(1_000_000 - self.remainder_ns);
            super::rig_runtime_advance_time_ns(delta);
            self.remainder_ns += delta;
            elapsed_ns -= delta;
            if self.remainder_ns == 1_000_000 {
                self.remainder_ns = 0;
                self.can_interrupts();
                self.application.timer_interrupt();
                if self.application.reset_requested() {
                    // Keep external pin levels and already-emitted CAN replies.
                    self.application.reset();
                    self.resets = self.resets.wrapping_add(1);
                }
            }
        }
    }
}
