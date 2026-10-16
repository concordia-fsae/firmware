use crate::{
    can::{Network, Signal},
    protocol::*,
};

pub use firmware_shared::time::Timer;

pub struct Buttons(firmware_shared::input::Buttons<8>);
impl Default for Buttons {
    fn default() -> Self {
        Self(firmware_shared::input::Buttons::new([
            (40, 15),
            (40, 15),
            (40, 15),
            (40, 15),
            (40, 15),
            (40, 15),
            (50, 50),
            (50, 50),
        ]))
    }
}
impl core::ops::Deref for Buttons {
    type Target = firmware_shared::input::Buttons<8>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
impl core::ops::DerefMut for Buttons {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum Request {
    Run,
    Reverse,
    CrashReset,
    Race,
    TorqueDec,
    TorqueInc,
    SlipDec,
    SlipInc,
    KpDec,
    KpInc,
    KiDec,
    KiInc,
    KdDec,
    KdInc,
    MaxDec,
    MaxInc,
    ILimDec,
    ILimInc,
    TLeakDec,
    TLeakInc,
    FluxDec,
    FluxInc,
    Tc,
    Regen,
    Launch,
    PreloadDec,
    PreloadInc,
    CalibImu,
    CalibImuYaw,
    ImuSelfTest,
    CalibSteer,
    AppsBypass,
    TestPump,
    TestFan,
    Option13,
    TireLimit,
    MappingDec,
    MappingInc,
}
use Request::*;
pub const REQUEST_SIGNALS: &[(Request, Signal)] = &[
    (Run, SWS_REQUEST_RUN),
    (Reverse, SWS_REQUEST_REVERSE),
    (CrashReset, SWS_REQUEST_CRASH_RESET),
    (Race, SWS_REQUEST_RACE_MODE),
    (TorqueDec, SWS_REQUEST_TORQUE_DEC),
    (TorqueInc, SWS_REQUEST_TORQUE_INC),
    (SlipDec, SWS_REQUEST_SLIP_DEC),
    (SlipInc, SWS_REQUEST_SLIP_INC),
    (KpDec, SWS_REQUEST_TC_KP_DEC),
    (KpInc, SWS_REQUEST_TC_KP_INC),
    (KiDec, SWS_REQUEST_TC_KI_DEC),
    (KiInc, SWS_REQUEST_TC_KI_INC),
    (KdDec, SWS_REQUEST_TC_KD_DEC),
    (KdInc, SWS_REQUEST_TC_KD_INC),
    (MaxDec, SWS_REQUEST_TC_MAX_LIM_DEC),
    (MaxInc, SWS_REQUEST_TC_MAX_LIM_INC),
    (ILimDec, SWS_REQUEST_TC_ILIM_DEC),
    (ILimInc, SWS_REQUEST_TC_ILIM_INC),
    (TLeakDec, SWS_REQUEST_TC_TLEAK_MS_DEC),
    (TLeakInc, SWS_REQUEST_TC_TLEAK_MS_INC),
    (FluxDec, SWS_REQUEST_FLUX_WEAKENING_CURRENT_DEC),
    (FluxInc, SWS_REQUEST_FLUX_WEAKENING_CURRENT_INC),
    (Tc, SWS_REQUEST_TRACTION_CONTROL),
    (Regen, SWS_REQUEST_REGEN_ENABLED),
    (Launch, SWS_REQUEST_LAUNCH_CONTROL),
    (PreloadDec, SWS_REQUEST_PRELOAD_TORQUE_DEC),
    (PreloadInc, SWS_REQUEST_PRELOAD_TORQUE_INC),
    (CalibImu, SWS_REQUEST_CALIB_IMU),
    (CalibImuYaw, SWS_REQUEST_CALIB_IMU_YAW),
    (ImuSelfTest, SWS_REQUEST_IMU_SELF_TEST),
    (CalibSteer, SWS_REQUEST_CALIB_STEER_ANGLE),
    (AppsBypass, SWS_REQUEST_APPS_BYPASS),
    (TestPump, SWS_REQUEST_TEST_PUMP),
    (TestFan, SWS_REQUEST_TEST_FAN),
    (TireLimit, SWS_REQUEST_TC_TIRE_MODEL_LIMIT),
    (MappingDec, SWS_REQUEST_TC_MAPPING_DEC),
    (MappingInc, SWS_REQUEST_TC_MAPPING_INC),
];
#[derive(Clone, Copy)]
pub struct Config {
    pub selection: u32,
    pub requests: [Option<Request>; 2],
    pub options: [u32; 2],
    pub source: Option<Signal>,
    pub scale: u32,
    pub unit: u32,
}
const fn config(selection: u32, left: Request, right: Request, options: [u32; 2]) -> Config {
    Config {
        selection,
        requests: [Some(left), Some(right)],
        options,
        source: None,
        scale: 0,
        unit: 0,
    }
}
const fn param(
    selection: u32,
    left: Request,
    right: Request,
    source: Signal,
    scale: u32,
    unit: u32,
) -> Config {
    Config {
        source: Some(source),
        scale,
        unit,
        ..config(
            selection,
            left,
            right,
            [config_option::DEC, config_option::INC],
        )
    }
}
const CONFIGS: [Config; 14] = [
    config(
        config_selection::TC_TIRE_MODEL_LIM,
        TireLimit,
        TireLimit,
        [config_option::ENABLE; 2],
    ),
    config(
        config_selection::TC_MAPPING,
        MappingDec,
        MappingInc,
        [config_option::SNA; 2],
    ),
    config(
        config_selection::TEST_PUMP_FAN,
        TestPump,
        TestFan,
        [config_option::TEST_PUMP, config_option::TEST_FAN],
    ),
    param(
        config_selection::PARAM_TC_KP,
        KpDec,
        KpInc,
        VCFRONT_PARAM_TC_KP,
        config_scale::DIV_1000,
        config_unit::NONE,
    ),
    param(
        config_selection::PARAM_TC_KI,
        KiDec,
        KiInc,
        VCFRONT_PARAM_TC_KI,
        config_scale::DIV_1000,
        config_unit::NONE,
    ),
    param(
        config_selection::PARAM_TC_KD,
        KdDec,
        KdInc,
        VCFRONT_PARAM_TC_KD,
        config_scale::DIV_1000,
        config_unit::NONE,
    ),
    param(
        config_selection::PARAM_TC_MAX_LIM,
        MaxDec,
        MaxInc,
        VCFRONT_PARAM_TC_PID_MAX,
        config_scale::DIV_100,
        config_unit::PERCENT,
    ),
    param(
        config_selection::PARAM_TC_ILIM,
        ILimDec,
        ILimInc,
        VCFRONT_PARAM_TC_ILIM,
        config_scale::DIV_100,
        config_unit::PERCENT,
    ),
    param(
        config_selection::PARAM_TC_TLEAK,
        TLeakDec,
        TLeakInc,
        VCFRONT_PARAM_TC_TLEAK,
        config_scale::DIV_1000,
        config_unit::SECONDS,
    ),
    param(
        config_selection::PARAM_FLUX_WEAKENING_CURRENT,
        FluxDec,
        FluxInc,
        VCREAR_FLUX_WEAKENING_CURRENT,
        config_scale::DIV_10,
        config_unit::AMP,
    ),
    config(
        config_selection::CALIB_DYNAMICS,
        CalibImu,
        CalibImuYaw,
        [config_option::CALIB_IMU, config_option::CALIB_IMU_YAW],
    ),
    config(
        config_selection::CALIB_DYNAMICS2,
        CalibSteer,
        ImuSelfTest,
        [config_option::CALIB_STW_ANGLE, config_option::IMU_SELFTEST],
    ),
    Config {
        requests: [Some(Option13), None],
        ..config(
            config_selection::OPTION13,
            Option13,
            Option13,
            [config_option::ENABLE, config_option::SNA],
        )
    },
    Config {
        requests: [Some(AppsBypass), None],
        ..config(
            config_selection::VEHICLE_CONTROL,
            AppsBypass,
            AppsBypass,
            [config_option::APPS_BYPASS, config_option::SNA],
        )
    },
];
pub struct Driver {
    /// Internal page numbering follows the C implementation: home/buttons/data/config.
    pub page: u8,
    pub config_index: usize,
    pub option13: bool,
    pub requests: u64,
    pub selected: Config,
    nav: Timer,
    config_timer: Timer,
    combos: [Timer; 5],
    crash_lockout: bool,
}
impl Default for Driver {
    fn default() -> Self {
        Self {
            page: 0,
            config_index: 0,
            option13: false,
            requests: 0,
            selected: CONFIGS[0],
            nav: Timer::default(),
            config_timer: Timer::default(),
            combos: [Timer::default(); 5],
            crash_lockout: false,
        }
    }
}
impl Driver {
    pub fn requested(&self, request: Request) -> bool {
        self.requests & (1 << request as u8) != 0
    }
    fn set(&mut self, request: Request, active: bool) {
        if active {
            self.requests |= 1 << request as u8;
        }
    }
    pub fn launch_active(network: &Network, now: u32) -> bool {
        matches!(network.raw(VCFRONT_LAUNCH_CONTROL_STATE, now), Some(2..=5))
    }
    pub fn screen(&self, network: &Network, now: u32) -> u32 {
        if Self::launch_active(network, now) {
            screen_page::LAUNCH
        } else {
            self.page as u32 + 1
        }
    }
    pub fn update(&mut self, buttons: &Buttons, network: &Network, now: u32) {
        let b = buttons.state.map(|s| s.pressed);
        let db = buttons.state.map(|s| s.debouncing);
        let [prev, dec, sl_dec, next, inc, sl_inc, tc, regen] = b;
        let axis_db = db[1] || db[2] || db[4] || db[5];
        let outside_config = self.page != 3;
        let run = prev && next && outside_config && !db[0] && !db[3];
        let reverse = dec && inc && sl_dec && sl_inc && outside_config && !axis_db;
        let race = sl_dec && sl_inc && outside_config && !reverse && !axis_db;
        let launch = dec && inc && outside_config && !reverse && !axis_db;
        let crash = network
            .raw(VCPDU_CRASH_SENSOR_STATE, now)
            .is_some_and(|v| v != crash_sensor_state::OK);
        let option_requested = self.requested(Option13);
        self.requests = 0;
        let active = [
            self.combos[0].hold(run, now, 250),
            self.combos[1].hold(reverse && !crash && !self.crash_lockout, now, 500),
            self.combos[2].hold(reverse && crash, now, 5000),
            self.combos[3].hold(race, now, 500),
            self.combos[4].hold(launch, now, 500),
        ];
        for (r, on) in [Run, Reverse, CrashReset, Race, Launch]
            .into_iter()
            .zip(active)
        {
            self.set(r, on);
        }
        self.set(Tc, tc);
        self.set(Regen, regen);
        if Self::launch_active(network, now) {
            if !db[0] && !db[3] && (prev ^ next) {
                self.set(PreloadDec, prev);
                self.set(PreloadInc, next);
            }
        } else if !db[0] && !db[3] && (prev ^ next) {
            if !self.nav.running(now, 250) {
                self.page = if prev {
                    self.page.saturating_sub(1)
                } else {
                    (self.page + 1).min(3)
                };
                self.nav.restart(now);
            }
        } else {
            self.nav.stop();
        }
        self.selected = CONFIGS[self.config_index];
        if self.config_index == 0 {
            let opt = match network.raw(VCFRONT_PARAM_TC_TIRE_MODEL_LIMIT, now) {
                Some(digital_status::ON) => config_option::DISABLE,
                Some(_) => config_option::ENABLE,
                None => config_option::SNA,
            };
            self.selected.options = [opt; 2];
        } else if self.config_index == 1 {
            self.selected.options = match network.raw(VCFRONT_PARAM_TC_MAPPING, now) {
                Some(tc_mapping::CUSTOM) => [
                    config_option::TC_MAPPING_100_NM,
                    config_option::TC_MAPPING_150_NM,
                ],
                Some(tc_mapping::MAP_100_NM) => [
                    config_option::TC_MAPPING_CUSTOM,
                    config_option::TC_MAPPING_150_NM,
                ],
                Some(tc_mapping::MAP_150_NM) => [
                    config_option::TC_MAPPING_100_NM,
                    config_option::TC_MAPPING_CUSTOM,
                ],
                _ => [config_option::SNA; 2],
            };
        }
        if self.page == 1 && !axis_db && (inc ^ dec) {
            self.set(TorqueInc, inc);
            self.set(TorqueDec, dec);
        }
        if self.page == 3 {
            if dec {
                if let Some(r) = self.selected.requests[0] {
                    self.set(r, true);
                }
            }
            if inc {
                if let Some(r) = self.selected.requests[1] {
                    self.set(r, true);
                }
            }
        }
        if self.requested(Option13) && !option_requested {
            self.option13 = !self.option13;
        }
        if self.config_index == 12 {
            self.selected.options[0] = if self.option13 {
                config_option::DISABLE
            } else {
                config_option::ENABLE
            };
        }
        if !axis_db && (sl_inc ^ sl_dec) {
            if self.page == 1 {
                self.set(SlipInc, sl_inc);
                self.set(SlipDec, sl_dec);
            } else if self.page == 3 && !self.config_timer.running(now, 500) {
                if sl_dec {
                    self.config_index = self.config_index.saturating_sub(1);
                } else if self.config_index < 13 && (self.config_index < 12 || self.option13) {
                    self.config_index += 1;
                }
                self.config_timer.restart(now);
                // Match legacy ordering: option labels update on the next 100 Hz tick.
            }
        } else {
            self.config_timer.stop();
        }
        if active[2] {
            self.crash_lockout = true;
        }
        if !b[..6].iter().any(|b| *b) {
            self.crash_lockout = false;
        }
    }
    pub fn display_value(&self, network: &Network, now: u32) -> u32 {
        let Some(signal) = self.selected.source else {
            return 0;
        };
        let multiplier = match self.selected.scale {
            config_scale::DIV_10 => 10.,
            config_scale::DIV_100 => 100.,
            config_scale::DIV_1000 => 1000.,
            _ => 1.,
        };
        (network.value(signal, now).unwrap_or(0.) * multiplier) as u16 as u32
    }
}
