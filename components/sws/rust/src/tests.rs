use crate::{
    app::App,
    can::{Frame, Network},
    input::{Buttons, Request, Timer},
    protocol::*,
    uds::{Descriptor, Diagnostics},
};
fn advance(app: &mut App, raw: [bool; 8], ms: u32) {
    for _ in 0..ms {
        app.tick(raw);
    }
}
fn combo(indices: &[usize]) -> [bool; 8] {
    let mut raw = [false; 8];
    for &i in indices {
        raw[i] = true;
    }
    raw
}
fn receive(app: &mut App, signal: crate::can::Signal, value: u32) {
    let mut data = [0; 8];
    signal.set(&mut data, value);
    app.receive(
        MESSAGES[signal.message].frame(data, 0),
        Descriptor::default(),
    );
}
#[test]
fn asymmetric_debounce_rejects_bounce() {
    let mut b = Buttons::default();
    let on = combo(&[0]);
    b.sample(on, 0);
    b.sample(on, 39);
    assert!(!b.state[0].pressed);
    assert!(b.state[0].debouncing);
    b.sample([false; 8], 40);
    b.sample(on, 41);
    b.sample(on, 80);
    assert!(!b.state[0].pressed);
    b.sample(on, 81);
    assert!(b.state[0].pressed);
    b.sample([false; 8], 82);
    b.sample([false; 8], 96);
    assert!(b.state[0].pressed);
    b.sample([false; 8], 97);
    assert!(!b.state[0].pressed);
    b.sample(combo(&[6]), 100);
    b.sample(combo(&[6]), 149);
    assert!(!b.state[6].pressed);
    b.sample(combo(&[6]), 150);
    assert!(b.state[6].pressed);
}
#[test]
fn run_requires_button_debounce_and_combo_hold() {
    let mut app = App::default();
    let raw = combo(&[0, 3]);
    advance(&mut app, raw, 290);
    assert!(!app.driver.requested(Request::Run));
    advance(&mut app, raw, 10);
    assert!(app.driver.requested(Request::Run));
    assert_eq!(app.driver.page, 0);
    advance(&mut app, [false; 8], 30);
    assert!(!app.driver.requested(Request::Run));
}
#[test]
fn reverse_has_priority_over_race_and_launch() {
    let mut app = App::default();
    let raw = combo(&[1, 2, 4, 5]);
    advance(&mut app, raw, 540);
    for r in [Request::Reverse, Request::Race, Request::Launch] {
        assert!(!app.driver.requested(r));
    }
    advance(&mut app, raw, 20);
    assert!(app.driver.requested(Request::Reverse));
    assert!(!app.driver.requested(Request::Race));
    assert!(!app.driver.requested(Request::Launch));
}
#[test]
fn race_and_launch_hold_and_release() {
    for (indices, request) in [(&[2, 5][..], Request::Race), (&[1, 4][..], Request::Launch)] {
        let mut app = App::default();
        let raw = combo(indices);
        advance(&mut app, raw, 540);
        assert!(!app.driver.requested(request));
        advance(&mut app, raw, 20);
        assert!(app.driver.requested(request));
        advance(&mut app, [false; 8], 30);
        assert!(!app.driver.requested(request));
    }
}
#[test]
fn opposing_axes_cancel_but_independent_axes_work() {
    for (raw, inc, dec, slip) in [
        (combo(&[1, 4]), false, false, false),
        (combo(&[2, 5]), false, false, false),
        (combo(&[4, 5]), true, false, true),
    ] {
        let mut app = App::default();
        app.driver.page = 1;
        advance(&mut app, raw, 125);
        assert_eq!(app.driver.requested(Request::TorqueInc), inc);
        assert_eq!(app.driver.requested(Request::TorqueDec), dec);
        assert_eq!(app.driver.requested(Request::SlipInc), slip);
    }
}
#[test]
fn launch_feedback_reassigns_navigation_to_preload_then_expires() {
    let mut app = App::default();
    receive(
        &mut app,
        VCFRONT_LAUNCH_CONTROL_STATE,
        launch_control_state::PRELOAD,
    );
    advance(&mut app, combo(&[3]), 75);
    assert_eq!(app.driver.page, 0);
    assert!(app.driver.requested(Request::PreloadInc));
    assert_eq!(
        app.driver.screen(&app.network, app.now),
        screen_page::LAUNCH
    );
    advance(&mut app, combo(&[3]), 50);
    assert_eq!(app.driver.page, 1);
    assert!(!app.driver.requested(Request::PreloadInc));
}
#[test]
fn page_repeat_is_clamped_and_config_blocks_combos() {
    let mut app = App::default();
    advance(&mut app, combo(&[3]), 1000);
    assert_eq!(app.driver.page, 3);
    advance(&mut app, combo(&[0, 1, 2, 3, 4, 5]), 700);
    for r in [
        Request::Run,
        Request::Reverse,
        Request::Race,
        Request::Launch,
    ] {
        assert!(!app.driver.requested(r));
    }
    advance(&mut app, [false; 8], 30);
    advance(&mut app, combo(&[0]), 1000);
    assert_eq!(app.driver.page, 0);
}
#[test]
fn crash_reset_needs_five_seconds_and_release_before_reverse() {
    let mut app = App::default();
    let raw = combo(&[1, 2, 4, 5]);
    for _ in 0..50 {
        receive(
            &mut app,
            VCPDU_CRASH_SENSOR_STATE,
            crash_sensor_state::CRASHED,
        );
        advance(&mut app, raw, 100);
    }
    assert!(!app.driver.requested(Request::CrashReset));
    assert!(!app.driver.requested(Request::Reverse));
    receive(
        &mut app,
        VCPDU_CRASH_SENSOR_STATE,
        crash_sensor_state::CRASHED,
    );
    advance(&mut app, raw, 60);
    assert!(app.driver.requested(Request::CrashReset));
    for _ in 0..7 {
        receive(&mut app, VCPDU_CRASH_SENSOR_STATE, crash_sensor_state::OK);
        advance(&mut app, raw, 100);
    }
    assert!(!app.driver.requested(Request::Reverse));
    advance(&mut app, [false; 8], 30);
    advance(&mut app, raw, 600);
    assert!(app.driver.requested(Request::Reverse));
}
#[test]
fn config_option13_unlock_is_edge_triggered() {
    let mut app = App::default();
    app.driver.page = 3;
    app.driver.config_index = 12;
    advance(&mut app, combo(&[5]), 100);
    assert_eq!(app.driver.config_index, 12);
    advance(&mut app, [false; 8], 30);
    advance(&mut app, combo(&[1]), 100);
    assert!(app.driver.option13);
    advance(&mut app, combo(&[1]), 600);
    assert!(app.driver.option13);
    advance(&mut app, [false; 8], 30);
    advance(&mut app, combo(&[5]), 100);
    assert_eq!(app.driver.config_index, 13);
    assert_eq!(
        app.driver.selected.selection,
        config_selection::VEHICLE_CONTROL
    );
}
#[test]
fn config_mapping_and_numerical_values_follow_can() {
    let mut app = App::default();
    app.driver.page = 3;
    app.driver.config_index = 1;
    receive(&mut app, VCFRONT_PARAM_TC_MAPPING, tc_mapping::CUSTOM);
    advance(&mut app, [false; 8], 10);
    assert_eq!(
        app.driver.selected.options,
        [
            config_option::TC_MAPPING_100_NM,
            config_option::TC_MAPPING_150_NM
        ]
    );
    app.driver.config_index = 3;
    receive(&mut app, VCFRONT_PARAM_TC_KP, 1234);
    advance(&mut app, [false; 8], 10);
    assert_eq!(app.driver.display_value(&app.network, app.now), 1234);
}
#[test]
fn checksum_length_and_timeout_are_enforced_including_tick_wrap() {
    let mut network = Network::default();
    let msg = MESSAGES[MSG_VCFRONT_TORQUE_MANAGER];
    let mut data = [0; 8];
    VCFRONT_GEAR.set(&mut data, gear::REVERSE);
    let frame = msg.frame(data, 0);
    assert!(network.receive(frame, u32::MAX - 50));
    assert_eq!(network.raw(VCFRONT_GEAR, 49), Some(gear::REVERSE));
    assert_eq!(network.raw(VCFRONT_GEAR, 50), None);
    let mut bad = frame;
    bad.data[0] ^= 1;
    assert!(!network.receive(bad, 50));
    bad = frame;
    bad.len -= 1;
    assert!(!network.receive(bad, 50));
    let mut timer = Timer::default();
    assert!(!timer.hold(true, u32::MAX - 5, 10));
    assert!(timer.hold(true, 4, 10));
}
#[test]
fn warning_rotation_remembers_cleared_unseen_warnings_and_prioritizes_alerts() {
    let mut app = App::default();
    let mut data = [0; 8];
    VCPDU_LOW_VOLTAGE.set(&mut data, 1);
    VCPDU_IMU_UNCALIBRATED.set(&mut data, 1);
    app.receive(
        MESSAGES[MSG_VCPDU_FAULTS].frame(data, 0),
        Descriptor::default(),
    );
    receive(&mut app, VCPDU_CRASH_SENSOR_STATE, crash_sensor_state::OK);
    advance(&mut app, [false; 8], 100);
    assert_eq!(app.screen.warning, screen_warnings::LOW_GLV);
    app.receive(
        MESSAGES[MSG_VCPDU_FAULTS].frame([0; 8], 0),
        Descriptor::default(),
    );
    advance(&mut app, [false; 8], 2000);
    assert_eq!(app.screen.warning, screen_warnings::IMU_UNCALIBRATED);
    receive(&mut app, VCREAR_MC_CALIBRATING_RESOLVER, 1);
    advance(&mut app, [false; 8], 100);
    assert_eq!(app.screen.alert, screen_alerts::RESOLVER_CALIBRATING);
}
fn diagnostic(payload: &[u8]) -> Frame {
    let mut data = [0; 8];
    data[0] = payload.len() as u8;
    data[1..=payload.len()].copy_from_slice(payload);
    Frame {
        id: 0x651,
        len: 8,
        data,
    }
}
#[test]
fn deployment_diagnostics_match_conuds_wire_conventions() {
    let mut uds = Diagnostics::new(
        MESSAGES[MSG_UDSCLIENT_SWS_UDS_REQUEST].id,
        MESSAGES[MSG_SWS_UDS_RESPONSE].id,
    );
    let desc = Descriptor {
        start: 0x08002100,
        end: 0x08004500,
        crc_location: 0x08004500,
        crc: 0x12345678,
        component: 9,
        variant: 0,
    };
    let response = uds.receive(diagnostic(&[0x22, 1, 1]), 0, desc).unwrap();
    assert_eq!(&response.data[..3], &[2, 0x62, 1]);
    let response = uds.receive(diagnostic(&[0x22, 0, 0]), 0, desc).unwrap();
    assert_eq!(&response.data[..6], &[5, 0x62, 0, 0x21, 0, 8]);
    uds.receive(diagnostic(&[0x10, 3]), 10, desc);
    assert_eq!(uds.session, 3);
    let response = uds.receive(diagnostic(&[0x22, 2, 1]), 20, desc).unwrap();
    assert_eq!(&response.data[..3], &[2, 0x62, 3]);
    assert!(uds.receive(diagnostic(&[0x3e, 0x80]), 30, desc).is_none());
    uds.tick(129);
    assert_eq!(uds.session, 3);
    uds.tick(130);
    assert_eq!(uds.session, 1);
    let response = uds.receive(diagnostic(&[0x11, 1]), 6000, desc).unwrap();
    assert_eq!(&response.data[..3], &[2, 0x51, 1]);
    assert!(!uds.reset_due(6009));
    assert!(uds.reset_due(6010));
    let response = uds.receive(diagnostic(&[0x22, 0]), 7000, desc).unwrap();
    assert_eq!(&response.data[..4], &[3, 0x7f, 0x22, 0x13]);
}
#[test]
fn isotp_handles_multiframe_and_rejects_bad_sequence_and_oversize() {
    let mut uds = Diagnostics::new(
        MESSAGES[MSG_UDSCLIENT_SWS_UDS_REQUEST].id,
        MESSAGES[MSG_SWS_UDS_RESPONSE].id,
    );
    let desc = Descriptor::default();
    let ff = Frame {
        id: 0x651,
        len: 8,
        data: [0x10, 8, 0x22, 0, 0, 0, 0, 0],
    };
    assert_eq!(uds.receive(ff, 0, desc).unwrap().data[0], 0x30);
    let cf = Frame {
        id: 0x651,
        len: 8,
        data: [0x21, 0, 0, 0, 0, 0, 0, 0],
    };
    assert_eq!(
        &uds.receive(cf, 1, desc).unwrap().data[..4],
        &[3, 0x7f, 0x22, 0x13]
    );
    uds.receive(ff, 2, desc);
    let mut bad = cf;
    bad.data[0] = 0x22;
    assert!(uds.receive(bad, 3, desc).is_none());
    assert!(uds.receive(cf, 4, desc).is_none());
    let mut large = ff;
    large.data[1] = 129;
    assert_eq!(uds.receive(large, 5, desc).unwrap().data[0], 0x32);
}
#[test]
fn periodic_can_matches_dbc_default_bits_and_schedule() {
    let mut app = App::default();
    let mut request = None;
    let mut count = 0;
    for _ in 0..1000 {
        app.tick([false; 8]);
        app.transmit_due(|f| {
            if f.id == 0x8f {
                count += 1;
                request = Some(f);
            }
        });
    }
    assert_eq!(count, 20);
    let request = request.unwrap();
    assert_eq!(request.len, 7);
    // Golden values from the legacy Yamcan little-endian driverRequest layout.
    assert_eq!(
        &request.data[..7],
        &[0x55, 0xa5, 0xaa, 0xaa, 0xaa, 0x6a, 0x0d]
    );
    assert!(MESSAGES[MSG_SWS_DRIVER_REQUEST].valid(&request.into()));
    assert_eq!(
        app.telemetry(MSG_SWS_INPUT_DEBUG_STATUS),
        [0x55, 0x55, 0x55, 0x55, 0, 0, 0, 0]
    );
}

