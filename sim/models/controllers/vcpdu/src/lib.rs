mod rig_runtime {
    include!(env!("RIG_RUNTIME_RS"));
}

mod bindings {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("VCPDU_BINDINGS_RS"));
}

mod features {
    #![allow(non_upper_case_globals)]
    include!(env!("VCPDU_FEATURES_RS"));
}

mod yamcan {
    include!(env!("VCPDU_YAMCAN_RS"));
}

pub use yamcan::SignalMeasurement;

mod rust_model_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("VCPDU_YAMCAN_MODEL_RS"));
}

mod rust_decode_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("VCPDU_YAMCAN_DECODE_RS"));
}

use bindings::drv_inputAD_channelAnalog_E::{
    DRV_INPUTAD_ANALOG_5V_VOLTAGE, DRV_INPUTAD_ANALOG_UVL_BATT,
};
use bindings::drv_outputAD_channelDigital_E::DRV_OUTPUTAD_DIGITAL_LED;
use rig_runtime::nvm::ControllerNvm;
use rig_runtime::{AppDesc, ModuleDesc, NodeModel, NodeTarget, RTController};
use std::sync::Mutex;

const VCPDU_APP_START: u32 = 0x0800_2000;
const VCPDU_APP_END: u32 = 0x0802_0000;
const VCPDU_APP_CRC_LOCATION: u32 = 0x0801_FFF0;

rig_rt_controller_nvm_storage!(ControllerNvm<
    { features::NVM_BLOCK_SIZE as usize },
    { features::NVM_LIB_ENABLED },
    { features::NVM_FLASH_BACKED },
>);

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static appDesc: AppDesc = AppDesc::new(
    VCPDU_APP_START,
    VCPDU_APP_END,
    VCPDU_APP_CRC_LOCATION,
    features::APP_COMPONENT_ID as u16,
    features::APP_VARIANT_ID as u16,
);

unsafe extern "C" fn vcpdu_sys_1hz() {
    unsafe { bindings::drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_LED) };
}

#[unsafe(no_mangle)]
pub static sys_desc: ModuleDesc = ModuleDesc::new(None, None, None, None, Some(vcpdu_sys_1hz));

struct Vcpdu {
    nvm: ControllerNvm<
        { features::NVM_BLOCK_SIZE as usize },
        { features::NVM_LIB_ENABLED },
        { features::NVM_FLASH_BACKED },
    >,
}

impl Vcpdu {
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

impl NodeTarget for Vcpdu {
    unsafe fn reset_node(&mut self, controller: &mut RTController) {
        self.nvm.reset();
        rig_runtime::can::configure_network(VCPDU_CAN_NETWORK);
        unsafe { bindings::YAMCAN_shared_init_static() };
        controller.set_analog_input(DRV_INPUTAD_ANALOG_UVL_BATT as i32, 1.8);
        controller.set_analog_input(DRV_INPUTAD_ANALOG_5V_VOLTAGE as i32, 1.4);
    }
}

static VCPDU: Mutex<NodeModel<Vcpdu>> = Mutex::new(NodeModel::new(
    RTController::new_embedded_module(),
    Vcpdu::new(),
));

rig_model_abi!(VCPDU);

#[unsafe(no_mangle)]
pub extern "C" fn vcpdu_sim_get_vn9008_cs_amps_per_volt(channel: i32) -> f32 {
    if channel < 0 || channel >= bindings::drv_vn9008_E::DRV_VN9008_CHANNEL_COUNT as i32 {
        return 0.0;
    }
    unsafe { bindings::drv_vn9008_channels[channel as usize].cs_amp_per_volt }
}

rig_yamcan_network!(
    VCPDU_CAN_NETWORK,
    rust_model_generated,
    rust_decode_generated,
    yamcan
);
