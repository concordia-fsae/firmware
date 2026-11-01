use crate::{can::Network, input::Timer, protocol::*};
#[derive(Default)]
pub struct Screen {
    pub alert: u32,
    pub warning: u32,
    active: u16,
    unseen: u16,
    timer: Timer,
}
impl Screen {
    pub fn update(&mut self, network: &Network, now: u32) {
        self.alert = if network.fault(VCREAR_MC_CALIBRATING_RESOLVER, now) {
            screen_alerts::RESOLVER_CALIBRATING
        } else if network.raw(VCPDU_CRASH_SENSOR_STATE, now) != Some(crash_sensor_state::OK) {
            screen_alerts::CRASH
        } else if network.fault(VCPDU_IMU_SELF_TEST_FAILED, now) {
            screen_alerts::IMU_SELFTEST_FAILED
        } else if network.raw(VCFRONT_GEAR, now) == Some(gear::REVERSE) {
            screen_alerts::REVERSE
        } else {
            screen_alerts::NONE
        };
        // Ordering matches the legacy screen manager, independently of the CAN enum order.
        let states = [
            (
                screen_warnings::LOW_GLV,
                network.fault(VCPDU_LOW_VOLTAGE, now),
            ),
            (
                screen_warnings::CONTACTS_OPEN_IN_RUN,
                network.fault(VCPDU_CONTACTS_OPEN_IN_RUN, now),
            ),
            (
                screen_warnings::IMU_UNCALIBRATED,
                network.fault(VCPDU_IMU_UNCALIBRATED, now),
            ),
            (
                screen_warnings::APPS_DISABLED,
                network.fault(VCFRONT_APPS_DISABLED, now),
            ),
            (
                screen_warnings::CONTACTOR_SOH_LOW,
                network.fault(BMSB_CONTACTOR_LOW_SOH_HVP, now)
                    || network.fault(BMSB_CONTACTOR_LOW_SOH_HVN, now)
                    || network.fault(BMSB_CONTACTOR_LOW_SOH_PRECHARGE, now),
            ),
            (
                screen_warnings::IMU_YAW_CALIBRATION_FAILED,
                network.fault(VCPDU_IMU_YAW_CALIBRATION_FAILED, now),
            ),
            (
                screen_warnings::RESOLVER_CALIBRATING_FAILED,
                network.fault(VCREAR_MC_CALIBRATING_RESOLVER_FAILED, now),
            ),
            (
                screen_warnings::BRAKE_PRESSURE_SENSOR_REAR_FAULT,
                network.fault(VCREAR_BRAKE_PRESSURE_SENSOR_FAULT, now),
            ),
            (
                screen_warnings::BRAKE_PRESSURE_SENSOR_FRONT_FAULT,
                network.fault(VCFRONT_BRAKE_PRESSURE_SENSOR_FAULT, now),
            ),
            (
                screen_warnings::STEERING_ANGLE_SENSOR_FAULT,
                network.fault(VCFRONT_STEERING_ANGLE_SENSOR_FAULT, now),
            ),
        ];
        let mut active = 0;
        for (i, (_, set)) in states.iter().enumerate() {
            if *set {
                active |= 1 << i;
            }
        }
        self.unseen |= active & !self.active;
        self.active = active;
        if active | self.unseen == 0 {
            self.warning = 0;
            self.timer.stop();
            return;
        }
        if !self.timer.running(now, 2000) {
            let index = if self.unseen != 0 {
                self.unseen.trailing_zeros() as usize
            } else {
                let current = states
                    .iter()
                    .position(|(warning, _)| *warning == self.warning)
                    .unwrap_or(0);
                (1..=states.len())
                    .map(|step| (current + step) % states.len())
                    .find(|i| active & (1 << i) != 0)
                    .unwrap()
            };
            self.warning = states[index].0;
            self.unseen &= !(1 << index);
            self.timer.restart(now);
        }
    }
}
