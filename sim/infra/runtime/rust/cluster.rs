use std::collections::VecDeque;
use std::mem;
use std::sync::Mutex;

pub type ClusterNodeRunForFn = unsafe extern "C" fn(u64);
pub type ClusterNodeFastForwardForFn = unsafe extern "C" fn(u64);
pub type ClusterNodeNextStepFn = unsafe extern "C" fn(u64) -> u64;
pub type ClusterNodeResetFn = unsafe extern "C" fn();
pub type ClusterRouteFn = unsafe extern "C" fn(u64);
pub type ClusterCanTxCountFn = unsafe extern "C" fn(u8) -> u32;
pub type ClusterCanRecvEventsFn = unsafe extern "C" fn(u8, *mut CanEvent, u32) -> u32;
pub type ClusterCanSendManyFn = unsafe extern "C" fn(u8, *const CanPacket, u32) -> u32;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanPacket {
    pub id: u32,
    pub len: u8,
    pub data: [u8; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CanEvent {
    pub bus: u8,
    pub timestamp_ns: u64,
    pub packet: CanPacket,
}

#[derive(Clone, Copy)]
struct ClusterNode {
    run_for: ClusterNodeRunForFn,
    fast_forward_for: ClusterNodeFastForwardForFn,
    next_step: ClusterNodeNextStepFn,
    reset: ClusterNodeResetFn,
    online: bool,
    elapsed_ns: u64,
}

#[derive(Clone, Copy)]
struct ClusterCanRoute {
    source_node: u32,
    source_bus: u8,
    source_tx_count: ClusterCanTxCountFn,
    source_recv_events: ClusterCanRecvEventsFn,
    sink_node: u32,
    sink_bus: u8,
    sink_send_many: ClusterCanSendManyFn,
}

#[derive(Clone, Copy)]
struct ClusterCanRecord {
    source_node: u32,
    bus: u8,
    event: CanEvent,
}

#[derive(Default)]
struct ClusterRuntime {
    nodes: Vec<ClusterNode>,
    can_routes: Vec<ClusterCanRoute>,
    can_records: VecDeque<ClusterCanRecord>,
    elapsed_ns: u64,
}

impl ClusterRuntime {
    fn reset(&mut self) {
        self.nodes.clear();
        self.can_routes.clear();
        self.can_records.clear();
        self.elapsed_ns = 0;
    }

    fn add_node(
        &mut self,
        run_for: ClusterNodeRunForFn,
        fast_forward_for: ClusterNodeFastForwardForFn,
        next_step: ClusterNodeNextStepFn,
        reset: ClusterNodeResetFn,
        online: bool,
    ) -> u32 {
        self.nodes.push(ClusterNode {
            run_for,
            fast_forward_for,
            next_step,
            reset,
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_can_route(&mut self, route: ClusterCanRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.can_routes.push(route);
        true
    }

    fn set_node_online(&mut self, node: u32, online: bool) -> bool {
        let Some(node) = self.nodes.get_mut(node as usize) else {
            return false;
        };
        if node.online && !online {
            unsafe { (node.reset)() };
            node.elapsed_ns = 0;
        }
        node.online = online;
        true
    }

    fn next_cluster_step(&self, max_step_ns: u64) -> u64 {
        self.nodes
            .iter()
            .filter(|node| node.online)
            .map(|node| unsafe { (node.next_step)(max_step_ns) })
            .min()
            .unwrap_or(max_step_ns)
            .min(max_step_ns)
    }

    fn run_next_step(&mut self, remaining_ns: u64, max_step_ns: u64, fast_forward: bool) -> u64 {
        if remaining_ns == 0 || max_step_ns == 0 {
            return 0;
        }

        let max_delta_ns = remaining_ns.min(max_step_ns);
        let delta_ns = if fast_forward {
            max_delta_ns
        } else {
            self.next_cluster_step(max_delta_ns)
        };
        if delta_ns == 0 {
            return 0;
        }

        for node in self.nodes.iter_mut().filter(|node| node.online) {
            if fast_forward {
                unsafe { (node.fast_forward_for)(delta_ns) };
            } else {
                unsafe { (node.run_for)(delta_ns) };
            }
            node.elapsed_ns = node.elapsed_ns.saturating_add(delta_ns);
        }

        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
        self.route_can();
        delta_ns
    }

    fn route_can(&mut self) {
        for route in self.can_routes.iter().copied() {
            let Some(source) = self.nodes.get(route.source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let pending = unsafe { (route.source_tx_count)(route.source_bus) };
            if pending == 0 {
                continue;
            }

            let mut events = vec![CanEvent::default(); pending as usize];
            let count = unsafe {
                (route.source_recv_events)(route.source_bus, events.as_mut_ptr(), pending)
            };
            let count = count.min(pending) as usize;
            if count == 0 {
                continue;
            }
            events.truncate(count);

            let packets: Vec<CanPacket> = events.iter().map(|event| event.packet).collect();
            if !packets.is_empty() {
                unsafe {
                    (route.sink_send_many)(
                        route.sink_bus,
                        packets.as_ptr(),
                        packets.len().min(u32::MAX as usize) as u32,
                    )
                };
            }

            self.can_records
                .extend(events.into_iter().map(|event| ClusterCanRecord {
                    source_node: route.source_node,
                    bus: route.source_bus,
                    event,
                }));
        }
    }

    fn node_elapsed_ns(&self, node: u32) -> u64 {
        self.nodes
            .get(node as usize)
            .map(|node| node.elapsed_ns)
            .unwrap_or(0)
    }

    fn node_elapsed_ns_many(&self, out: &mut [u64]) -> u32 {
        let count = self.nodes.len().min(out.len()).min(u32::MAX as usize);
        for (slot, node) in out.iter_mut().zip(self.nodes.iter()).take(count) {
            *slot = node.elapsed_ns;
        }
        count as u32
    }

    fn latest_can_message(&self, source_node: u32, bus: u8, message_id: u32) -> Option<CanEvent> {
        self.can_records
            .iter()
            .rev()
            .find(|record| {
                record.source_node == source_node
                    && record.bus == bus
                    && record.event.packet.id == message_id
            })
            .map(|record| record.event)
    }

    fn latest_can_bus_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        self.can_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.bus == bus)
            .map(|record| record.event)
    }
}

static CLUSTER_RUNTIME: Mutex<ClusterRuntime> = Mutex::new(ClusterRuntime {
    nodes: Vec::new(),
    can_routes: Vec::new(),
    can_records: VecDeque::new(),
    elapsed_ns: 0,
});

unsafe fn function_pointer<T>(address: usize) -> Option<T> {
    if address == 0 {
        return None;
    }
    Some(unsafe { mem::transmute_copy(&address) })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_reset() {
    CLUSTER_RUNTIME.lock().unwrap().reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_node(
    run_for: usize,
    fast_forward_for: usize,
    next_step: usize,
    reset: usize,
    online: bool,
) -> u32 {
    let Some(run_for) = (unsafe { function_pointer::<ClusterNodeRunForFn>(run_for) }) else {
        return u32::MAX;
    };
    let Some(fast_forward_for) =
        (unsafe { function_pointer::<ClusterNodeFastForwardForFn>(fast_forward_for) })
    else {
        return u32::MAX;
    };
    let Some(next_step) = (unsafe { function_pointer::<ClusterNodeNextStepFn>(next_step) }) else {
        return u32::MAX;
    };
    let Some(reset) = (unsafe { function_pointer::<ClusterNodeResetFn>(reset) }) else {
        return u32::MAX;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_node(run_for, fast_forward_for, next_step, reset, online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_set_node_online(node: u32, online: bool) -> bool {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .set_node_online(node, online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_can_route(
    source_node: u32,
    source_bus: u8,
    source_tx_count: usize,
    source_recv_events: usize,
    sink_node: u32,
    sink_bus: u8,
    sink_send_many: usize,
) -> bool {
    let Some(source_tx_count) =
        (unsafe { function_pointer::<ClusterCanTxCountFn>(source_tx_count) })
    else {
        return false;
    };
    let Some(source_recv_events) =
        (unsafe { function_pointer::<ClusterCanRecvEventsFn>(source_recv_events) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterCanSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME.lock().unwrap().add_can_route(ClusterCanRoute {
        source_node,
        source_bus,
        source_tx_count,
        source_recv_events,
        sink_node,
        sink_bus,
        sink_send_many,
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_for(
    duration_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
) {
    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(duration_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_elapsed_ns() -> u64 {
    CLUSTER_RUNTIME.lock().unwrap().elapsed_ns
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_node_elapsed_ns(node: u32) -> u64 {
    CLUSTER_RUNTIME.lock().unwrap().node_elapsed_ns(node)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_node_elapsed_ns_many(out: *mut u64, capacity: u32) -> u32 {
    if out.is_null() {
        return 0;
    }
    let out = unsafe { std::slice::from_raw_parts_mut(out, capacity as usize) };
    CLUSTER_RUNTIME.lock().unwrap().node_elapsed_ns_many(out)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_can_message(
    source_node: u32,
    bus: u8,
    message_id: u32,
    out: *mut CanEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(event) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_can_message(source_node, bus, message_id)
    else {
        return false;
    };
    unsafe { *out = event };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_can_bus_event(
    source_node: u32,
    bus: u8,
    out: *mut CanEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(event) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_can_bus_event(source_node, bus)
    else {
        return false;
    };
    unsafe { *out = event };
    true
}
