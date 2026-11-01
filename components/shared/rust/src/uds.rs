use crate::can::Frame;
/// The bootloader and conUDS use little-endian DIDs and omit the DID echo.
#[derive(Clone, Copy, Default)]
pub struct Descriptor {
    pub start: u32,
    pub end: u32,
    pub crc_location: u32,
    pub crc: u32,
    pub component: u16,
    pub variant: u16,
}
impl Descriptor {
    /// Read the repository bootloader descriptor and its appended CRC.
    ///
    /// # Safety
    /// The address must contain an aligned, readable descriptor whose CRC pointer
    /// also points to readable flash.
    pub unsafe fn read_flash(address: u32) -> Self {
        let p = address as *const u32;
        let crc_location = p.add(2).read();
        Self {
            start: p.read(),
            end: p.add(1).read(),
            crc_location,
            crc: (crc_location as *const u32).read(),
            component: (p.add(3) as *const u16).read(),
            variant: (p.add(3) as *const u16).add(1).read(),
        }
    }
}
pub struct Diagnostics {
    request_id: u32,
    response_id: u32,
    pub session: u8,
    last_request: u32,
    pub reset_at: Option<u32>,
    // Requests may use ISO-TP even though all supported responses fit a single frame.
    rx: [u8; 128],
    rx_len: usize,
    rx_expected: usize,
    rx_sequence: u8,
    rx_at: u32,
}
impl Diagnostics {
    pub fn new(request_id: u32, response_id: u32) -> Self {
        Self {
            request_id,
            response_id,
            session: 1,
            last_request: 0,
            reset_at: None,
            rx: [0; 128],
            rx_len: 0,
            rx_expected: 0,
            rx_sequence: 1,
            rx_at: 0,
        }
    }
}
impl Diagnostics {
    pub fn tick(&mut self, now: u32) {
        if now.wrapping_sub(self.last_request) >= 100 {
            self.session = 1;
        }
        if self.rx_expected != 0 && now.wrapping_sub(self.rx_at) >= 1000 {
            self.rx_expected = 0;
        }
    }
    pub fn reset_due(&self, now: u32) -> bool {
        self.reset_at
            .is_some_and(|at| now.wrapping_sub(at) < 0x8000_0000)
    }
    fn response(&self, payload: &[u8]) -> Frame {
        let mut data = [0; 8];
        data[0] = payload.len() as u8;
        data[1..=payload.len()].copy_from_slice(payload);
        Frame {
            id: self.response_id,
            len: 8,
            data,
        }
    }
    fn negative(&self, sid: u8, nrc: u8) -> Frame {
        self.response(&[0x7f, sid, nrc])
    }
    pub fn receive(&mut self, frame: Frame, now: u32, descriptor: Descriptor) -> Option<Frame> {
        if frame.id != self.request_id || frame.len == 0 || frame.len > 8 {
            return None;
        }
        let size = frame.len as usize;
        match frame.data[0] >> 4 {
            0 => {
                self.rx_expected = 0;
                let len = (frame.data[0] & 15) as usize;
                if len == 0 || len >= size {
                    return None;
                }
                self.service(&frame.data[1..=len], now, descriptor)
            }
            1 if size == 8 => {
                let expected = (((frame.data[0] & 15) as usize) << 8) | frame.data[1] as usize;
                self.rx_expected = 0;
                let mut data = [0; 8];
                if expected <= 7 || expected > self.rx.len() {
                    data[0] = 0x32;
                } else {
                    data[0] = 0x30;
                    self.rx[..6].copy_from_slice(&frame.data[2..]);
                    self.rx_len = 6;
                    self.rx_expected = expected;
                    self.rx_sequence = 1;
                    self.rx_at = now;
                }
                Some(Frame {
                    id: self.response_id,
                    len: 8,
                    data,
                })
            }
            2 if self.rx_expected != 0 => {
                if frame.data[0] & 15 != self.rx_sequence || now.wrapping_sub(self.rx_at) >= 1000 {
                    self.rx_expected = 0;
                    return None;
                }
                let count = (self.rx_expected - self.rx_len).min(7);
                if size < count + 1 {
                    self.rx_expected = 0;
                    return None;
                }
                self.rx[self.rx_len..self.rx_len + count]
                    .copy_from_slice(&frame.data[1..count + 1]);
                self.rx_len += count;
                self.rx_sequence = (self.rx_sequence + 1) & 15;
                self.rx_at = now;
                if self.rx_len == self.rx_expected {
                    let payload = self.rx;
                    let len = self.rx_expected;
                    self.rx_expected = 0;
                    self.service(&payload[..len], now, descriptor)
                } else {
                    None
                }
            }
            _ => None,
        }
    }
    fn service(&mut self, payload: &[u8], now: u32, desc: Descriptor) -> Option<Frame> {
        self.last_request = now;
        let sid = payload[0];
        let sub = payload.get(1).copied().unwrap_or(0) & 0x7f;
        let suppress = payload.get(1).is_some_and(|b| b & 0x80 != 0);
        let response = match sid {
            0x10 => {
                if payload.len() != 2 {
                    self.negative(sid, 0x13)
                } else if !(1..=3).contains(&sub) {
                    self.negative(sid, 0x12)
                } else {
                    self.session = sub;
                    if suppress {
                        return None;
                    }
                    self.response(&[0x50, sub])
                }
            }
            0x3e => {
                if payload.len() != 2 {
                    self.negative(sid, 0x13)
                } else if sub != 0 {
                    self.negative(sid, 0x12)
                } else {
                    if suppress {
                        return None;
                    }
                    self.response(&[0x7e, 0])
                }
            }
            0x11 => {
                if payload.len() != 2 {
                    self.negative(sid, 0x13)
                } else if sub != 1 {
                    self.negative(sid, 0x12)
                } else {
                    self.reset_at = Some(now.wrapping_add(10));
                    if suppress {
                        return None;
                    }
                    self.response(&[0x51, 1])
                }
            }
            0x22 => {
                if payload.len() != 3 {
                    return Some(self.negative(sid, 0x13));
                }
                let did = u16::from_le_bytes([payload[1], payload[2]]);
                let mut data = [0; 5];
                data[0] = 0x62;
                let bytes = match did {
                    0 => desc.start.to_le_bytes(),
                    1 => desc.end.to_le_bytes(),
                    2 => desc.crc_location.to_le_bytes(),
                    3 => desc.crc.to_le_bytes(),
                    4 => {
                        data[1..3].copy_from_slice(&desc.component.to_le_bytes());
                        return Some(self.response(&data[..3]));
                    }
                    5 => {
                        data[1..3].copy_from_slice(&desc.variant.to_le_bytes());
                        return Some(self.response(&data[..3]));
                    }
                    0x102 => return Some(self.response(&[0x62, self.session])),
                    0x101 => return Some(self.response(&[0x62, 1])),
                    _ => return Some(self.negative(sid, 0x10)),
                };
                data[1..].copy_from_slice(&bytes);
                self.response(&data)
            }
            0x31 => self.negative(sid, if payload.len() < 4 { 0x13 } else { 0x11 }),
            _ => self.negative(sid, 0x11),
        };
        Some(response)
    }
}

#[cfg(test)]
mod tests {
    use super::{Descriptor, Diagnostics};
    use crate::can::Frame;
    #[test]
    fn diagnostics_addressing_is_independent_of_the_application() {
        let mut server = Diagnostics::new(0x123, 0x456);
        let request = Frame {
            id: 0x123,
            len: 8,
            data: [2, 0x3e, 0, 0, 0, 0, 0, 0],
        };
        let reply = server.receive(request, 0, Descriptor::default()).unwrap();
        assert_eq!(reply.id, 0x456);
        assert_eq!(&reply.data[..3], &[2, 0x7e, 0]);
        let mut other = Diagnostics::new(0x321, 0x654);
        assert!(other.receive(request, 0, Descriptor::default()).is_none());
    }
}
