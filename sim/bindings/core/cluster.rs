use std::ffi::CStr;
use std::mem;
use std::os::raw::c_char;
use std::sync::{LazyLock, Mutex};

use super::dataflow::{DataflowAlgorithm, DataflowWait};
use super::registry::{
    CanEvent, CanPacket, CanSignalComparison, CanSignalDecoderFn, CanSignalWake,
    CanSignalWakeCallback,
    ClusterCanRoute, ClusterSpiRoute,
    ClusterTimerRoute, InterfaceRoute, RuntimeInterface, RuntimeInterfaces,
    SpiTransaction, TimerChannelEvent,
};
use super::interfaces::{InterfaceCaller, InterfaceImplementation};
use super::runtime::{RigBackend, RigRuntime};
use super::node::{RigNodeResetFn, RigNodeRunForFn, RigPythonScheduledFn};
use super::scheduler;
use super::scalar::{self, ScalarCountFn, ScalarRecvManyFn, ScalarRoute, ScalarSendManyFn,
    ScalarSink, ScalarSinkSetFn, ScalarEvent};

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
#[derive(Default)]
pub(super) struct FirmwareBackend {
    pub(super) interfaces: RuntimeInterfaces,
    pub(super) scalar: scalar::ScalarInterface,
}

impl RigBackend for FirmwareBackend {
    fn reset(&mut self) {
        self.interfaces.reset();
        self.scalar.reset_interface();
    }

    fn reset_node(&mut self, node: u32) {
        self.interfaces.reset_node_interfaces(node);
    }

    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        self.interfaces.append_algorithm_specs(specs);
        self.scalar.append_algorithm_specs(specs);
    }

    fn scalar_state_ready(&mut self, node: u32, route_id: u32, value: f32) {
        self.interfaces.timer.update_scaled_scalar_scale(node, route_id, value);
    }

    fn scalar_interface(&self) -> &scalar::ScalarInterface {
        &self.scalar
    }

    fn scalar_interface_mut(&mut self) -> &mut scalar::ScalarInterface {
        &mut self.scalar
    }
}

impl RigRuntime<FirmwareBackend> {
    fn register_route(&mut self, route: InterfaceRoute) -> bool {
        let (source_node, sink_node) = route.nodes();
        if self.nodes.get(source_node as usize).is_none()
            || sink_node.is_some_and(|node| self.nodes.get(node as usize).is_none())
        {
            return false;
        }
        self.interfaces.register_route(route);
        self.scheduler.mark_dirty();
        true
    }

    fn send_native_can_source_event(&mut self, node: u32, bus: u8, packet: CanPacket) -> bool {
        if !self.node_exists(node) {
            return false;
        }
        let elapsed_ns = self.elapsed_ns;
        self.interfaces
            .send_native_can_source_event(node, bus, elapsed_ns, packet);
        true
    }

    fn register_can_signal_wake(
        &mut self,
        wake: CanSignalWake,
        callback: CanSignalWakeCallback,
    ) -> bool {
        if !self.node_exists(wake.source_node) {
            return false;
        }
        let registered = self.interfaces.can.register_signal_wake(wake, callback);
        if registered {
            self.scheduler.mark_dirty();
        }
        registered
    }

    fn begin_can_signal_wait(
        &mut self,
        source_node: u32,
        comparisons: &[CanSignalComparison],
        decoder: CanSignalDecoderFn,
    ) -> DataflowWait {
        let wait = self.scheduler.begin_dataflow_wait();
        let matched = self
            .interfaces
            .begin_can_signal_wait(source_node, comparisons, decoder, wait);
        if matched {
            self.scheduler.complete_dataflow_wait(wait);
        }
        self.scheduler.mark_dirty();
        wait
    }

    pub(crate) fn latest_can_message(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
    ) -> Option<CanEvent> {
        self.interfaces
            .latest_can_message(source_node, bus, message_id)
    }

