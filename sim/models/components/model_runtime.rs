// Shared runtime for standalone firmware component-model tests.
//
// The firmware runtime library stays model-free. This crate is the explicit
// model-library composition used when a test has no controller simulation
// library to provide the component's native ABI.

include!(env!("RIG_RUNTIME_RUST_LIB_RS"));

mod asm330 {
    include!(env!("RIG_MODEL_ASM330_RS"));
}

mod battery_source {
    include!(env!("RIG_MODEL_BATTERY_SOURCE_RS"));
}

mod dc_load {
    include!(env!("RIG_MODEL_DC_LOAD_RS"));
}

mod drivetrain {
    include!(env!("RIG_MODEL_DRIVETRAIN_RS"));
}
