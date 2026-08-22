use std::sync::atomic::{AtomicU64, Ordering};

static RUNTIME_TIME_NS: AtomicU64 = AtomicU64::new(0);

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_advance_time_ns(elapsed_ns: u64) {
    RUNTIME_TIME_NS.fetch_add(elapsed_ns, Ordering::Relaxed);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_get_time_ns() -> u64 {
    RUNTIME_TIME_NS.load(Ordering::Relaxed)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_notify_rx(_bus: u8) {}

include!(env!("RIG_RUNTIME_RUST_MODULES_RS"));

mod io {
    include!(env!("RIG_RUNTIME_RUST_IO_HOST_RS"));
}
