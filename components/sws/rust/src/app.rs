use crate::{
    can::{Frame, Network, Scheduler},
    input::{Buttons, Driver, REQUEST_SIGNALS},
    protocol::*,
    screen::Screen,
    uds::{Descriptor, Diagnostics},
};

pub struct App {
    pub network: Network,
    pub buttons: Buttons,
    pub driver: Driver,
    pub screen: Screen,
    pub diagnostics: Diagnostics,
    pub sleeping: bool,
    pub now: u32,
    pub runtime_cycles: u64,
    sleep_at: u32,
    vehicle_state: u32,
    missing_state_at: Option<u32>,
    phase: u16,
    scheduler: Scheduler<{ MESSAGES.len() }>,
}
impl Default for App {
    fn default() -> Self {
        Self {
            network: Network::default(),
            buttons: Buttons::default(),
            driver: Driver::default(),
            screen: Screen::default(),
            diagnostics: Diagnostics::new(
                MESSAGES[MSG_UDSCLIENT_SWS_UDS_REQUEST].id,
                MESSAGES[MSG_SWS_UDS_RESPONSE].id,
            ),
            sleeping: false,
            now: 0,
            runtime_cycles: 0,
            sleep_at: 900_000,
            vehicle_state: vehicle_state::INIT,
            missing_state_at: None,
            phase: 0,
            scheduler: Scheduler::default(),
        }
    }
}
impl App {
    pub fn receive(&mut self, frame: impl Into<Frame>, descriptor: Descriptor) -> Option<Frame> {
        let frame = frame.into();
        if frame.id == MESSAGES[MSG_UDSCLIENT_SWS_UDS_REQUEST].id {
            let remaining = self.sleep_at.wrapping_sub(self.now);
            if !(300_000..0x8000_0000).contains(&remaining) {
                self.sleep_at = self.now.wrapping_add(300_000);
            }
            self.diagnostics.receive(frame, self.now, descriptor)
        } else {
            self.network.receive(frame, self.now);
            None
        }
    }
    pub fn tick(&mut self, raw: [bool; 8]) {
        self.now = self.now.wrapping_add(1);
        self.phase = (self.phase + 1) % 1000;
        self.buttons.sample(raw, self.now);
        self.diagnostics.tick(self.now);
        if self.phase % 10 == 0 {
            if let Some(state) = self.network.raw(VCPDU_VEHICLE_STATE, self.now) {
                self.vehicle_state = state;
                self.missing_state_at = None;
            } else if let Some(at) = self.missing_state_at {
                if self.now.wrapping_sub(at) >= 1000 {
                    self.vehicle_state = vehicle_state::ON_GLV;
                    self.missing_state_at = None;
                }
            } else {
                self.missing_state_at = Some(self.now);
            }
            self.sleeping = self.vehicle_state == vehicle_state::SLEEP;
            self.driver.update(&self.buttons, &self.network, self.now);
            if self.buttons.state[..6].iter().any(|b| b.pressed) {
                self.sleep_at = self.now.wrapping_add(900_000);
            }
        }
        if self.phase % 100 == 0 {
            self.screen.update(&self.network, self.now);
        }
    }
    /// Shared TIM2 task body. RTIC and SIL supply their CAN/GPIO backends.
    pub fn timer_task(
        &mut self,
        descriptor: Descriptor,
        mut receive: impl FnMut() -> Option<Frame>,
        inputs: impl FnOnce() -> [bool; 8],
        mut send: impl FnMut(Frame),
        heartbeat: impl FnOnce(),
    ) {
        for _ in 0..32 {
            let Some(frame) = receive() else {
                break;
            };
            if let Some(response) = self.receive(frame, descriptor) {
                send(response);
            }
        }
        self.tick(inputs());
        if self.heartbeat() {
            heartbeat();
        }
        self.transmit_due(send);
    }
    pub fn heartbeat(&self) -> bool {
        self.phase == 0
    }
    pub fn telemetry(&self, message: usize) -> [u8; 8] {
        let mut data = [0; 8];
        match message {
            MSG_SWS_DRIVER_REQUEST | MSG_SWS_DRIVER_PARAM_REQUEST => {
                for (request, signal) in REQUEST_SIGNALS {
                    if signal.message == message {
                        signal.set(
                            &mut data,
                            if self.driver.requested(*request) {
                                digital_status::ON
                            } else {
                                digital_status::OFF
                            },
                        );
                    }
                }
                if message == MSG_SWS_DRIVER_REQUEST {
                    SWS_REQUEST_SCREEN.set(&mut data, self.driver.screen(&self.network, self.now));
                }
            }
            MSG_SWS_INPUT_DEBUG_STATUS => {
                for (i, button) in self.buttons.state.iter().enumerate() {
                    crate::can::set_bits(
                        &mut data,
                        (i * 4) as u8,
                        2,
                        if button.pressed { 2 } else { 1 },
                    );
                    crate::can::set_bits(
                        &mut data,
                        (i * 4 + 2) as u8,
                        2,
                        if button.debouncing { 2 } else { 1 },
                    );
                }
            }
            MSG_SWS_SLEEP => SWS_SLEEPABLE.set(
                &mut data,
                if self.now.wrapping_sub(self.sleep_at) < 0x8000_0000 {
                    sleep_follower_state::OK_TO_SLEEP
                } else {
                    sleep_follower_state::NOK_TO_SLEEP
                },
            ),
            MSG_SWS_SCREEN_MANAGER => {
                SWS_SCREEN_ALERT.set(&mut data, self.screen.alert);
                SWS_SCREEN_WARNING.set(&mut data, self.screen.warning);
            }
            MSG_SWS_CONFIG_MANAGER => {
                let config = self.driver.selected;
                SWS_DRIVER_CONFIG_SELECTION.set(
                    &mut data,
                    if self.driver.page == 3 {
                        config.selection
                    } else {
                        config_selection::NONE
                    },
                );
                SWS_DRIVER_CONFIG_OPTION_LEFT.set(&mut data, config.options[0]);
                SWS_DRIVER_CONFIG_OPTION_RIGHT.set(&mut data, config.options[1]);
                SWS_OPTION13.set(&mut data, if self.driver.option13 { 2 } else { 1 });
                SWS_CONFIG_HAS_VALUE.set(&mut data, if config.source.is_some() { 2 } else { 1 });
                SWS_CONFIG_VALUE_SCALE.set(&mut data, config.scale);
                SWS_CONFIG_VALUE_UNIT.set(&mut data, config.unit);
                SWS_DISPLAY_VALUE.set(
                    &mut data,
                    self.driver.display_value(&self.network, self.now),
                );
            }
            MSG_SWS_RTOS_TASK_INFO => {
                // Preserve the legacy cumulative counters (the CAN fields truncate to 9 bits).
                SWS_TASK_USAGE1K_HZ.set(
                    &mut data,
                    ((self.runtime_cycles * 100) / (self.now.max(1) as u64 * 64_000)).min(100)
                        as u32,
                );
                SWS_TASK_ITERATIONS1_HZ.set(&mut data, self.now / 1000);
                SWS_TASK_ITERATIONS10_HZ.set(&mut data, self.now / 100);
                SWS_TASK_ITERATIONS100_HZ.set(&mut data, self.now / 10);
                SWS_TASK_ITERATIONS1K_HZ.set(&mut data, self.now);
            }
            _ => {}
        }
        data
    }
    pub fn transmit_due(&mut self, mut send: impl FnMut(Frame)) {
        for &index in TX_MESSAGES {
            let message = MESSAGES[index];
            if let Some(counter) = self.scheduler.due(index, message.period, self.now) {
                send(message.frame(self.telemetry(index), counter).into());
            }
        }
    }
}
