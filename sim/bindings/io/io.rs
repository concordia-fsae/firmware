pub fn set_analog_input(channel: i32, voltage: f32) {
    unsafe { super::ffi::rig_runtime_set_analog_input(channel, voltage) };
}

pub fn get_analog_input(channel: i32) -> f32 {
    unsafe { super::ffi::rig_runtime_get_analog_input(channel) }
}

pub fn set_digital(channel: i32, state: bool) {
    unsafe { super::ffi::rig_runtime_set_digital_io(channel, state) };
}

pub fn get_digital(channel: i32) -> bool {
    unsafe { super::ffi::rig_runtime_get_digital_io(channel) }
}
