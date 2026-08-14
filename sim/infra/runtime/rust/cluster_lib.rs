#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_can_notify_rx(_bus: u8) {}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_get_time_ns() -> u64 {
    cluster::rig_cluster_elapsed_ns()
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_set_digital_io(channel: i32, state: bool) {
    io::set_digital(channel, state);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_get_digital_io(channel: i32) -> bool {
    io::get_digital(channel)
}

mod ffi {
    include!(env!("RIG_RUNTIME_RUST_FFI_RS"));
}

pub mod can {
    include!(env!("RIG_RUNTIME_RUST_CAN_RS"));
}

pub mod datapath {
    include!(env!("RIG_RUNTIME_RUST_DATAPATH_RS"));
}

mod io {
    use std::sync::{LazyLock, Mutex};

    static DIGITAL_IO: LazyLock<Mutex<Vec<bool>>> = LazyLock::new(|| Mutex::new(Vec::new()));

    pub fn set_digital(channel: i32, state: bool) {
        if channel < 0 {
            return;
        }
        let mut digital_io = DIGITAL_IO.lock().unwrap();
        let channel = channel as usize;
        if channel >= digital_io.len() {
            digital_io.resize(channel + 1, false);
        }
        digital_io[channel] = state;
    }

    pub fn get_digital(channel: i32) -> bool {
        if channel < 0 {
            return false;
        }
        DIGITAL_IO
            .lock()
            .unwrap()
            .get(channel as usize)
            .copied()
            .unwrap_or(false)
    }
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

pub mod spi {
    include!(env!("RIG_RUNTIME_RUST_SPI_RS"));
}

mod networks {
    include!(env!("RIG_RUNTIME_RUST_NETWORKS_RS"));
}

pub mod cluster {
    include!(env!("RIG_RUNTIME_RUST_CLUSTER_RS"));
}
