mod rig_runtime {
    include!(env!("RIG_RUNTIME_RS"));
}

mod bindings {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("VCFRONT_BINDINGS_RS"));
}

mod features {
    #![allow(non_upper_case_globals)]
    include!(env!("VCFRONT_FEATURES_RS"));
}

mod yamcan {
    include!(env!("VCFRONT_YAMCAN_RS"));
}

pub use yamcan::SignalMeasurement;

mod rust_model_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("VCFRONT_YAMCAN_MODEL_RS"));
}

mod rust_decode_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("VCFRONT_YAMCAN_DECODE_RS"));
}

use bindings::drv_inputAD_channelAnalog_E::{
    DRV_INPUTAD_ANALOG_APPS_P1, DRV_INPUTAD_ANALOG_APPS_P2, DRV_INPUTAD_ANALOG_BR_PR,
};
use bindings::drv_outputAD_channelDigital_E::DRV_OUTPUTAD_DIGITAL_LED;
use rig_runtime::nvm::ControllerNvm;
use rig_runtime::node_abi::ModelDataPathProvider;
use rig_runtime::{AppDesc, ModuleDesc, NodeModel, NodeTarget, RTController};
use std::sync::Mutex;

const VCFRONT_APP_START: u32 = 0x0800_2000;
const VCFRONT_APP_END: u32 = 0x0802_0000;
const VCFRONT_APP_CRC_LOCATION: u32 = 0x0801_FFF0;

rig_rt_controller_nvm_storage!(ControllerNvm<
    { features::NVM_BLOCK_SIZE as usize },
    { features::NVM_LIB_ENABLED },
    { features::NVM_FLASH_BACKED },
>);

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static appDesc: AppDesc = AppDesc::new(
    VCFRONT_APP_START,
    VCFRONT_APP_END,
    VCFRONT_APP_CRC_LOCATION,
    features::APP_COMPONENT_ID as u16,
    features::APP_VARIANT_ID as u16,
);

unsafe extern "C" fn vcfront_sys_1hz() {
    unsafe { bindings::drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED) };
}

#[unsafe(no_mangle)]
pub static sys_desc: ModuleDesc = ModuleDesc::new(None, None, None, None, Some(vcfront_sys_1hz));

struct Vcfront {
    nvm: ControllerNvm<
        { features::NVM_BLOCK_SIZE as usize },
        { features::NVM_LIB_ENABLED },
        { features::NVM_FLASH_BACKED },
    >,
}

impl Vcfront {
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

impl NodeTarget<RTController> for Vcfront {
    unsafe fn reset_node(&mut self, controller: &mut RTController) {
        self.nvm.reset();
        rig_runtime::can::configure_network(VCFRONT_CAN_NETWORK);
        unsafe { bindings::YAMCAN_shared_init_static() };
        controller.set_analog_input(DRV_INPUTAD_ANALOG_BR_PR as i32, 0.3);
        controller.set_analog_input(DRV_INPUTAD_ANALOG_APPS_P1 as i32, 0.720);
        controller.set_analog_input(DRV_INPUTAD_ANALOG_APPS_P2 as i32, 1.475);
    }
}

impl ModelDataPathProvider for Vcfront {}

static VCFRONT: Mutex<NodeModel<Vcfront, RTController>> = Mutex::new(NodeModel::new(
    RTController::new_embedded_module(),
    Vcfront::new(),
));

rig_model_abi!(VCFRONT, rig_runtime::node_abi);
rig_model_fault_abi!();

rig_yamcan_network!(
    VCFRONT_CAN_NETWORK,
    rust_model_generated,
    rust_decode_generated,
    yamcan
);
