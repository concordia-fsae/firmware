mod rig_runtime {
    include!(env!("RIG_RUNTIME_RS"));
}

mod bindings {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("SWS_BINDINGS_RS"));
}

mod features {
    #![allow(non_upper_case_globals)]
    include!(env!("SWS_FEATURES_RS"));
}

mod yamcan {
    include!(env!("SWS_YAMCAN_RS"));
}

pub use yamcan::SignalMeasurement;

mod rust_model_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("SWS_YAMCAN_MODEL_RS"));
}

mod rust_decode_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("SWS_YAMCAN_DECODE_RS"));
}

use bindings::drv_outputAD_channelDigital_E::DRV_OUTPUTAD_DIGITAL_LED;
use rig_runtime::nvm::ControllerNvm;
use rig_runtime::{AppDesc, ModuleDesc, NodeModel, NodeTarget, RTController};
use std::sync::Mutex;

const SWS_APP_START: u32 = 0x0800_2000;
const SWS_APP_END: u32 = 0x0801_0000;
const SWS_APP_CRC_LOCATION: u32 = 0x0800_FFF0;

rig_rt_controller_nvm_storage!(ControllerNvm<
    { features::NVM_BLOCK_SIZE as usize },
    { features::NVM_LIB_ENABLED },
    { features::NVM_FLASH_BACKED },
>);

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static appDesc: AppDesc = AppDesc::new(
    SWS_APP_START,
    SWS_APP_END,
    SWS_APP_CRC_LOCATION,
    features::APP_COMPONENT_ID as u16,
    features::APP_VARIANT_ID as u16,
);

unsafe extern "C" fn sws_sys_1hz() {
    unsafe { bindings::drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED) };
}

#[unsafe(no_mangle)]
pub static sys_desc: ModuleDesc = ModuleDesc::new(None, None, None, None, Some(sws_sys_1hz));

struct Sws {
    nvm: ControllerNvm<
        { features::NVM_BLOCK_SIZE as usize },
        { features::NVM_LIB_ENABLED },
        { features::NVM_FLASH_BACKED },
    >,
}

impl Sws {
    const fn new() -> Self {
        Self {
            nvm: ControllerNvm::<
                { features::NVM_BLOCK_SIZE as usize },
                { features::NVM_LIB_ENABLED },
                { features::NVM_FLASH_BACKED },
            >::new(),
        }
    }
}

impl NodeTarget for Sws {
    unsafe fn reset_node(&mut self, _controller: &mut RTController) {
        self.nvm.reset();
        rig_runtime::can::configure_network(SWS_CAN_NETWORK);
        unsafe { bindings::YAMCAN_shared_init_static() };
    }
}

static SWS: Mutex<NodeModel<Sws>> = Mutex::new(NodeModel::new(
    RTController::new_embedded_module(),
    Sws::new(),
));

rig_model_abi!(SWS);

rig_yamcan_network!(SWS_CAN_NETWORK, rust_model_generated, rust_decode_generated, yamcan);
