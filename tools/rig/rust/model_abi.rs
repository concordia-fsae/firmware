// Generic C ABI for a Rig model backed by a NodeModel.
//
// The consuming crate supplies the concrete model instance and the node-ABI
// module path. No firmware symbols or peripheral identifiers are part of this
// contract.

#[macro_export]
macro_rules! rig_model_abi {
    ($model:expr, $($node_abi:ident)::+) => {
        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_new() {
            unsafe {
                $model.lock().unwrap().reset();
            }
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_run_for(elapsed_ns: u64) {
            unsafe {
                $model.lock().unwrap().run_for_ns(elapsed_ns);
            }
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_datapath_count() -> u32 {
            let mut model = $model.lock().unwrap();
            $($node_abi)::+::datapath_count(&mut *model)
        }

        #[unsafe(no_mangle)]
        pub extern "C" fn rig_model_datapath_descriptor(
            index: u32,
            out: *mut $($node_abi)::+::ModelDataPathDescriptor,
        ) -> bool {
            if out.is_null() {
                return false;
            }
            let mut model = $model.lock().unwrap();
            match $($node_abi)::+::datapath_descriptor(&mut *model, index) {
                Some(descriptor) => {
                    unsafe { *out = descriptor };
                    true
                }
                None => false,
            }
        }
    };
}
