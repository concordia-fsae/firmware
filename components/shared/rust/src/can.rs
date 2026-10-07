//! Protocol-independent CAN state and periodic scheduling. Wire codecs come from Yamcan.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Frame {
    pub id: u32,
    pub len: u8,
    pub data: [u8; 8],
}
pub trait Message: Copy {
    fn id(self) -> u32;
    fn timeout(self) -> u32;
    fn valid(self, frame: &Frame) -> bool;
}
pub trait Signal: Copy {
    fn message(self) -> usize;
    fn sna(self) -> Option<u64>;
    fn raw(self, data: &[u8; 8]) -> u64;
    fn value(self, raw: u64) -> f64;
}
#[derive(Clone, Copy)]
struct Received {
    data: [u8; 8],
    at: Option<u32>,
}
pub struct Network<M: Message + 'static, const N: usize> {
    messages: &'static [M; N],
    received: [Received; N],
}
impl<M: Message, const N: usize> Network<M, N> {
    pub fn new(messages: &'static [M; N]) -> Self {
        Self {
            messages,
            received: [Received {
                data: [0; 8],
                at: None,
            }; N],
        }
    }
}
impl<M: Message, const N: usize> Network<M, N> {
    pub fn receive(&mut self, frame: impl Into<Frame>, now: u32) -> bool {
        let frame = frame.into();
        if let Some((i, msg)) = self
            .messages
            .iter()
            .enumerate()
            .find(|(_, m)| m.id() == frame.id)
        {
            if msg.valid(&frame) {
                self.received[i] = Received {
                    data: frame.data,
                    at: Some(now),
                };
                return true;
            }
            // A bad checksum does not refresh the last valid value.
        }
        false
    }
    pub fn raw(&self, signal: impl Signal, now: u32) -> Option<u32> {
        let r = &self.received[signal.message()];
        let at = r.at?;
        if now.wrapping_sub(at) > self.messages[signal.message()].timeout() {
            return None;
        }
        let raw = signal.raw(&r.data);
        if signal.sna() == Some(raw) {
            None
        } else {
            Some(raw as u32)
        }
    }
    pub fn value(&self, signal: impl Signal, now: u32) -> Option<f32> {
        let raw = self.raw(signal, now)? as u64;
        Some(signal.value(raw) as f32)
    }
    pub fn fault(&self, signal: impl Signal, now: u32) -> bool {
        self.raw(signal, now) == Some(1)
    }
}

/// One wrapping millisecond deadline and rolling counter per message.
pub struct Scheduler<const N: usize> {
    last_tx: [u32; N],
    counters: [u8; N],
}
impl<const N: usize> Default for Scheduler<N> {
    fn default() -> Self {
        Self {
            last_tx: [0; N],
            counters: [0; N],
        }
    }
}
impl<const N: usize> Scheduler<N> {
    pub fn due(&mut self, index: usize, period: u32, now: u32) -> Option<u8> {
        if now.wrapping_sub(self.last_tx[index]) < period {
            return None;
        }
        self.last_tx[index] = now;
        self.counters[index] = self.counters[index].wrapping_add(1);
        Some(self.counters[index])
    }
}

#[cfg(test)]
mod tests {
    use super::{Frame, RxQueue, Scheduler, TxQueue};

    fn frame(id: u32, value: u8) -> Frame {
        Frame {
            id,
            len: 1,
            data: [value; 8],
        }
    }

    #[test]
    fn receive_overflow_keeps_the_oldest_frames() {
        let mut queue = RxQueue::<2>::default();
        queue.push(frame(1, 1));
        queue.push(frame(2, 2));
        queue.push(frame(3, 3));
        assert_eq!(queue.dropped, 1);
        assert_eq!(queue.pop(), Some(frame(1, 1)));
        assert_eq!(queue.pop(), Some(frame(2, 2)));
        assert_eq!(queue.pop(), None);
    }

    #[test]
    fn transmit_coalesces_state_and_preserves_diagnostic_order() {
        let mut queue = TxQueue::<3>::new(0x691);
        queue.enqueue(frame(1, 1));
        queue.enqueue(frame(0x691, 2));
        queue.enqueue(frame(1, 3));
        queue.enqueue(frame(0x691, 4));
        assert_eq!(queue.pop(), Some(frame(1, 3)));
        queue.requeue(frame(2, 5));
        assert_eq!(queue.pop(), Some(frame(2, 5)));
        assert_eq!(queue.pop(), Some(frame(0x691, 2)));
        assert_eq!(queue.pop(), Some(frame(0x691, 4)));
        assert_eq!(queue.pop(), None);
    }
    #[test]
    fn schedule_preserves_deadlines_and_counters_across_wrap() {
        let mut schedule = Scheduler::<2>::default();
        assert_eq!(schedule.due(0, 10, u32::MAX - 10), Some(1));
        assert_eq!(schedule.due(0, 10, u32::MAX - 1), None);
        assert_eq!(schedule.due(0, 10, 0), Some(2));
        assert_eq!(schedule.due(1, 50, 49), None);
        assert_eq!(schedule.due(1, 50, 50), Some(1));
        for i in 1..=254 {
            assert_eq!(schedule.due(0, 10, i * 10), Some((i + 2) as u8));
        }
        assert_eq!(schedule.due(0, 10, 2541), None);
    }
}

/// Bounded receive buffering used by a higher-priority CAN task.
pub struct RxQueue<const N: usize> {
    frames: heapless::Deque<Frame, N>,
    pub dropped: u32,
}
impl<const N: usize> Default for RxQueue<N> {
    fn default() -> Self {
        Self {
            frames: heapless::Deque::new(),
            dropped: 0,
        }
    }
}
impl<const N: usize> RxQueue<N> {
    pub fn push(&mut self, frame: Frame) {
        if self.frames.push_back(frame).is_err() {
            self.dropped = self.dropped.wrapping_add(1);
        }
    }
    pub fn pop(&mut self) -> Option<Frame> {
        self.frames.pop_front()
    }
}

/// Coalesce periodic state while keeping diagnostics ordered, on every backend.
pub struct TxQueue<const N: usize> {
    frames: heapless::Deque<Frame, N>,
    ordered_id: u32,
}
impl<const N: usize> TxQueue<N> {
    pub fn new(ordered_id: u32) -> Self {
        Self {
            frames: heapless::Deque::new(),
            ordered_id,
        }
    }
    pub fn enqueue(&mut self, frame: Frame) {
        assert!(
            frame.id <= 0x7ff && frame.len <= 8,
            "invalid standard CAN frame"
        );
        if frame.id != self.ordered_id {
            if let Some(pending) = self.frames.iter_mut().find(|p| p.id == frame.id) {
                *pending = frame;
                return;
            }
        }
        if self.frames.push_back(frame).is_err() {
            panic!("CAN transmit queue exhausted");
        }
    }
    pub fn front(&self) -> Option<&Frame> {
        self.frames.front()
    }
    pub fn pop(&mut self) -> Option<Frame> {
        self.frames.pop_front()
    }
    pub fn requeue(&mut self, frame: Frame) {
        if self.frames.push_front(frame).is_err() {
            panic!("CAN transmit queue exhausted");
        }
    }
}
