use super::cluster::{CanEvent, CanPacket};

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
