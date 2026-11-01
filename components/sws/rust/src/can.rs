//! SWS adapters for Yamcan's generated wrappers and shared CAN state.
use crate::protocol::MESSAGES;
pub use crate::protocol::{get_bits, set_bits, Message, Signal};
pub use firmware_shared::can::{Frame, Scheduler};

impl From<crate::protocol::Frame> for Frame {
    fn from(frame: crate::protocol::Frame) -> Self {
        Self {
            id: frame.id,
            len: frame.len,
            data: frame.data,
        }
    }
}
impl From<Frame> for crate::protocol::Frame {
    fn from(frame: Frame) -> Self {
        Self {
            id: frame.id,
            len: frame.len,
            data: frame.data,
        }
    }
}
impl firmware_shared::can::Message for Message {
    fn id(self) -> u32 {
        self.id
    }
    fn timeout(self) -> u32 {
        self.timeout
    }
    fn valid(self, frame: &Frame) -> bool {
        self.valid(&(*frame).into())
    }
}
impl firmware_shared::can::Signal for Signal {
    fn message(self) -> usize {
        self.message
    }
    fn sna(self) -> Option<u64> {
        self.sna
    }
    fn raw(self, data: &[u8; 8]) -> u64 {
        self.raw(data)
    }
    fn value(self, raw: u64) -> f64 {
        self.value(raw)
    }
}
pub struct Network(firmware_shared::can::Network<Message, { MESSAGES.len() }>);
impl Default for Network {
    fn default() -> Self {
        Self(firmware_shared::can::Network::new(&MESSAGES))
    }
}
impl core::ops::Deref for Network {
    type Target = firmware_shared::can::Network<Message, { MESSAGES.len() }>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
impl core::ops::DerefMut for Network {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}
