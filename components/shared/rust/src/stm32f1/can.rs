use crate::can::Frame;
use bxcan::{filter::Mask32, Fifo, Id, Interrupt, StandardId};
use heapless::Deque;
use stm32f1xx_hal::{can::Can, pac};
pub type CanDevice = bxcan::Can<Can<pac::CAN1>>;
pub struct Bus<const N: usize> {
    can: CanDevice,
    rx: Deque<Frame, N>,
    pub dropped: u32,
}
impl<const N: usize> Bus<N> {
    pub fn new(can: Can<pac::CAN1>, bit_timing: u32, ids: impl IntoIterator<Item = u32>) -> Self {
        // The application supplies timing for its peripheral clock and bus speed.
        let mut can = bxcan::Can::builder(can)
            .set_bit_timing(bit_timing)
            .set_automatic_retransmit(true)
            .leave_disabled();
        {
            let mut filters = can.modify_filters();
            for (bank, id) in ids.into_iter().enumerate() {
                filters.enable_bank(
                    bank as u8,
                    Fifo::Fifo0,
                    Mask32::frames_with_std_id(
                        StandardId::new(id as u16).unwrap(),
                        StandardId::new(0x7ff).unwrap(),
                    ),
                );
            }
        }
        // ABOM makes the controller recover from bus-off after the bus becomes quiet.
        unsafe {
            (*pac::CAN1::ptr()).mcr.modify(|_, w| w.abom().set_bit());
        }
        nb::block!(can.enable_non_blocking()).unwrap();
        can.enable_interrupt(Interrupt::Fifo0MessagePending);
        Self {
            can,
            rx: Deque::new(),
            dropped: 0,
        }
    }
    pub fn pop_received(&mut self) -> Option<Frame> {
        self.rx.pop_front()
    }
    pub fn transmit(
        &mut self,
        frame: &bxcan::Frame,
    ) -> nb::Result<bxcan::TransmitStatus, core::convert::Infallible> {
        self.can.transmit(frame)
    }
    pub fn receive(&mut self) {
        // Drain at most one hardware FIFO depth; pending frames retrigger the IRQ.
        for _ in 0..3 {
            match self.can.receive() {
                Ok(frame) if !frame.is_remote_frame() => {
                    let Id::Standard(id) = frame.id() else {
                        continue;
                    };
                    let payload = frame.data().unwrap();
                    let mut data = [0; 8];
                    data[..payload.len()].copy_from_slice(payload);
                    if self
                        .rx
                        .push_back(Frame {
                            id: id.as_raw() as u32,
                            len: frame.dlc(),
                            data,
                        })
                        .is_err()
                    {
                        self.dropped = self.dropped.wrapping_add(1);
                    }
                }
                Ok(_) => {}
                Err(nb::Error::WouldBlock) => break,
                Err(nb::Error::Other(_)) => {
                    self.dropped = self.dropped.wrapping_add(1);
                }
            }
        }
    }
}

/// Periodic frames coalesce by ID; diagnostic responses preserve their order.
pub struct TxQueue<const N: usize> {
    pending: Deque<bxcan::Frame, N>,
    ordered_id: u32,
}
impl<const N: usize> TxQueue<N> {
    pub fn new(ordered_id: u32) -> Self {
        Self {
            pending: Deque::new(),
            ordered_id,
        }
    }
    pub fn enqueue(&mut self, frame: Frame) {
        let frame = bxcan::Frame::new_data(
            StandardId::new(frame.id as u16).unwrap(),
            bxcan::Data::new(&frame.data[..frame.len as usize]).unwrap(),
        );
        // Keep only the newest periodic state when the bus is congested. Diagnostic responses are ordered.
        if frame.id() != Id::Standard(StandardId::new(self.ordered_id as u16).unwrap()) {
            if let Some(pending) = self
                .pending
                .iter_mut()
                .find(|pending| pending.id() == frame.id())
            {
                *pending = frame;
                return;
            }
        }
        if self.pending.push_back(frame).is_err() {
            panic!("CAN transmit queue exhausted");
        }
    }
    pub fn flush(
        &mut self,
        mut transmit: impl FnMut(
            &bxcan::Frame,
        )
            -> nb::Result<bxcan::TransmitStatus, core::convert::Infallible>,
    ) {
        for _ in 0..3 {
            let Some(frame) = self.pending.front() else {
                break;
            };
            let result = transmit(frame);
            match result {
                Ok(status) => {
                    self.pending.pop_front();
                    if let Some(displaced) = status.dequeued_frame() {
                        self.pending.push_front(displaced.clone()).unwrap();
                    }
                }
                Err(_) => break,
            }
        }
    }
}
