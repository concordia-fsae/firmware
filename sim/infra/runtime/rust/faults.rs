pub fn get(fault: i32) -> bool {
    unsafe { super::ffi::rig_runtime_get_fault(fault) }
}
