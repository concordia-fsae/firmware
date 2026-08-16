unsafe extern "C" {
    pub(super) fn rig_runtime_reset();
    pub(super) fn rig_runtime_advance_time_ns(elapsed_ns: u64);
    pub(super) fn rig_runtime_get_time_ns() -> u64;
    pub(super) fn HW_TIM_getTimeMS() -> u32;
    pub(super) fn rig_runtime_set_analog_input(channel: i32, voltage: f32);
    pub(super) fn rig_runtime_get_analog_input(channel: i32) -> f32;
    pub(super) fn rig_runtime_set_digital_io(channel: i32, state: bool);
    pub(super) fn rig_runtime_get_digital_io(channel: i32) -> bool;
    pub(super) fn rig_runtime_get_fault(fault: i32) -> bool;
}
