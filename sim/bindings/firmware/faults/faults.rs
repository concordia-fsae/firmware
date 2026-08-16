unsafe extern "C" {
    fn rig_runtime_get_fault(fault: i32) -> bool;
}

pub fn get(fault: i32) -> bool {
    unsafe { rig_runtime_get_fault(fault) }
}

#[macro_export]
macro_rules! rig_model_fault_abi {
    () => {
        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_get_fault(fault: i32) -> bool {
            $crate::rig_runtime::faults::get(fault)
        }
    };
}
