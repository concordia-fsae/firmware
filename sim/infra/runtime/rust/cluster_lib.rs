#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_notify_rx(_bus: u8) {}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_get_time_ns() -> u64 {
    cluster::rig_cluster_elapsed_ns()
}

mod ffi {
    include!(env!("RIG_RUNTIME_RUST_FFI_RS"));
}

pub mod can {
    include!(env!("RIG_RUNTIME_RUST_CAN_RS"));
}

pub mod battery_source {
    include!(env!("RIG_RUNTIME_RUST_BATTERY_SOURCE_RS"));
}

pub mod dc_load {
    include!(env!("RIG_RUNTIME_RUST_DC_LOAD_RS"));
}

pub mod simple {
    include!(env!("RIG_RUNTIME_RUST_SIMPLE_RS"));
}

pub mod cluster {
    include!(env!("RIG_RUNTIME_RUST_CLUSTER_RS"));
}
