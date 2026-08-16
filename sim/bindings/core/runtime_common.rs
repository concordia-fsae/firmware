mod ffi {
    include!(env!("RIG_RUNTIME_RUST_FFI_RS"));
}

pub mod can {
    include!(env!("RIG_RUNTIME_RUST_CAN_RS"));
}

pub mod datapath {
    include!(env!("RIG_RUNTIME_RUST_DATAPATH_RS"));
}

mod dataflow {
    include!(env!("RIG_RUNTIME_RUST_DATAFLOW_RS"));
}

pub mod model {
    include!(env!("RIG_RUNTIME_RUST_MODEL_RS"));
}

pub mod rig {
    include!(env!("RIG_RUNTIME_RUST_RIG_RS"));
}

pub mod model_abi {
    include!(env!("RIG_RUNTIME_RUST_MODEL_ABI_RS"));
}

pub mod node_abi {
    include!(env!("RIG_RUNTIME_RUST_NODE_ABI_RS"));
}

pub mod battery_source {
    include!(env!("RIG_RUNTIME_RUST_BATTERY_SOURCE_RS"));
}

pub mod dc_load {
    include!(env!("RIG_RUNTIME_RUST_DC_LOAD_RS"));
}

pub mod drivetrain {
    include!(env!("RIG_RUNTIME_RUST_DRIVETRAIN_RS"));
}

pub mod simple {
    include!(env!("RIG_RUNTIME_RUST_SIMPLE_RS"));
}

mod scalar {
    include!(env!("RIG_RUNTIME_RUST_SCALAR_RS"));
}

pub mod spi {
    include!(env!("RIG_RUNTIME_RUST_SPI_RS"));
}

pub mod timer {
    include!(env!("RIG_RUNTIME_RUST_TIMER_RS"));
}

mod interfaces {
    include!(env!("RIG_RUNTIME_RUST_INTERFACES_RS"));
}

mod registry {
    include!(env!("RIG_RUNTIME_RUST_REGISTRY_RS"));
}

mod algorithms {
    include!(env!("RIG_RUNTIME_RUST_ALGORITHMS_RS"));
}

mod node {
    include!(env!("RIG_RUNTIME_RUST_NODE_RS"));
}

mod scheduler {
    include!(env!("RIG_RUNTIME_RUST_SCHEDULER_RS"));
}

pub mod cluster {
    include!(env!("RIG_RUNTIME_RUST_CLUSTER_RS"));
}
