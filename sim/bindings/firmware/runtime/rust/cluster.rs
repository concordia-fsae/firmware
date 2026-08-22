use std::mem;
use std::sync::{LazyLock, Mutex};

use super::dataflow::{DataflowAlgorithm, DataflowWait};
use super::registry::{InterfaceRoute, RuntimeInterface, RuntimeInterfaces};
use super::interfaces::{InterfaceCaller, InterfaceImplementation};
use super::runtime::{RigBackend, RigRuntime};
use super::node::{RigNodeResetFn, RigNodeRunForFn, RigPythonScheduledFn};
use super::scheduler;
use super::scalar::{self, ScalarCountFn, ScalarRecvManyFn, ScalarRoute, ScalarSendManyFn,
    ScalarSink, ScalarSinkSetFn, ScalarEvent};

pub type ClusterRouteFn = unsafe extern "C" fn(u64);
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

    fn cancel_dataflow_wait(&mut self, wait: DataflowWait) {
        self.interfaces.cancel_dataflow_wait(wait);
    }

    fn reset_node(&mut self, node: u32) {
        self.interfaces.reset_node_interfaces(node);
    }

    fn append_algorithm_specs(&self, specs: &mut Vec<DataflowAlgorithm>) {
        self.interfaces.append_algorithm_specs(specs);
        self.scalar.append_algorithm_specs(specs);
    }

    fn scalar_state_ready(&mut self, node: u32, route_id: u32, value: f32) {
        self.update_scalar_state_scale(node, route_id, value);
    }

    fn scalar_interface(&self) -> &scalar::ScalarInterface {
        &self.scalar
    }

    fn scalar_interface_mut(&mut self) -> &mut scalar::ScalarInterface {
        &mut self.scalar
    }
}

impl RigRuntime<FirmwareBackend> {
    pub(super) fn register_route(&mut self, route: InterfaceRoute) -> bool {
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
        super::scalar_source::add_periodic_scalar_source(
            &mut *runtime,
            node,
            route_id,
            period_ns,
            reader,
        )
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_add_scalar_source_bank_route(
    node: u32,
    route_id: u32,
    period_ns: u64,
    initial_value: f32,
) -> bool {
    with_runtime(|runtime| {
        super::scalar_source::add_scalar_source_bank_route(
            &mut *runtime,
            node,
            route_id,
            period_ns,
            initial_value,
        )
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_cluster_set_scalar_source_bank_value(
    node: u32,
    route_id: u32,
    value: f32,
) -> bool {
    super::scalar_source::set_scalar_source_bank_value(node, route_id, value)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rig_cluster_publish_scalar_source_bank_events(
    node: u32,
    period_ns: u64,
    timestamp_ns: u64,
    route_ids: *const u32,
    values: *const f32,
    count: u32,
) -> bool {
    if route_ids.is_null() || values.is_null() || count == 0 {
        return false;
    }
    let route_ids = unsafe { std::slice::from_raw_parts(route_ids, count as usize) };
    let values = unsafe { std::slice::from_raw_parts(values, count as usize) };
    super::scalar_source::publish_scalar_source_bank_events(
        node,
        period_ns,
        timestamp_ns,
        route_ids,
        values,
    )
}

pub(super) unsafe fn function_pointer<T>(address: usize) -> Option<T> {
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
