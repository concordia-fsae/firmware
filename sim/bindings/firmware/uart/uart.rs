// UART receive adapters for the single-threaded simulator. Configuration,
// firmware execution, injection, and reset must use the same simulation thread.
// Injections run between firmware tasks, which access DMA memory directly.
use std::cell::RefCell;
use std::collections::{BTreeMap, VecDeque};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RxMode {
    CircularDma,
    SingleShotDma,
    Polling { capacity: usize },
}

struct DmaRx {
    // Firmware owns this memory through stop/reset or single-shot completion.
    address: usize,
    size: usize,
    position: usize,
}

struct Port {
    mode: RxMode,
    dma: Option<DmaRx>,
    fifo: VecDeque<u8>,
    completed: bool,
    accepted: u64,
    dropped: u64,
}

impl Port {
    fn new(mode: RxMode) -> Self {
        Self {
            mode,
            dma: None,
            fifo: VecDeque::new(),
            completed: false,
            accepted: 0,
            dropped: 0,
        }
    }

    fn start(&mut self, data: *mut u8, size: usize) -> bool {
        if data.is_null()
            || size == 0
            || self.dma.is_some()
            || matches!(self.mode, RxMode::Polling { .. })
        {
            return false;
        }
        self.dma = Some(DmaRx {
            address: data as usize,
            size,
            position: 0,
        });
        self.completed = false;
        true
    }

    fn stop(&mut self) {
        self.dma = None;
        self.completed = false;
    }

    // Safety: any registered DMA buffer must still be valid and exclusively
    // accessible for this call; caller must not run firmware simultaneously.
    unsafe fn receive(&mut self, bytes: &[u8]) -> usize {
        let accepted = match self.mode {
            RxMode::Polling { capacity } => {
                let count = bytes.len().min(capacity - self.fifo.len());
                self.fifo.extend(&bytes[..count]);
                count
            }
            mode => {
                let mut count = 0;
                if let Some(rx) = self.dma.as_mut() {
                    for &byte in bytes {
                        unsafe {
                            *((rx.address as *mut u8).add(rx.position)) = byte;
                        }
                        count += 1;
                        rx.position += 1;
                        if rx.position == rx.size {
                            if mode == RxMode::SingleShotDma {
                                self.completed = true;
                                break;
                            }
                            rx.position = 0;
                        }
                    }
                }
                // Do not retain a pointer to a completed single-shot buffer.
                if self.completed {
                    self.dma = None;
                }
                count
            }
        };
        self.accepted = self.accepted.saturating_add(accepted as u64);
        self.dropped = self.dropped.saturating_add((bytes.len() - accepted) as u64);
        accepted
    }

    fn read(&mut self, output: &mut [u8]) -> Option<usize> {
        if !matches!(self.mode, RxMode::Polling { .. }) {
            return None;
        }
        let count = output.len().min(self.fifo.len());
        for slot in &mut output[..count] {
            *slot = self.fifo.pop_front().unwrap();
        }
        Some(count)
    }

