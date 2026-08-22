use std::sync::{LazyLock, Mutex};

static ANALOG_INPUTS: LazyLock<Mutex<Vec<f32>>> = LazyLock::new(|| Mutex::new(Vec::new()));
static DIGITAL_IO: LazyLock<Mutex<Vec<bool>>> = LazyLock::new(|| Mutex::new(Vec::new()));

pub fn set_analog_input(channel: i32, voltage: f32) {
    if channel < 0 {
        return;
    }
    let mut analog_inputs = ANALOG_INPUTS.lock().unwrap();
    let channel = channel as usize;
    if channel >= analog_inputs.len() {
        analog_inputs.resize(channel + 1, 0.0);
    }
    analog_inputs[channel] = voltage;
}

pub fn get_analog_input(channel: i32) -> f32 {
    if channel < 0 {
        return 0.0;
    }
    ANALOG_INPUTS
        .lock()
        .unwrap()
        .get(channel as usize)
        .copied()
        .unwrap_or(0.0)
}

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
