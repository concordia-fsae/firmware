mod rig_runtime {
    include!(env!("RIG_RUNTIME_RS"));
}

mod bindings {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("BMSB_BINDINGS_RS"));
}

mod features {
    #![allow(non_upper_case_globals)]
    include!(env!("BMSB_FEATURES_RS"));
}

mod yamcan {
    include!(env!("BMSB_YAMCAN_RS"));
}

pub use yamcan::SignalMeasurement;

mod rust_model_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("BMSB_YAMCAN_MODEL_RS"));
}

mod rust_decode_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("BMSB_YAMCAN_DECODE_RS"));
}

use bindings::drv_inputAD_channelAnalog_E::DRV_INPUTAD_ANALOG_CS;
use bindings::drv_inputAD_channelDigital_E::{
    DRV_INPUTAD_DIGITAL_BMS_IMD_RESET, DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM,
    DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM, DRV_INPUTAD_DIGITAL_OK_HS, DRV_INPUTAD_DIGITAL_TSMS_CHG,
};
use bindings::drv_outputAD_channelDigital_E::DRV_OUTPUTAD_DIGITAL_LED;
use rig_runtime::nvm::ControllerNvm;
use rig_runtime::node_abi::ModelDataPathProvider;
use rig_runtime::{AppDesc, ModuleDesc, NodeModel, NodeTarget, RTController};
use std::sync::Mutex;

const BMSB_APP_START: u32 = 0x0800_2000;
const BMSB_APP_END: u32 = 0x0802_0000;
const BMSB_APP_CRC_LOCATION: u32 = 0x0801_FFF0;

rig_rt_controller_nvm_storage!(ControllerNvm<
    { features::NVM_BLOCK_SIZE as usize },
    { features::NVM_LIB_ENABLED },
    { features::NVM_FLASH_BACKED },
>);

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static appDesc: AppDesc = AppDesc::new(
    BMSB_APP_START,
    BMSB_APP_END,
    BMSB_APP_CRC_LOCATION,
    features::APP_COMPONENT_ID as u16,
    features::APP_VARIANT_ID as u16,
);

unsafe extern "C" fn bmsb_sys_1hz() {
    unsafe { bindings::drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED) };
}

#[unsafe(no_mangle)]
pub static SYS_desc: ModuleDesc = ModuleDesc::new(None, None, None, None, Some(bmsb_sys_1hz));

struct Bmsb {
    nvm: ControllerNvm<
        { features::NVM_BLOCK_SIZE as usize },
        { features::NVM_LIB_ENABLED },
        { features::NVM_FLASH_BACKED },
    >,
}

impl Bmsb {
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

impl NodeTarget<RTController> for Bmsb {
    unsafe fn reset_node(&mut self, controller: &mut RTController) {
        self.nvm.reset();
        rig_runtime::can::configure_network(BMSB_CAN_NETWORK);
        unsafe { bindings::YAMCAN_shared_init_static() };
        controller.set_analog_input(DRV_INPUTAD_ANALOG_CS as i32, 0.0);
        controller.set_digital(DRV_INPUTAD_DIGITAL_TSMS_CHG as i32, false);
        controller.set_digital(DRV_INPUTAD_DIGITAL_OK_HS as i32, false);
        controller.set_digital(DRV_INPUTAD_DIGITAL_BMS_IMD_RESET as i32, false);
        controller.set_digital(DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM as i32, false);
        controller.set_digital(DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM as i32, false);
    }
}

impl ModelDataPathProvider for Bmsb {}

static BMSB: Mutex<NodeModel<Bmsb, RTController>> = Mutex::new(NodeModel::new(
    RTController::new_embedded_module(),
    Bmsb::new(),
));

rig_model_abi!(BMSB, rig_runtime::node_abi);
rig_model_fault_abi!();

rig_yamcan_network!(
    BMSB_CAN_NETWORK,
    rust_model_generated,
    rust_decode_generated,
    yamcan
);
