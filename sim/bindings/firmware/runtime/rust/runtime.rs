// Firmware runtime composition for embedded model crates.
//
// Peripheral behavior and its C ABI live in their owning bindings. This
// module only assembles those bindings with the generic Rig runtime and
// exposes the firmware lifecycle types used by controller models.

include!(env!("RIG_RUNTIME_RUST_MODULES_RS"));

mod reset {
    unsafe extern "C" {
        fn rig_runtime_reset();
    }

    pub fn reset() {
        unsafe { rig_runtime_reset() };
    }
}

pub mod faults {
    include!(env!("RIG_RUNTIME_RUST_FAULTS_RS"));
}

pub mod io {
    include!(env!("RIG_RUNTIME_RUST_IO_RS"));
}

pub mod nvm {
    include!(env!("RIG_RUNTIME_RUST_NVM_RS"));
}

pub mod rt_controller {
    include!(env!("RIG_RUNTIME_RUST_RT_CONTROLLER_RS"));
}

pub use can::{
    CanEnumValueDescriptor, CanEvent, CanMessageDescriptor, CanNetwork, CanPacket,
    CanSignalDescriptor, CanSignalValue,
};
pub use model::{NodeModel, NodeTarget};
pub use rt_controller::{
    AppDesc, MAX_PERIODIC_TASKS, ModuleDesc, ModuleTask, PeriodicTask, RTController, Scheduler,
    TaskCallbacks, TaskFn,
};