    fn status(&self) -> RxStatus {
        RxStatus {
            accepted: self.accepted,
            dropped: self.dropped,
            remaining: self
                .dma
                .as_ref()
                .map_or(0, |rx| (rx.size - rx.position) as u32),
            queued: self.fifo.len() as u32,
            active: u32::from(self.dma.is_some() || matches!(self.mode, RxMode::Polling { .. })),
            completed: u32::from(self.completed),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RxStatus {
    pub accepted: u64,
    pub dropped: u64,
    pub remaining: u32,
    pub queued: u32,
    pub active: u32,
    pub completed: u32,
}

thread_local! {
    static PORTS: RefCell<BTreeMap<i32, Port>> = const { RefCell::new(BTreeMap::new()) };
}

fn with_port<T>(id: i32, f: impl FnOnce(&mut Port) -> T) -> Option<T> {
    PORTS.with_borrow_mut(|ports| ports.get_mut(&id).map(f))
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_uart_reset() {
    reset();
}

pub fn reset() {
    PORTS.with_borrow_mut(|ports| ports.clear());
}

/// Configure before firmware initialization. Reconfiguration is rejected until
/// reset, so it cannot silently invalidate an active firmware-owned buffer.
pub fn configure(port: i32, mode: RxMode) -> bool {
    if port < 0
        || matches!(mode, RxMode::Polling { capacity } if capacity == 0 || capacity > u32::MAX as usize)
    {
        return false;
    }
    PORTS.with_borrow_mut(|ports| {
        if ports.contains_key(&port) {
            return false;
        }
        ports.insert(port, Port::new(mode));
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_uart_configure_rx(port: i32, mode: u32, capacity: u32) -> bool {
    let mode = match mode {
        0 => RxMode::CircularDma,
        1 => RxMode::SingleShotDma,
        2 => RxMode::Polling {
            capacity: capacity as usize,
        },
        _ => return false,
    };
    configure(port, mode)
}

/// Safety: data must remain writable for size bytes until stop/reset/completion.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_runtime_uart_start_rx(id: i32, data: *mut u8, size: u32) -> bool {
    with_port(id, |port| port.start(data, size as usize)).unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_uart_stop_rx(id: i32) {
    with_port(id, Port::stop);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_uart_rx_remaining(id: i32) -> u32 {
    with_port(id, |port| port.status().remaining).unwrap_or(0)
}

/// Returns accepted bytes, or -1 for an invalid port/input. Inactive DMA and
/// polling FIFO overflow drop bytes rather than queueing an impossible backlog.
/// Safety: data is readable for size bytes; registered DMA memory is accessible
/// exclusively during this call. Injection must occur between scheduler steps.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_uart_receive(id: i32, data: *const u8, size: u32) -> i64 {
    if data.is_null() && size != 0 {
        return -1;
    }
    let bytes = if size == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(data, size as usize) }
    };
    with_port(id, |port| unsafe { port.receive(bytes) as i64 }).unwrap_or(-1)
}

/// Nonblocking firmware polling read. Returns -1 for invalid arguments or a
/// non-polling port. It never advances the simulation clock or waits for input.
/// Safety: output is writable for capacity bytes and does not alias UART state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_runtime_uart_read_rx(id: i32, output: *mut u8, capacity: u32) -> i64 {
    if output.is_null() && capacity != 0 {
        return -1;
    }
    let output = if capacity == 0 {
        &mut []
    } else {
        unsafe { std::slice::from_raw_parts_mut(output, capacity as usize) }
    };
    with_port(id, |port| port.read(output))
        .flatten()
        .map_or(-1, |count| count as i64)
}

/// Safety: status points to writable RxStatus storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_uart_rx_status(id: i32, status: *mut RxStatus) -> bool {
    if status.is_null() {
        return false;
    }
    let Some(snapshot) = with_port(id, |port| port.status()) else {
        return false;
    };
    unsafe {
        status.write(snapshot);
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn circular_partial_exact_and_multiple_wraps() {
        let mut rx = Port::new(RxMode::CircularDma);
        let mut buffer = [0; 4];
        assert!(rx.start(buffer.as_mut_ptr(), buffer.len()));
        unsafe {
            assert_eq!(rx.receive(b"ab"), 2);
        }
        assert_eq!(buffer, [b'a', b'b', 0, 0]);
        assert_eq!(rx.status().remaining, 2);
        unsafe {
            assert_eq!(rx.receive(b"cd"), 2);
        }
        assert_eq!(&buffer, b"abcd");
        assert_eq!(rx.status().remaining, 4);
        unsafe {
            assert_eq!(rx.receive(b"efghijklm"), 9);
        }
        assert_eq!(&buffer, b"mjkl");
        assert_eq!(rx.status().remaining, 3);
        assert_eq!(rx.status().accepted, 13);
        assert_eq!(rx.status().dropped, 0);
        assert_eq!(rx.status().completed, 0);
    }

    #[test]
    fn single_shot_completes_drops_and_rearms() {
        let mut rx = Port::new(RxMode::SingleShotDma);
        let mut first = [0; 4];
        let mut second = [0; 2];
        assert!(rx.start(first.as_mut_ptr(), first.len()));
        assert!(!rx.start(second.as_mut_ptr(), second.len()));
        unsafe {
            assert_eq!(rx.receive(b"ab"), 2);
        }
        assert_eq!(rx.status().remaining, 2);
        unsafe {
            assert_eq!(rx.receive(b"cdef"), 2);
        }
        assert_eq!(&first, b"abcd");
        assert_eq!(rx.status().remaining, 0);
        assert_eq!(rx.status().active, 0);
        assert_eq!(rx.status().completed, 1);
        assert!(rx.dma.is_none());
        unsafe {
            assert_eq!(rx.receive(b"g"), 0);
        }
        assert_eq!(rx.status().dropped, 3);
        assert!(rx.start(second.as_mut_ptr(), second.len()));
        assert_eq!(rx.status().completed, 0);
        unsafe {
            assert_eq!(rx.receive(b"hi"), 2);
        }
        assert_eq!(&second, b"hi");
        assert_eq!(&first, b"abcd");
    }

    #[test]
    fn stop_invalidates_buffer_and_restart_resets_position() {
        let mut rx = Port::new(RxMode::CircularDma);
        let mut buffer = [0; 3];
        assert!(!rx.start(std::ptr::null_mut(), 3));
        assert!(!rx.start(buffer.as_mut_ptr(), 0));
        assert!(rx.start(buffer.as_mut_ptr(), 3));
        unsafe {
            rx.receive(b"a");
        }
        rx.stop();
        unsafe {
            assert_eq!(rx.receive(b"bc"), 0);
        }
        assert_eq!(buffer, [b'a', 0, 0]);
        assert_eq!(rx.status().remaining, 0);
        assert!(rx.start(buffer.as_mut_ptr(), 3));
        unsafe {
            rx.receive(b"d");
        }
        assert_eq!(buffer, [b'd', 0, 0]);
    }

    #[test]
    fn polling_is_bounded_nonblocking_and_preserves_unread_bytes() {
        let mut rx = Port::new(RxMode::Polling { capacity: 3 });
        let mut output = [0; 4];
        assert!(!rx.start(output.as_mut_ptr(), 4));
        assert_eq!(rx.read(&mut output), Some(0));
        unsafe {
            assert_eq!(rx.receive(b"abcd"), 3);
        }
        assert_eq!(rx.status().queued, 3);
        assert_eq!(rx.status().dropped, 1);
        assert_eq!(rx.read(&mut output[..2]), Some(2));
        assert_eq!(&output[..2], b"ab");
        unsafe {
            assert_eq!(rx.receive(b"ef"), 2);
        }
        assert_eq!(rx.read(&mut output), Some(3));
        assert_eq!(&output[..3], b"cef");
        assert_eq!(rx.read(&mut output), Some(0));
        assert_eq!(rx.status().queued, 0);
    }

    // Exercise the thread-local ABI registry as well as independent port state.
    #[test]
    fn abi_reset_validation_and_ports_are_independent() {
        reset();
        assert!(!configure(-1, RxMode::CircularDma));
        assert!(!configure(0, RxMode::Polling { capacity: 0 }));
        assert!(!rig_runtime_uart_configure_rx(0, 9, 0));
        assert!(configure(0, RxMode::CircularDma));
        assert!(!configure(0, RxMode::SingleShotDma));
        assert!(configure(1, RxMode::Polling { capacity: 2 }));
        let mut buffer = [0; 4];
        unsafe {
            assert!(rig_runtime_uart_start_rx(0, buffer.as_mut_ptr(), 4));
            assert_eq!(rig_uart_receive(99, b"x".as_ptr(), 1), -1);
            assert_eq!(rig_uart_receive(0, std::ptr::null(), 1), -1);
            assert_eq!(rig_uart_receive(0, std::ptr::null(), 0), 0);
            assert_eq!(rig_uart_receive(1, b"xy".as_ptr(), 2), 2);
            assert_eq!(rig_runtime_uart_rx_remaining(0), 4);
            assert_eq!(buffer, [0; 4]);
        }
        unsafe {
            assert_eq!(rig_runtime_uart_read_rx(0, buffer.as_mut_ptr(), 4), -1);
            assert_eq!(rig_runtime_uart_read_rx(1, buffer.as_mut_ptr(), 4), 2);
        }
        assert_eq!(&buffer[..2], b"xy");
        rig_runtime_uart_stop_rx(0);
        unsafe {
            assert_eq!(rig_uart_receive(0, b"z".as_ptr(), 1), 0);
        }
        reset();
        unsafe {
            assert_eq!(rig_uart_receive(0, b"z".as_ptr(), 1), -1);
        }
        assert!(configure(0, RxMode::SingleShotDma));
        let mut status = RxStatus::default();
        unsafe {
            assert!(rig_uart_rx_status(0, &mut status));
        }
        assert_eq!(status.accepted, 0);
        assert_eq!(status.dropped, 0);
        assert_eq!(status.active, 0);
        reset();
    }
}