    fn latest_can_bus_event(&self, source_node: u32, bus: u8) -> Option<CanEvent> {
        self.interfaces.latest_can_event(source_node, bus)
    }

    pub(crate) fn latest_can_signal(
        &self,
        source_node: u32,
        bus: u8,
        message_id: u32,
        signal_name: &str,
    ) -> Option<f64> {
        let event = self.latest_can_message(source_node, bus, message_id)?;
        let packet = CanPacket {
            id: event.packet.id,
            len: event.packet.len,
            data: event.packet.data,
        };
        self.interfaces.decode_can_signal(bus, &packet, signal_name)
    }

    fn latest_timer_event(
        &self,
        source_node: u32,
        interface: u16,
        port: i32,
        channel: i32,
    ) -> Option<TimerChannelEvent> {
        self.interfaces
            .latest_timer_event(source_node, interface, port, channel)
    }

}

static CLUSTER_RUNTIME: LazyLock<Mutex<RigRuntime<FirmwareBackend>>> =
    LazyLock::new(|| Mutex::new(RigRuntime::<FirmwareBackend>::default()));

pub(super) fn with_runtime<R>(f: impl FnOnce(&mut RigRuntime<FirmwareBackend>) -> R) -> R {
    let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
    f(&mut runtime)
}

pub fn add_periodic_scalar_source(
    node: u32,
    route_id: u32,
    period_ns: u64,
    reader: fn() -> f32,
) -> bool {
    with_runtime(|runtime| {
        super::simple::add_periodic_scalar_source(runtime, node, route_id, period_ns, reader)
    })
}

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
pub extern "C" fn rig_cluster_add_scalar_transform_algorithm(
    owner_node: u32,
    sort_index: u32,
    input_route_id: u32,
    output_route_id: u32,
) -> bool {
    let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
    scheduler::add_dataflow_algorithm(
        &mut *runtime,
        scalar::test_scalar_transform_algorithm(
            owner_node,
            sort_index,
            input_route_id,
            output_route_id,
        ),
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_compile_dataflow_graph() -> bool {
    scheduler::compile_dataflow_graph(&mut *CLUSTER_RUNTIME.lock().unwrap())
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_node(run_for: usize, reset: usize, online: bool) -> u32 {
    let Some(run_for) = (unsafe { function_pointer::<RigNodeRunForFn>(run_for) }) else {
        return u32::MAX;
    };
    let Some(reset) = (unsafe { function_pointer::<RigNodeResetFn>(reset) }) else {
        return u32::MAX;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_node(run_for, reset, online)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_python_node(
    scheduled: usize,
    reset: usize,
    period_ns: u64,
    online: bool,
) -> u32 {
    let scheduled = unsafe { function_pointer::<RigPythonScheduledFn>(scheduled) };
    let Some(reset) = (unsafe { function_pointer::<RigNodeResetFn>(reset) }) else {
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
        .register_route(InterfaceRoute::Can(ClusterCanRoute {
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
        }))
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
        .register_route(InterfaceRoute::Timer(ClusterTimerRoute {
            source_node,
            interface,
            port,
            channel,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        }))
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_timer_source(
    source_node: u32,
    interface: u16,
    port: i32,
    channel: i32,
    source_count: usize,
    source_recv_many: usize,
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

    CLUSTER_RUNTIME.lock().unwrap().register_route(InterfaceRoute::Timer(
        ClusterTimerRoute {
            source_node,
            interface,
            port,
            channel,
            source_count,
            source_recv_many,
            sink_node: u32::MAX,
            sink_send_many: rig_cluster_noop_timer_send_many,
        },
    ))
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
        .register_route(InterfaceRoute::Spi(ClusterSpiRoute {
            source_node,
            device,
            source_count,
            source_recv_many,
            sink_node,
            sink_send_many,
        }))
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
    let Some(source_count) = (unsafe { function_pointer::<ScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(sink_send_many) =
        (unsafe { function_pointer::<ScalarSendManyFn>(sink_send_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .register_scalar_route(ScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ScalarSink::SendMany(sink_send_many),
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_sink_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_id: i32,
    value_scale: f32,
    set_value: usize,
) -> bool {
    if !value_scale.is_finite() {
        return false;
    }
    let Some(source_count) = (unsafe { function_pointer::<ScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    let Some(set_value) = (unsafe { function_pointer::<ScalarSinkSetFn>(set_value) }) else {
        return false;
    };

    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .register_scalar_route(ScalarRoute {
            source_node,
            route_id,
            source_count,
            source_recv_many,
            sink_node,
            sink: ScalarSink::Native {
                sink_id,
                value_scale,
                set_value,
            },
        })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_state_sink(
    node: u32,
    route_id: u32,
    initial_value: f32,
) -> bool {
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .add_scalar_state_sink(node, route_id, initial_value)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_state_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_route_id: u32,
    sink_id: i32,
    value_scale: f32,
    set_value: usize,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ScalarCountFn>(source_count) })
    else {
        return false;
    };
    let set_value = if set_value == 0 {
        None
    } else {
        let Some(set_value) = (unsafe { function_pointer::<ScalarSinkSetFn>(set_value) })
        else {
            return false;
        };
        Some(set_value)
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };

    CLUSTER_RUNTIME.lock().unwrap().add_scalar_state_route(
        source_node,
        route_id,
        source_count,
        source_recv_many,
        sink_node,
        sink_route_id,
        sink_id,
        value_scale,
        set_value,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_input_route(
    source_node: u32,
    route_id: u32,
    source_count: usize,
    source_recv_many: usize,
    sink_node: u32,
    sink_route_id: u32,
) -> bool {
    let Some(source_count) = (unsafe { function_pointer::<ScalarCountFn>(source_count) })
    else {
        return false;
    };
    let Some(source_recv_many) =
        (unsafe { function_pointer::<ScalarRecvManyFn>(source_recv_many) })
    else {
        return false;
    };
    CLUSTER_RUNTIME.lock().unwrap().add_scalar_input_route(
        source_node,
        route_id,
        source_count,
        source_recv_many,
        sink_node,
        sink_route_id,
    )
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
    super::simple::add_periodic_can_source(
        &mut CLUSTER_RUNTIME.lock().unwrap(),
        node,
        bus,
        period_ns,
        unsafe { *packet },
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_update_periodic_can_source(
    handle: u32,
    packet: *const CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    super::simple::update_periodic_can_source(
        &mut CLUSTER_RUNTIME.lock().unwrap(),
        handle,
        unsafe { *packet },
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_send_native_can_source_event(
    node: u32,
    bus: u8,
    packet: *const CanPacket,
) -> bool {
    if packet.is_null() {
        return false;
    }
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .send_native_can_source_event(node, bus, unsafe { *packet })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_register_can_signal_wake(
    wake: *const CanSignalWake,
    callback: usize,
) -> bool {
    if wake.is_null() {
        return false;
    }
    let Some(callback) = (unsafe { function_pointer::<CanSignalWakeCallback>(callback) }) else {
        return false;
    };
    CLUSTER_RUNTIME
        .lock()
        .unwrap()
        .register_can_signal_wake(unsafe { *wake }, callback)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_can_tx_count(_bus: u8) -> u32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_noop_can_recv_events(
    _bus: u8,
    _events: *mut CanEvent,
    _capacity: u32,
) -> u32 {
    0
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
pub extern "C" fn rig_cluster_run_for(duration_ns: u64, max_step_ns: u64, route: usize) {
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
            let delta_ns = scheduler::run_next_step(&mut *runtime, remaining_ns, max_step_ns);
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

fn begin_can_signal_wait(
    source_node: u32,
    comparisons: &[CanSignalComparison],
    decoder: usize,
) -> u64 {
    if comparisons.is_empty() || decoder == 0 {
        return u64::MAX;
    }
    let decoder = unsafe { std::mem::transmute::<usize, CanSignalDecoderFn>(decoder) };
    let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
    runtime
        .begin_can_signal_wait(source_node, comparisons, decoder)
        .0
}

fn run_until_dataflow_wait(
    timeout_ns: u64,
    max_step_ns: u64,
    route: usize,
    wait: DataflowWait,
) -> u64 {
    let route = unsafe { function_pointer::<ClusterRouteFn>(route) };
    let (current_elapsed_ns, matched) = {
        let runtime = CLUSTER_RUNTIME.lock().unwrap();
        (runtime.elapsed_ns, runtime.dataflow_wait_matched(wait))
    };
    if matched {
        CLUSTER_RUNTIME.lock().unwrap().cancel_dataflow_wait(wait);
        return 0;
    }
    if timeout_ns == 0 || max_step_ns == 0 {
        CLUSTER_RUNTIME.lock().unwrap().cancel_dataflow_wait(wait);
        return u64::MAX;
    }

    let target_elapsed_ns = current_elapsed_ns.saturating_add(timeout_ns);
    let mut next_route_elapsed_ns = current_elapsed_ns.saturating_add(max_step_ns);
    loop {
        let (delta_ns, elapsed_ns, matched) = {
            let mut runtime = CLUSTER_RUNTIME.lock().unwrap();
            if runtime.elapsed_ns >= target_elapsed_ns {
                runtime.cancel_dataflow_wait(wait);
                return u64::MAX;
            }
            let remaining_ns = target_elapsed_ns - runtime.elapsed_ns;
            let delta_ns = scheduler::run_next_step(&mut *runtime, remaining_ns, max_step_ns);
            let elapsed_ns = runtime.elapsed_ns;
            let matched = runtime.dataflow_wait_matched(wait);
            (delta_ns, elapsed_ns, matched)
        };

        if delta_ns == 0 {
            CLUSTER_RUNTIME.lock().unwrap().cancel_dataflow_wait(wait);
            return u64::MAX;
        }
        if matched {
            CLUSTER_RUNTIME.lock().unwrap().cancel_dataflow_wait(wait);
            return elapsed_ns.saturating_sub(current_elapsed_ns);
        }

        if let Some(route) = route {
            if elapsed_ns >= next_route_elapsed_ns || elapsed_ns >= target_elapsed_ns {
                unsafe { route(elapsed_ns) };
                next_route_elapsed_ns = elapsed_ns.saturating_add(max_step_ns);
                if CLUSTER_RUNTIME.lock().unwrap().dataflow_wait_matched(wait) {
                    CLUSTER_RUNTIME.lock().unwrap().cancel_dataflow_wait(wait);
                    return elapsed_ns.saturating_sub(current_elapsed_ns);
                }
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_begin_can_signal_wait(
    source_node: u32,
    comparisons: *const CanSignalComparison,
    comparison_count: u32,
    decoder: usize,
) -> u64 {
    if comparisons.is_null() {
        return u64::MAX;
    }
    let comparisons = unsafe { std::slice::from_raw_parts(comparisons, comparison_count as usize) };
    begin_can_signal_wait(source_node, comparisons, decoder)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_run_until_dataflow_wait(
    timeout_ns: u64,
    max_step_ns: u64,
    route: usize,
    wait_id: u64,
) -> u64 {
    if wait_id == u64::MAX {
        return u64::MAX;
    }
    run_until_dataflow_wait(
        timeout_ns,
        max_step_ns,
        route,
        DataflowWait(wait_id),
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_cancel_dataflow_wait(wait_id: u64) {
    if wait_id != u64::MAX {
        CLUSTER_RUNTIME
            .lock()
            .unwrap()
            .cancel_dataflow_wait(DataflowWait(wait_id));
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