#[test]
fn sleep_inhibition_extends_after_expiry_and_on_button_activity() {
    let mut app = App::default();
    app.now = 900_000;
    assert_eq!(
        SWS_SLEEPABLE.raw(&app.telemetry(MSG_SWS_SLEEP)),
        sleep_follower_state::OK_TO_SLEEP as u64
    );
    app.receive(diagnostic(&[0x3e, 0x80]), Descriptor::default());
    assert_eq!(
        SWS_SLEEPABLE.raw(&app.telemetry(MSG_SWS_SLEEP)),
        sleep_follower_state::NOK_TO_SLEEP as u64
    );
    app.now = 1_200_000;
    assert_eq!(
        SWS_SLEEPABLE.raw(&app.telemetry(MSG_SWS_SLEEP)),
        sleep_follower_state::OK_TO_SLEEP as u64
    );
    advance(&mut app, combo(&[3]), 100);
    assert_eq!(
        SWS_SLEEPABLE.raw(&app.telemetry(MSG_SWS_SLEEP)),
        sleep_follower_state::NOK_TO_SLEEP as u64
    );
    receive(&mut app, VCPDU_VEHICLE_STATE, vehicle_state::SLEEP);
    advance(&mut app, [false; 8], 10);
    assert!(app.sleeping);
    advance(&mut app, [false; 8], 1200);
    assert!(!app.sleeping);
}

