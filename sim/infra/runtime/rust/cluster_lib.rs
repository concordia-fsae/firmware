pub mod dc_load {
    include!(env!("RIG_RUNTIME_RUST_DC_LOAD_RS"));
}

pub mod simple {
    include!(env!("RIG_RUNTIME_RUST_SIMPLE_RS"));
}

pub mod cluster {
    include!(env!("RIG_RUNTIME_RUST_CLUSTER_RS"));
}
