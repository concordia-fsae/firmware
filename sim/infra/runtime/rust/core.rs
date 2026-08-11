pub fn reset() {
    unsafe { super::ffi::rig_runtime_reset() };
}