#[test]
fn signed_can_values_use_resolved_scale_and_offset() {
    let mut app = App::default();
    receive(&mut app, VCPDU_CRASH_SENSOR_ACCEL_MAX, 0x7fff);
    assert_eq!(
        app.network.value(VCPDU_CRASH_SENSOR_ACCEL_MAX, app.now),
        Some(-0.01)
    );
}

#[test]
fn yamcan_shared_codec_handles_full_width_signed_and_checksums() {
    use crate::protocol::codec;
    let mut data = [0; 8];
    codec::set_bits(&mut data, 0, 64, u64::MAX);
    assert_eq!(codec::get_bits(&data, 0, 64), u64::MAX);
    assert_eq!(codec::decode_raw(u64::MAX, 64, true, 0.5, 2.0), 1.5);
    assert_eq!(codec::encode_raw(-3.0, 0.5, 2.0, -4.0, 5.0) as i64, -10);
    assert_eq!(codec::encode_raw(99.0, 0.5, 2.0, -4.0, 5.0), 6);
    // Independent checksum vector: seed 0x1234 + bytes 1..7 = 0x1250.
    data = [1, 2, 3, 4, 5, 6, 7, 0xff];
    codec::apply_checksum(&mut data, 8, 56, 8, 0x1234);
    assert_eq!(data[7], 0x50);
    assert!(codec::valid_checksum(&data, 8, 56, 8, 0x1234));
    data[0] ^= 1;
    assert!(!codec::valid_checksum(&data, 8, 56, 8, 0x1234));
}

#[test]
fn yamcan_generated_wrapper_encodes_physical_signed_values() {
    let signal = VCPDU_CRASH_SENSOR_ACCEL_MAX;
    let mut data = [0; 8];
    signal.encode(&mut data, -0.25);
    assert_eq!(signal.raw(&data), 0x7fe7);
    assert_eq!(signal.value(signal.raw(&data)), -0.25);
}
