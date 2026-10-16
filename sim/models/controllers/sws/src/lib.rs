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

use bindings::HW_GPIO_pinmux_E::*;
use firmware_shared::can::{Frame, RxQueue, TxQueue};
use rig_runtime::model::{NodeModel, NodeTarget};
use rig_runtime::node_abi::ModelDataPathProvider;
use rig_runtime::rtic_controller::{RticApplication, RticController};
use std::sync::{LazyLock, Mutex};
use sws::{app::App, protocol::*, uds::Descriptor};

const PINS: [i32; 8] = [
    HW_GPIO_DIN1 as i32,
    HW_GPIO_DIN2 as i32,
    HW_GPIO_DIN3 as i32,
    HW_GPIO_DIN4 as i32,
    HW_GPIO_DIN5 as i32,
    HW_GPIO_DIN6 as i32,
    HW_GPIO_DIN7 as i32,
    HW_GPIO_DIN8 as i32,
];
const IMAGE: &[u8] = include_bytes!(env!("FIRMWARE_SIM_IMAGE"));

fn descriptor() -> Descriptor {
    let word = |offset| u32::from_le_bytes(IMAGE[offset..offset + 4].try_into().unwrap());
    Descriptor {
        start: word(0),
        end: word(4),
        crc_location: word(8),
        crc: word(IMAGE.len() - 4),
        component: u16::from_le_bytes(IMAGE[12..14].try_into().unwrap()),
        variant: u16::from_le_bytes(IMAGE[14..16].try_into().unwrap()),
    }
}

struct SwsRtic {
    app: App,
    rx: RxQueue<32>,
    tx: TxQueue<32>,
}
impl SwsRtic {
    fn new() -> Self {
        Self {
            app: App::default(),
            rx: RxQueue::default(),
            tx: TxQueue::new(MESSAGES[MSG_SWS_UDS_RESPONSE].id),
        }
    }
}
impl RticApplication for SwsRtic {
    fn reset(&mut self) {
        *self = Self::new();
        rig_runtime::io::set_digital(HW_GPIO_LED as i32, false);
        rig_runtime::io::set_digital(HW_GPIO_CAN_SLEEP as i32, false);
    }
    fn can_interrupt(&mut self, bus: u8, packet: rig_runtime::CanPacket) {
        if bus == 0
            && packet.len <= 8
            && MESSAGES[..MSG_SWS_FAULTS].iter().any(|m| m.id == packet.id)
        {
            self.rx.push(Frame {
                id: packet.id,
                len: packet.len,
                data: packet.data,
            });
        }
    }
    fn timer_interrupt(&mut self) {
        self.app.timer_task(
            descriptor(),
            || self.rx.pop(),
            || PINS.map(|pin| !rig_runtime::io::get_digital(pin)),
            |frame| self.tx.enqueue(frame),
            || {
                let pin = HW_GPIO_LED as i32;
                rig_runtime::io::set_digital(pin, !rig_runtime::io::get_digital(pin));
            },
        );
        // Match the firmware's bounded three-mailbox transmit service per tick.
        for _ in 0..3 {
            let Some(frame) = self.tx.front() else {
                break;
            };
            let packet = rig_runtime::CanPacket::new(frame.id, frame.data, frame.len);
            if !rig_runtime::can::rig_runtime_can_push_tx(0, &packet) {
                break;
            }
            self.tx.pop();
        }
    }
    fn reset_requested(&self) -> bool {
        self.app.diagnostics.reset_due(self.app.now)
    }
}
struct Sws;
impl NodeTarget<RticController<SwsRtic>> for Sws {
    unsafe fn reset_node(&mut self, controller: &mut RticController<SwsRtic>) {
        for pin in PINS {
            controller.set_digital(pin, true);
        }
        rig_runtime::can::configure_network(SWS_CAN_NETWORK);
    }
}
impl ModelDataPathProvider for Sws {}
static SWS: LazyLock<Mutex<NodeModel<Sws, RticController<SwsRtic>>>> =
    LazyLock::new(|| Mutex::new(NodeModel::new(RticController::new(SwsRtic::new()), Sws)));
rig_model_abi!(SWS, rig_runtime::node_abi);
rig_yamcan_network!(
    SWS_CAN_NETWORK,
    rust_model_generated,
    rust_decode_generated,
    yamcan
);

#[unsafe(no_mangle)]
pub extern "C" fn sws_sim_get_tick_count() -> u32 {
    SWS.lock().unwrap().controller().application.app.now
}
#[unsafe(no_mangle)]
pub extern "C" fn sws_sim_get_reset_count() -> u32 {
    SWS.lock().unwrap().controller().resets
}
#[unsafe(no_mangle)]
pub extern "C" fn sws_sim_get_rx_dropped() -> u32 {
    SWS.lock().unwrap().controller().application.rx.dropped
}
