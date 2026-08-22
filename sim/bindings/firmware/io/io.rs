unsafe extern "C" {
    fn rig_runtime_set_analog_input(channel: i32, voltage: f32);
    fn rig_runtime_get_analog_input(channel: i32) -> f32;
    fn rig_runtime_set_digital_io(channel: i32, state: bool);
    fn rig_runtime_get_digital_io(channel: i32) -> bool;
}

pub fn set_analog_input(channel: i32, voltage: f32) {
    unsafe { rig_runtime_set_analog_input(channel, voltage) };
}

pub fn get_analog_input(channel: i32) -> f32 {
    unsafe { rig_runtime_get_analog_input(channel) }
}

pub fn set_digital(channel: i32, state: bool) {
    unsafe { rig_runtime_set_digital_io(channel, state) };
}

pub fn get_digital(channel: i32) -> bool {
    unsafe { rig_runtime_get_digital_io(channel) }
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_set_analog_input(channel: i32, voltage: f32) {
    set_analog_input(channel, voltage);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_get_analog_input(channel: i32) -> f32 {
    get_analog_input(channel)
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_set_digital_io(channel: i32, state: bool) {
    set_digital(channel, state);
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_model_get_digital_io(channel: i32) -> bool {
    get_digital(channel)
}
