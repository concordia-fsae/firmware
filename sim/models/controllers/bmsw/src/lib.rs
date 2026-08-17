mod rig_runtime {
    include!(env!("RIG_RUNTIME_RS"));
}

mod bindings {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("BMSW_BINDINGS_RS"));
}

mod features {
    #![allow(non_upper_case_globals)]
    include!(env!("BMSW_FEATURES_RS"));
}

mod yamcan {
    include!(env!("BMSW_YAMCAN_RS"));
}

pub use yamcan::SignalMeasurement;

mod rust_model_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("BMSW_YAMCAN_MODEL_RS"));
}

mod rust_decode_generated {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    include!(env!("BMSW_YAMCAN_DECODE_RS"));
}

use rig_runtime::nvm::ControllerNvm;
use rig_runtime::node_abi::ModelDataPathProvider;
use rig_runtime::spi::SpiTransaction;
use rig_runtime::{AppDesc, ModuleDesc, NodeModel, NodeTarget, RTController};
use std::sync::Mutex;

const BMSW_APP_START: u32 = 0x0800_2000;
const BMSW_APP_END: u32 = 0x0802_0000;
const BMSW_APP_CRC_LOCATION: u32 = 0x0801_FFF0;

fn reverse_byte(mut value: u8) -> u8 {
    value = (value & 0xF0) >> 4 | (value & 0x0F) << 4;
    value = (value & 0xCC) >> 2 | (value & 0x33) << 2;
    (value & 0xAA) >> 1 | (value & 0x55) << 1
}

unsafe extern "C" fn max14921_response(
    transaction: *const SpiTransaction,
    response: *mut SpiTransaction,
) -> bool {
    if transaction.is_null() || response.is_null() {
        return false;
    }
    let transaction = unsafe { *transaction };
    if transaction.rx_len == 0 {
        return false;
    }

    let mut next_response = SpiTransaction {
        device: transaction.device,
        rx_len: transaction.rx_len,
        timestamp_ns: transaction.timestamp_ns,
        ..SpiTransaction::default()
    };
    if next_response.rx_len >= 3 {
        // MAX14921 product id, valid die status, and no diagnostic faults.
        next_response.rx_data[2] = reverse_byte(0x02);
    }
    unsafe { *response = next_response };
    true
}

rig_rt_controller_nvm_storage!(ControllerNvm<
    { features::NVM_BLOCK_SIZE as usize },
    { features::NVM_LIB_ENABLED },
    { features::NVM_FLASH_BACKED },
>);

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static appDesc: AppDesc = AppDesc::new(
    BMSW_APP_START,
    BMSW_APP_END,
    BMSW_APP_CRC_LOCATION,
    features::APP_COMPONENT_ID as u16,
    features::APP_VARIANT_ID as u16,
);

#[unsafe(no_mangle)]
pub static SYS_desc: ModuleDesc = ModuleDesc::new(None, None, None, None, None);

struct Bmsw {
    nvm: ControllerNvm<
        { features::NVM_BLOCK_SIZE as usize },
        { features::NVM_LIB_ENABLED },
        { features::NVM_FLASH_BACKED },
    >,
}

impl Bmsw {
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

impl NodeTarget<RTController> for Bmsw {
    unsafe fn reset_node(&mut self, controller: &mut RTController) {
        self.nvm.reset();
        rig_runtime::can::configure_network(BMSW_CAN_NETWORK);
        rig_runtime::spi::configure_device_chip_select(
            bindings::HW_spi_device_E::HW_SPI_DEV_BMS as i32,
            bindings::HW_GPIO_pinmux_E::HW_GPIO_SPI1_MAX_NCS as i32,
        );
        rig_runtime::spi::configure_responder(
            bindings::HW_spi_device_E::HW_SPI_DEV_BMS as i32,
            Some(max14921_response),
        );
        unsafe { bindings::YAMCAN_shared_init_static() };
        controller.set_analog_input(
            bindings::drv_inputAD_channelAnalog_E::DRV_INPUTAD_ANALOG_REF_VOLTAGE as i32,
            3.0,
        );
        controller.set_analog_input(
            bindings::drv_inputAD_channelAnalog_E::DRV_INPUTAD_ANALOG_TEMP_MCU as i32,
            1.43,
        );
        for channel in 0..8 {
            controller.set_analog_input(
                bindings::drv_inputAD_channelAnalog_E::DRV_INPUTAD_ANALOG_MUX1_CH1 as i32
                    + channel,
                1.5,
            );
        }
        controller.set_analog_input(
            bindings::drv_inputAD_channelAnalog_E::DRV_INPUTAD_ANALOG_SEGMENT as i32,
            25.9,
        );
    }
}

impl ModelDataPathProvider for Bmsw {}

static BMSW: Mutex<NodeModel<Bmsw, RTController>> = Mutex::new(NodeModel::new(
    RTController::new_embedded_module(),
    Bmsw::new(),
));

rig_model_abi!(BMSW, rig_runtime::node_abi);
rig_model_fault_abi!();

rig_yamcan_network!(
    BMSW_CAN_NETWORK,
    rust_model_generated,
    rust_decode_generated,
    yamcan
);
