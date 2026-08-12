use std::collections::VecDeque;
use std::ffi::CStr;
use std::mem;
use std::os::raw::c_char;
use std::sync::Mutex;

use super::can;
use super::dc_load::DcLoadModel;
use super::simple::PeriodicCanSource;

pub type ClusterNodeRunForFn = unsafe extern "C" fn(u64);
pub type ClusterNodeFastForwardForFn = unsafe extern "C" fn(u64);
pub type ClusterNodeNextStepFn = unsafe extern "C" fn(u64) -> u64;
pub type ClusterNodeResetFn = unsafe extern "C" fn();
pub type ClusterPythonScheduledFn = unsafe extern "C" fn(*const SchedulerCallbackContext);
pub type ClusterRouteFn = unsafe extern "C" fn(u64);
pub type ClusterCanTxCountFn = unsafe extern "C" fn(u8) -> u32;
pub type ClusterCanRecvEventsFn = unsafe extern "C" fn(u8, *mut CanEvent, u32) -> u32;
pub type ClusterCanSendManyFn = unsafe extern "C" fn(u8, *const CanPacket, u32) -> u32;
pub type ClusterTimerCountFn = unsafe extern "C" fn(i32, i32) -> u32;
pub type ClusterTimerRecvManyFn =
    unsafe extern "C" fn(i32, i32, *mut TimerChannelEvent, u32) -> u32;
pub type ClusterTimerSendManyFn = unsafe extern "C" fn(*const TimerChannelEvent, u32) -> u32;
pub type ClusterSpiCountFn = unsafe extern "C" fn(i32) -> u32;
pub type ClusterSpiRecvManyFn = unsafe extern "C" fn(i32, *mut SpiTransaction, u32) -> u32;
pub type ClusterSpiSendManyFn = unsafe extern "C" fn(*const SpiTransaction, u32) -> u32;
pub type ClusterScalarCountFn = unsafe extern "C" fn() -> u32;
pub type ClusterScalarRecvManyFn = unsafe extern "C" fn(*mut ScalarEvent, u32) -> u32;
pub type ClusterScalarSendManyFn = unsafe extern "C" fn(*const ScalarEvent, u32) -> u32;

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

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TimerChannelEvent {
    pub port: i32,
    pub channel: i32,
    pub value: f32,
    pub timestamp_ns: u64,
}

pub const RIG_SPI_TRANSACTION_MAX_BYTES: usize = 256;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SpiTransaction {
    pub device: i32,
    pub tx_len: u16,
    pub rx_len: u16,
    pub tx_data: [u8; RIG_SPI_TRANSACTION_MAX_BYTES],
    pub rx_data: [u8; RIG_SPI_TRANSACTION_MAX_BYTES],
    pub timestamp_ns: u64,
}

