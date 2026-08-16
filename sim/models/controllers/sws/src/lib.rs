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
use bindings::HW_GPIO_pinmux_E::{
    HW_GPIO_DIN1, HW_GPIO_DIN2, HW_GPIO_DIN3, HW_GPIO_DIN4, HW_GPIO_DIN5, HW_GPIO_DIN6,
    HW_GPIO_DIN7, HW_GPIO_DIN8,
};
use rig_runtime::nvm::ControllerNvm;
use rig_runtime::node_abi::ModelDataPathProvider;
use rig_runtime::{AppDesc, ModuleDesc, NodeModel, NodeTarget, RTController};
use std::sync::Mutex;

const SWS_APP_START: u32 = 0x0800_2000;
const SWS_APP_END: u32 = 0x0801_0000;
const SWS_APP_CRC_LOCATION: u32 = 0x0800_FFF0;

// The firmware's SWS inputs are active-low GPIOs. Resetting them here keeps
// the Rust-hosted model deterministic and makes the physical driver-input
// boundary explicit: Python tests set logical buttons through the generic IO
// ABI, while the embedded firmware remains responsible for debounce and CAN
// request generation.
fn release_driver_inputs(controller: &RTController) {
    for pin in [
        HW_GPIO_DIN1,
        HW_GPIO_DIN2,
        HW_GPIO_DIN3,
        HW_GPIO_DIN4,
        HW_GPIO_DIN5,
        HW_GPIO_DIN6,
        HW_GPIO_DIN7,
        HW_GPIO_DIN8,
    ] {
        controller.set_digital(pin as i32, true);
    }
}

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

impl NodeTarget<RTController> for Sws {
    unsafe fn reset_node(&mut self, controller: &mut RTController) {
        self.nvm.reset();
        release_driver_inputs(controller);
        rig_runtime::can::configure_network(SWS_CAN_NETWORK);
        unsafe { bindings::YAMCAN_shared_init_static() };
    }
}

impl ModelDataPathProvider for Sws {}

static SWS: Mutex<NodeModel<Sws, RTController>> = Mutex::new(NodeModel::new(
    RTController::new_embedded_module(),
    Sws::new(),
));

rig_model_abi!(SWS, rig_runtime::node_abi);

rig_yamcan_network!(SWS_CAN_NETWORK, rust_model_generated, rust_decode_generated, yamcan);