impl Default for SpiTransaction {
    fn default() -> Self {
        Self {
            device: 0,
            tx_len: 0,
            rx_len: 0,
            tx_data: [0; RIG_SPI_TRANSACTION_MAX_BYTES],
            rx_data: [0; RIG_SPI_TRANSACTION_MAX_BYTES],
            timestamp_ns: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ScalarEvent {
    pub value: f32,
    pub timestamp_ns: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SchedulerCallbackContext {
    pub elapsed_ns: u64,
    pub delta_ns: u64,
    pub fast_forward: bool,
}

#[derive(Clone, Copy)]
enum ClusterNodeScheduler {
    RustRuntimeModel,
    External {
        run_for: ClusterNodeRunForFn,
        fast_forward_for: ClusterNodeFastForwardForFn,
        next_step: ClusterNodeNextStepFn,
    },
    Python {
        scheduled: Option<ClusterPythonScheduledFn>,
        period_ns: u64,
        next_due_ns: u64,
        input_pending: bool,
    },
}

#[derive(Clone, Copy)]
struct ClusterNode {
    scheduler: ClusterNodeScheduler,
    reset: Option<ClusterNodeResetFn>,
    online: bool,
    elapsed_ns: u64,
}

impl ClusterNode {
    fn next_step(&self, cluster_elapsed_ns: u64, max_step_ns: u64) -> u64 {
        match self.scheduler {
            ClusterNodeScheduler::RustRuntimeModel => max_step_ns,
            ClusterNodeScheduler::External { next_step, .. } => unsafe { next_step(max_step_ns) },
            ClusterNodeScheduler::Python {
                period_ns,
                next_due_ns,
                input_pending,
                ..
            } => {
                if input_pending || period_ns == 0 {
                    max_step_ns
                } else if next_due_ns <= cluster_elapsed_ns {
                    0
                } else {
                    (next_due_ns - cluster_elapsed_ns).min(max_step_ns)
                }
            }
        }
    }

    fn run_for(&mut self, delta_ns: u64, fast_forward: bool) {
        match self.scheduler {
            ClusterNodeScheduler::RustRuntimeModel => {}
            ClusterNodeScheduler::External {
                run_for,
                fast_forward_for,
                ..
            } => {
                if fast_forward {
                    unsafe { fast_forward_for(delta_ns) };
                } else {
                    unsafe { run_for(delta_ns) };
                }
            }
            ClusterNodeScheduler::Python { .. } => {}
        }
        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
    }

    fn mark_input_pending(&mut self) {
        if let ClusterNodeScheduler::Python { input_pending, .. } = &mut self.scheduler {
            *input_pending = true;
        }
    }

    fn run_due_python(&mut self, cluster_elapsed_ns: u64, fast_forward: bool) {
        let ClusterNodeScheduler::Python {
            scheduled,
            period_ns,
            next_due_ns,
            input_pending,
        } = &mut self.scheduler
        else {
            return;
        };

        let due_to_period = *period_ns != 0 && *next_due_ns <= cluster_elapsed_ns;
        let due_to_input = *input_pending;
        if !due_to_period && !due_to_input {
            return;
        }

        if let Some(scheduled) = scheduled {
            let context = SchedulerCallbackContext {
                elapsed_ns: cluster_elapsed_ns,
                delta_ns: cluster_elapsed_ns.saturating_sub(self.elapsed_ns),
                fast_forward,
            };
            unsafe {
                scheduled(&context);
            };
        }
        self.elapsed_ns = cluster_elapsed_ns;
        *input_pending = false;
        if *period_ns != 0 {
            while *next_due_ns <= cluster_elapsed_ns {
                *next_due_ns = next_due_ns.saturating_add(*period_ns);
            }
        }
    }
}

#[derive(Clone, Copy)]
struct ClusterCanRoute {
    source_node: u32,
    source_bus: u8,
    source_tx_count: ClusterCanTxCountFn,
    source_recv_events: ClusterCanRecvEventsFn,
    sink_node: Option<u32>,
    sink_bus: u8,
    sink_send_many: Option<ClusterCanSendManyFn>,
}

#[derive(Clone, Copy)]
struct ClusterTimerRoute {
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: ClusterTimerCountFn,
    source_recv_many: ClusterTimerRecvManyFn,
    sink_node: u32,
    sink_send_many: ClusterTimerSendManyFn,
}

#[derive(Clone, Copy)]
struct ClusterSpiRoute {
    source_node: u32,
    device: i32,
    source_count: ClusterSpiCountFn,
    source_recv_many: ClusterSpiRecvManyFn,
    sink_node: u32,
    sink_send_many: ClusterSpiSendManyFn,
}

#[derive(Clone, Copy)]
struct ClusterScalarRoute {
    source_node: u32,
    route_id: u32,
    source_count: ClusterScalarCountFn,
    source_recv_many: ClusterScalarRecvManyFn,
    sink_node: u32,
    sink_send_many: ClusterScalarSendManyFn,
}

#[derive(Clone, Copy)]
struct ClusterCanRecord {
    source_node: u32,
    bus: u8,
    event: CanEvent,
}

#[derive(Clone, Copy)]
struct ClusterTimerRecord {
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    event: TimerChannelEvent,
}

#[derive(Clone, Copy)]
struct ClusterSpiRecord {
    source_node: u32,
    device: i32,
    transaction: SpiTransaction,
}

#[derive(Clone, Copy)]
struct ClusterScalarRecord {
    source_node: u32,
    route_id: u32,
    event: ScalarEvent,
}

#[derive(Default)]
struct ClusterRuntime {
    nodes: Vec<ClusterNode>,
    can_routes: Vec<ClusterCanRoute>,
    timer_routes: Vec<ClusterTimerRoute>,
    spi_routes: Vec<ClusterSpiRoute>,
    scalar_routes: Vec<ClusterScalarRoute>,
    periodic_can_sources: Vec<PeriodicCanSource>,
    dc_loads: Vec<DcLoadModel>,
    can_records: VecDeque<ClusterCanRecord>,
    timer_records: VecDeque<ClusterTimerRecord>,
    spi_records: VecDeque<ClusterSpiRecord>,
    scalar_records: VecDeque<ClusterScalarRecord>,
    elapsed_ns: u64,
}

impl ClusterRuntime {
    fn reset(&mut self) {
        self.nodes.clear();
        self.can_routes.clear();
        self.timer_routes.clear();
        self.spi_routes.clear();
        self.scalar_routes.clear();
        self.periodic_can_sources.clear();
        self.dc_loads.clear();
        self.can_records.clear();
        self.timer_records.clear();
        self.spi_records.clear();
        self.scalar_records.clear();
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
            scheduler: ClusterNodeScheduler::External {
                run_for,
                fast_forward_for,
                next_step,
            },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_rust_runtime_model_node(&mut self, online: bool) -> u32 {
        self.nodes.push(ClusterNode {
            scheduler: ClusterNodeScheduler::RustRuntimeModel,
            reset: None,
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_python_node(
        &mut self,
        scheduled: Option<ClusterPythonScheduledFn>,
        reset: ClusterNodeResetFn,
        period_ns: u64,
        online: bool,
    ) -> u32 {
        self.nodes.push(ClusterNode {
            scheduler: ClusterNodeScheduler::Python {
                scheduled,
                period_ns,
                next_due_ns: period_ns,
                input_pending: false,
            },
            reset: Some(reset),
            online,
            elapsed_ns: 0,
        });
        (self.nodes.len() - 1) as u32
    }

    fn add_can_route(&mut self, route: ClusterCanRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none() {
            return false;
        }
        if let Some(sink_node) = route.sink_node {
            if self.nodes.get(sink_node as usize).is_none() {
                return false;
            }
        }
        if route.sink_node.is_some() != route.sink_send_many.is_some() {
            return false;
        }
        self.can_routes.push(route);
        true
    }

    fn add_timer_route(&mut self, route: ClusterTimerRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.timer_routes.push(route);
        true
    }

    fn add_spi_route(&mut self, route: ClusterSpiRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.spi_routes.push(route);
        true
    }

    fn add_scalar_route(&mut self, route: ClusterScalarRoute) -> bool {
        if self.nodes.get(route.source_node as usize).is_none()
            || self.nodes.get(route.sink_node as usize).is_none()
        {
            return false;
        }
        self.scalar_routes.push(route);
        true
    }

    fn add_periodic_can_source(
        &mut self,
        node: u32,
        bus: u8,
        period_ns: u64,
        packet: CanPacket,
    ) -> u32 {
        if self.nodes.get(node as usize).is_none() || period_ns == 0 {
            return u32::MAX;
        }
        self.periodic_can_sources
            .push(PeriodicCanSource::new(node, bus, period_ns, packet));
        (self.periodic_can_sources.len() - 1) as u32
    }

    fn update_periodic_can_source(&mut self, handle: u32, packet: CanPacket) -> bool {
        let Some(source) = self.periodic_can_sources.get_mut(handle as usize) else {
            return false;
        };
        source.update_packet(packet);
        true
    }

    fn add_dc_load(
        &mut self,
        node: u32,
        current_route_id: u32,
        timer_interface: u16,
        timer_port: i32,
        timer_channel: i32,
        resistance_ohms: f32,
        inductance_henrys: f32,
        capacitance_farads: f32,
        scheduler_period_ns: u64,
    ) -> bool {
        if self.nodes.get(node as usize).is_none() {
            return false;
        }
        self.dc_loads.retain(|load| {
            !load.config_matches(
                node,
                current_route_id,
                timer_interface,
                timer_port,
                timer_channel,
            )
        });
        self.dc_loads.push(DcLoadModel::new(
            node,
            current_route_id,
            timer_interface,
            timer_port,
            timer_channel,
            resistance_ohms,
            inductance_henrys,
            capacitance_farads,
            scheduler_period_ns,
            self.elapsed_ns,
        ));
        true
    }

    fn set_node_online(&mut self, node_index: u32, online: bool) -> bool {
        let elapsed_ns = self.elapsed_ns;
        let reset_runtime_models;
        let Some(node) = self.nodes.get_mut(node_index as usize) else {
            return false;
        };
        if node.online && !online {
            if let Some(reset) = node.reset {
                unsafe { reset() };
            }
            node.elapsed_ns = 0;
            reset_runtime_models = matches!(node.scheduler, ClusterNodeScheduler::RustRuntimeModel);
            if let ClusterNodeScheduler::Python {
                period_ns,
                next_due_ns,
                input_pending,
                ..
            } = &mut node.scheduler
            {
                *next_due_ns = *period_ns;
                *input_pending = false;
            }
        } else {
            reset_runtime_models = false;
        }
        if !node.online && online {
            if let ClusterNodeScheduler::Python {
                period_ns,
                next_due_ns,
                input_pending,
                ..
            } = &mut node.scheduler
            {
                *next_due_ns = elapsed_ns.saturating_add(*period_ns);
                *input_pending = false;
            }
        }
        node.online = online;
        if reset_runtime_models {
            self.reset_rust_runtime_node_models(node_index, elapsed_ns);
        }
        true
    }

    fn reset_rust_runtime_node_models(&mut self, node: u32, elapsed_ns: u64) {
        for load in self.dc_loads.iter_mut().filter(|load| load.node() == node) {
            load.reset(elapsed_ns);
        }
    }

    fn next_cluster_step(&self, max_step_ns: u64) -> u64 {
        let node_step = self
            .nodes
            .iter()
            .filter(|node| node.online)
            .map(|node| node.next_step(self.elapsed_ns, max_step_ns))
            .min()
            .unwrap_or(max_step_ns);
        let dc_load_step = self
            .dc_loads
            .iter()
            .filter(|load| self.node_online(load.node()))
            .map(|load| load.next_step_ns(self.elapsed_ns, max_step_ns))
            .min()
            .unwrap_or(max_step_ns);
        node_step.min(dc_load_step).min(max_step_ns)
    }

    fn node_online(&self, node: u32) -> bool {
        self.nodes
            .get(node as usize)
            .map(|node| node.online)
            .unwrap_or(false)
    }

    fn run_next_step(&mut self, remaining_ns: u64, max_step_ns: u64, fast_forward: bool) -> u64 {
        if remaining_ns == 0 || max_step_ns == 0 {
            return 0;
        }

        self.run_due_python_nodes(fast_forward);

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
            node.run_for(delta_ns, fast_forward);
        }

        self.elapsed_ns = self.elapsed_ns.saturating_add(delta_ns);
        self.run_due_python_nodes(fast_forward);
        self.route_can();
        self.route_timer();
        self.run_dc_loads();
        self.route_spi();
        self.route_scalar();
        self.run_due_python_nodes(fast_forward);
        delta_ns
    }

    fn run_due_python_nodes(&mut self, fast_forward: bool) {
        let elapsed_ns = self.elapsed_ns;
        for node in self.nodes.iter_mut().filter(|node| node.online) {
            node.run_due_python(elapsed_ns, fast_forward);
        }
    }

    fn mark_input_pending(&mut self, node: u32) {
        let Some(node) = self.nodes.get_mut(node as usize) else {
            return;
        };
        node.mark_input_pending();
    }

    fn route_can(&mut self) {
        let routes = self.can_routes.clone();
        let mut routed_sources: Vec<(u32, u8)> = Vec::new();
        let nodes_online: Vec<bool> = self.nodes.iter().map(|node| node.online).collect();
        let mut input_pending_nodes = Vec::new();

        for source in self.periodic_can_sources.iter_mut() {
            if !nodes_online
                .get(source.node() as usize)
                .copied()
                .unwrap_or(false)
            {
                continue;
            }
            let Some(event) = source.emit_if_due(self.elapsed_ns) else {
                continue;
            };
            for sink_route in routes.iter().filter(|route| {
                route.source_node == source.node()
                    && route.source_bus == source.bus()
                    && route.sink_node.is_some()
            }) {
                if let (Some(sink_node), Some(sink_send_many)) =
                    (sink_route.sink_node, sink_route.sink_send_many)
                {
                    unsafe {
                        sink_send_many(sink_route.sink_bus, &event.packet, 1);
                    };
                    input_pending_nodes.push(sink_node);
                }
            }
            self.can_records.push_back(ClusterCanRecord {
                source_node: source.node(),
                bus: source.bus(),
                event,
            });
        }
        for sink_node in input_pending_nodes {
            self.mark_input_pending(sink_node);
        }

        for route in routes.iter().copied() {
            let source_key = (route.source_node, route.source_bus);
            if routed_sources.contains(&source_key) {
                continue;
            }
            routed_sources.push(source_key);

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
                for sink_route in routes.iter().filter(|sink_route| {
                    sink_route.source_node == route.source_node
                        && sink_route.source_bus == route.source_bus
                        && sink_route.sink_node.is_some()
                }) {
                    if let (Some(sink_node), Some(sink_send_many)) =
                        (sink_route.sink_node, sink_route.sink_send_many)
                    {
                        let accepted = unsafe {
                            sink_send_many(
                                sink_route.sink_bus,
                                packets.as_ptr(),
                                packets.len().min(u32::MAX as usize) as u32,
                            )
                        };
                        if accepted > 0 {
                            self.mark_input_pending(sink_node);
                        }
                    }
                }
            }

            self.can_records
                .extend(events.into_iter().map(|event| ClusterCanRecord {
                    source_node: route.source_node,
                    bus: route.source_bus,
                    event,
                }));
        }
    }

    fn route_timer(&mut self) {
        let routes = self.timer_routes.clone();
        let mut routed_sources: Vec<(u32, u16, i32, i32)> = Vec::new();

        for route in routes.iter().copied() {
            let source_key = (
                route.source_node,
                route.interface,
                route.port,
                route.channel,
            );
            if routed_sources.contains(&source_key) {
                continue;
            }
            routed_sources.push(source_key);

            let Some(source) = self.nodes.get(route.source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let pending = unsafe { (route.source_count)(route.port, route.channel) };
            if pending == 0 {
                continue;
            }

            let mut events = vec![TimerChannelEvent::default(); pending as usize];
            let count = unsafe {
                (route.source_recv_many)(route.port, route.channel, events.as_mut_ptr(), pending)
            };
            let count = count.min(pending) as usize;
            if count == 0 {
                continue;
            }
            events.truncate(count);

            for sink_route in routes.iter().filter(|sink_route| {
                sink_route.source_node == route.source_node
                    && sink_route.interface == route.interface
                    && sink_route.port == route.port
                    && sink_route.channel == route.channel
            }) {
                self.update_dc_load_voltage(sink_route, &events);
                let accepted = unsafe {
                    (sink_route.sink_send_many)(
                        events.as_ptr(),
                        events.len().min(u32::MAX as usize) as u32,
                    )
                };
                if accepted > 0 {
                    self.mark_input_pending(sink_route.sink_node);
                }
            }

            self.timer_records
                .extend(events.into_iter().map(|event| ClusterTimerRecord {
                    source_node: route.source_node,
                    interface: route.interface,
                    port: route.port,
                    channel: route.channel,
                    event,
                }));
        }
    }

    fn update_dc_load_voltage(&mut self, route: &ClusterTimerRoute, events: &[TimerChannelEvent]) {
        for load in self.dc_loads.iter_mut().filter(|load| {
            load.voltage_input_matches(route.sink_node, route.interface, route.port, route.channel)
        }) {
            load.update_voltage(events);
        }
    }

    fn run_dc_loads(&mut self) {
        let elapsed_ns = self.elapsed_ns;
        for load in self.dc_loads.iter_mut() {
            if !self
                .nodes
                .get(load.node() as usize)
                .map(|node| node.online)
                .unwrap_or(false)
            {
                continue;
            }
            load.run_until(elapsed_ns);
        }
    }

    fn route_spi(&mut self) {
        let routes = self.spi_routes.clone();
        let mut routed_sources: Vec<(u32, i32)> = Vec::new();

        for route in routes.iter().copied() {
            let source_key = (route.source_node, route.device);
            if routed_sources.contains(&source_key) {
                continue;
            }
            routed_sources.push(source_key);

            let Some(source) = self.nodes.get(route.source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let pending = unsafe { (route.source_count)(route.device) };
            if pending == 0 {
                continue;
            }

            let mut transactions = vec![SpiTransaction::default(); pending as usize];
            let count = unsafe {
                (route.source_recv_many)(route.device, transactions.as_mut_ptr(), pending)
            };
            let count = count.min(pending) as usize;
            if count == 0 {
                continue;
            }
            transactions.truncate(count);

            for sink_route in routes.iter().filter(|sink_route| {
                sink_route.source_node == route.source_node && sink_route.device == route.device
            }) {
                let accepted = unsafe {
                    (sink_route.sink_send_many)(
                        transactions.as_ptr(),
                        transactions.len().min(u32::MAX as usize) as u32,
                    )
                };
                if accepted > 0 {
                    self.mark_input_pending(sink_route.sink_node);
                }
            }

            self.spi_records.extend(
                transactions
                    .into_iter()
                    .map(|transaction| ClusterSpiRecord {
                        source_node: route.source_node,
                        device: route.device,
                        transaction,
                    }),
            );
        }
    }

    fn route_scalar(&mut self) {
        let routes = self.scalar_routes.clone();
        let mut routed_sources: Vec<(u32, u32)> = Vec::new();

        for route in routes.iter().copied() {
            let source_key = (route.source_node, route.route_id);
            if routed_sources.contains(&source_key) {
                continue;
            }
            routed_sources.push(source_key);

            let Some(source) = self.nodes.get(route.source_node as usize) else {
                continue;
            };
            if !source.online {
                continue;
            }

            let mut events = self.native_scalar_events(route.source_node, route.route_id);
            if events.is_empty() {
                let pending = unsafe { (route.source_count)() };
                if pending == 0 {
                    continue;
                }

                events = vec![ScalarEvent::default(); pending as usize];
                let count = unsafe { (route.source_recv_many)(events.as_mut_ptr(), pending) };
                let count = count.min(pending) as usize;
                if count == 0 {
                    continue;
                }
                events.truncate(count);
            }

            for sink_route in routes.iter().filter(|sink_route| {
                sink_route.source_node == route.source_node && sink_route.route_id == route.route_id
            }) {
                let accepted = unsafe {
                    (sink_route.sink_send_many)(
                        events.as_ptr(),
                        events.len().min(u32::MAX as usize) as u32,
                    )
                };
                if accepted > 0 {
                    self.mark_input_pending(sink_route.sink_node);
                }
            }

            self.scalar_records
                .extend(events.into_iter().map(|event| ClusterScalarRecord {
                    source_node: route.source_node,
                    route_id: route.route_id,
                    event,
                }));
        }
    }

    fn native_scalar_events(&mut self, source_node: u32, route_id: u32) -> Vec<ScalarEvent> {
        let mut events = Vec::new();
        for load in self
            .dc_loads
            .iter_mut()
            .filter(|load| load.node() == source_node && load.current_route_id() == route_id)
        {
            if let Some(event) = load.take_current_event(self.elapsed_ns) {
                events.push(event);
            }
        }
        events
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

    fn latest_can_signal(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
    ) -> Option<f64> {
        let event = self.latest_can_message(source_node, bus, message_id)?;
        let packet = can::CanPacket {
            id: event.packet.id,
            len: event.packet.len,
            data: event.packet.data,
        };
        can::decode_signal(bus, &packet, signal_name)
    }

    fn latest_can_signal_eq(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
        expected: f64,
        tolerance: f64,
    ) -> bool {
        let Some(value) = self.latest_can_signal(source_node, bus, message_id, signal_name) else {
            return false;
        };
        (value - expected).abs() <= tolerance
    }

    fn latest_timer_event(
        &self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
    ) -> Option<TimerChannelEvent> {
        self.timer_records
            .iter()
            .rev()
            .find(|record| {
                record.source_node == source_node
                    && record.interface == interface
                    && record.port == port
                    && record.channel == channel
            })
            .map(|record| record.event)
    }

    fn latest_spi_transaction(&self, source_node: u32, device: i32) -> Option<SpiTransaction> {
        self.spi_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.device == device)
            .map(|record| record.transaction)
    }

    fn latest_scalar_event(&self, source_node: u32, route_id: u32) -> Option<ScalarEvent> {
        self.scalar_records
            .iter()
            .rev()
            .find(|record| record.source_node == source_node && record.route_id == route_id)
            .map(|record| record.event)
    }
}

static CLUSTER_RUNTIME: Mutex<ClusterRuntime> = Mutex::new(ClusterRuntime {
    nodes: Vec::new(),
    can_routes: Vec::new(),
    timer_routes: Vec::new(),
    spi_routes: Vec::new(),
    scalar_routes: Vec::new(),
    periodic_can_sources: Vec::new(),
    dc_loads: Vec::new(),
    can_records: VecDeque::new(),
    timer_records: VecDeque::new(),
    spi_records: VecDeque::new(),
    scalar_records: VecDeque::new(),
    elapsed_ns: 0,
});

unsafe fn function_pointer<T>(address: usize) -> Option<T> {
    if address == 0 {
        return None;
    }
    Some(unsafe { mem::transmute_copy(&address) })
}

unsafe fn c_str_to_str<'a>(value: *const c_char) -> Option<&'a str> {
    if value.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(value) }.to_str().ok()
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
pub extern "C" fn rig_cluster_add_python_node(
    scheduled: usize,
    reset: usize,
    period_ns: u64,
    online: bool,
) -> u32 {
    let scheduled = unsafe { function_pointer::<ClusterPythonScheduledFn>(scheduled) };
    let Some(reset) = (unsafe { function_pointer::<ClusterNodeResetFn>(reset) }) else {
        return u32::MAX;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_python_node(scheduled, reset, period_ns, online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_rust_runtime_model_node(online: bool) -> u32 {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_rust_runtime_model_node(online)
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
    let sink_send_many = if sink_node == u32::MAX {
        if sink_send_many != 0 {
            return false;
        }
        None
    } else {
        let Some(sink_send_many) =
            (unsafe { function_pointer::<ClusterCanSendManyFn>(sink_send_many) })
        else {
            return false;
        };
        Some(sink_send_many)
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_can_route(ClusterCanRoute {
            source_node,
            source_bus,
            source_tx_count,
            source_recv_events,
            sink_node: if sink_node == u32::MAX {
                None
            } else {
                Some(sink_node)
            },
            sink_bus,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_timer_route(
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_send_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterTimerCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterTimerRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterTimerSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_timer_route(ClusterTimerRoute {
            source_node,
            interface,
            port,
            channel,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_spi_route(
    source_node: u32,
    device: i32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_send_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterSpiCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterSpiRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterSpiSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_spi_route(ClusterSpiRoute {
            source_node,
            device,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_send_many: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ClusterScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ClusterScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ClusterScalarSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_scalar_route(ClusterScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_periodic_can_source(
    node: u32,
    bus: u8,
    period_ns: u64,
    packet: *const CanPacket,
) -> u32 {
    if packet.is_null() {
        return u32::MAX;
    }
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_periodic_can_source(node, bus, period_ns, unsafe { *packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_update_periodic_can_source(
    handle: u32,
    packet: *const CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .update_periodic_can_source(handle, unsafe { *packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_dc_load(
    node: u32,
    current_route_id: u32,
    timer_interface: u16,
    timer_port: i32,
    timer_channel: i32,
    resistance_ohms: f32,
    inductance_henrys: f32,
    capacitance_farads: f32,
    scheduler_period_ns: u64,
) -> bool {
    CLUSTER_RUNTIME.lock().unwrap().add_dc_load(
        node,
        current_route_id,
        timer_interface,
        timer_port,
        timer_channel,
        resistance_ohms,
        inductance_henrys,
        capacitance_farads,
        scheduler_period_ns,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_timer_count(_port: i32, _channel: i32) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_timer_recv_many(
    _port: i32,
    _channel: i32,
    _events: *mut TimerChannelEvent,
    _capacity: u32,
) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_timer_send_many(
    _events: *const TimerChannelEvent,
    count: u32,
) -> u32 {
    count
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_scalar_count() -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_scalar_recv_many(
    _events: *mut ScalarEvent,
    _capacity: u32,
) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_scalar_send_many(
    _events: *const ScalarEvent,
    count: u32,
) -> u32 {
    count
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
pub extern "C" fn rig_cluster_run_until_can_signal_eq(
    timeout_ns: u64,
    max_step_ns: u64,
    fast_forward: bool,
    route: usize,
    source_node: u32,
    bus: u8,
    message_id: u32,
    signal_name: *const c_char,
    expected: f64,
    tolerance: f64,
) -> u64 {
    let Some(signal_name) = (unsafe { c_str_to_str(signal_name) }) else {
        return u64::MAX;
    };
    if timeout_ns == 0 || max_step_ns == 0 {
        return if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
        ) {
            0
        } else {
            u64::MAX
        };
    }

    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let current_elapsed_ns = CLUSTER_RUNTIME.lock().unwrap().elapsed_ns;
    let target_elapsed_ns = current_elapsed_ns.saturating_add(timeout_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);

    if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
        source_node,
        bus,
        message_id,
        signal_name,
        expected,
        tolerance,
    ) {
        return 0;
    }

    loop {
        let (delta_ns, elapsed_ns) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = runtime.run_next_step(remaining_ns, max_step_ns, fast_forward);
            (delta_ns, runtime.elapsed_ns)
        };

        if delta_ns == 0 {
            return u64::MAX;
        }

        if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
            if let Some(route) = route {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
            }
        }

        if CLUSTER_RUNTIME.lock().unwrap().latest_can_signal_eq(
            source_node,
            bus,
            message_id,
            signal_name,
            expected,
            tolerance,
        ) {
            return elapsed_ns.saturating_sub(current_elapsed_ns);
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
    let Some(event) =
        CLUSTER_RUNTIME
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

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_can_signal(
    source_node: u32,
    bus: u8,
    message_id: u32,
    signal_name: *const c_char,
    out: *mut f64,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(signal_name) = (unsafe { c_str_to_str(signal_name) }) else {
        return false;
    };
    let Some(value) = CLUSTER_RUNTIME.lock().unwrap().latest_can_signal(
        source_node,
        bus,
        message_id,
        signal_name,
    ) else {
        return false;
    };
    unsafe { *out = value };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_timer_event(
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    out: *mut TimerChannelEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(event) =
        CLUSTER_RUNTIME
            .lock()
            .unwrap()
            .latest_timer_event(source_node, interface, port, channel)
    else {
        return false;
    };
    unsafe { *out = event };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_spi_transaction(
    source_node: u32,
    device: i32,
    out: *mut SpiTransaction,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(transaction) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_spi_transaction(source_node, device)
    else {
        return false;
    };
    unsafe { *out = transaction };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_latest_scalar_event(
    source_node: u32,
    route_id: u32,
    out: *mut ScalarEvent,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(event) = CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .latest_scalar_event(source_node, route_id)
    else {
        return false;
    };
    unsafe { *out = event };
    true
}
