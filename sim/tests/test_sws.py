import pytest

from rig import RustSchedulerCallbacks
from sim.bindings.firmware.can import CanPacket
from sim.models.controllers.sws import DigitalStatus, SwsButton, SwsRequest
from sim.models.controllers.sws.fixtures import sws_cluster


_BUTTON_DEBUG_SIGNALS = {
    SwsButton.LEFT_TOP: "SWS_leftTop",
    SwsButton.LEFT_MID: "SWS_leftMid",
    SwsButton.LEFT_BOT: "SWS_leftBot",
    SwsButton.RIGHT_TOP: "SWS_rightTop",
    SwsButton.RIGHT_MID: "SWS_rightMid",
    SwsButton.RIGHT_BOT: "SWS_rightBot",
    SwsButton.LEFT_TOGGLE: "SWS_leftToggle",
    SwsButton.RIGHT_TOGGLE: "SWS_rightToggle",
}


@pytest.mark.parametrize("button,signal", tuple(_BUTTON_DEBUG_SIGNALS.items()))
def test_sws_button_is_reported_by_firmware_can(sws_cluster, button, signal):
    sws = sws_cluster.sws
    assert isinstance(sws.scheduler_callbacks(), RustSchedulerCallbacks)

    sws.press_button(button)
    sws_cluster.run_for(225)

    debug = sws.can.latest("SWS_inputDebugStatus", bus="veh")
    assert debug is not None
    assert getattr(debug, signal) == DigitalStatus.ON

    sws.release_button(button)
    sws_cluster.run_for(225)
    debug = sws.can.latest("SWS_inputDebugStatus", bus="veh")
    assert debug is not None
    assert getattr(debug, signal) == DigitalStatus.OFF


@pytest.mark.parametrize(
    ("driver_request_type", "signal", "settle_ms"),
    (
        (SwsRequest.RUN, "SWS_requestRun", 350),
        (SwsRequest.RACE, "SWS_requestRaceMode", 650),
        (SwsRequest.LAUNCH_CONTROL, "SWS_requestLaunchControl", 650),
        (SwsRequest.REVERSE, "SWS_requestReverse", 750),
        (SwsRequest.TRACTION_CONTROL, "SWS_requestTractionControl", 225),
        (SwsRequest.REGEN, "SWS_requestRegenEnabled", 225),
    ),
)
def test_sws_driver_combinations_generate_requests(
    sws_cluster, driver_request_type, signal, settle_ms
):
    sws = sws_cluster.sws

    # set_request applies the complete physical combination before any
    # scheduler step. The firmware therefore sees one stable combination,
    # rather than a sequence of intermediary button presses.
    sws.assert_request(driver_request_type)
    sws_cluster.run_for(settle_ms)

    driver_request = sws.can.latest("SWS_driverRequest", bus="veh")
    assert driver_request is not None
    assert getattr(driver_request, signal) == DigitalStatus.ON

    sws.clear_request(driver_request_type)
    sws_cluster.run_for(100)
    driver_request = sws.can.latest("SWS_driverRequest", bus="veh")
    assert driver_request is not None
    assert getattr(driver_request, signal) == DigitalStatus.OFF


@pytest.mark.parametrize(
    ("driver_request_type", "signal"),
    (
        (SwsRequest.TORQUE_DEC, "SWS_requestTorqueDec"),
        (SwsRequest.TORQUE_INC, "SWS_requestTorqueInc"),
        (SwsRequest.SLIP_DEC, "SWS_requestSlipDec"),
        (SwsRequest.SLIP_INC, "SWS_requestSlipInc"),
    ),
)
def test_sws_axis_buttons_generate_requests_on_buttons_page(
    sws_cluster, driver_request_type, signal
):
    sws = sws_cluster.sws

    # Axis requests are active on the firmware's BUTTONS page. Navigate there
    # with a complete press/release cycle before applying the test request.
    sws.press_button(SwsButton.RIGHT_TOP)
    sws_cluster.run_for(75)
    sws.release_button(SwsButton.RIGHT_TOP)
    sws_cluster.run_for(300)

    sws.assert_request(driver_request_type)
    sws_cluster.run_for(125)
    driver_request = sws.can.latest("SWS_driverRequest", bus="veh")
    assert driver_request is not None
    assert getattr(driver_request, signal) == DigitalStatus.ON


@pytest.mark.parametrize(
    ("driver_request_types", "expected"),
    (
        (
            (SwsRequest.TORQUE_DEC, SwsRequest.TORQUE_INC),
            {
                "SWS_requestTorqueDec": DigitalStatus.OFF,
                "SWS_requestTorqueInc": DigitalStatus.OFF,
            },
        ),
        (
            (SwsRequest.SLIP_DEC, SwsRequest.SLIP_INC),
            {
                "SWS_requestSlipDec": DigitalStatus.OFF,
                "SWS_requestSlipInc": DigitalStatus.OFF,
            },
        ),
        (
            (SwsRequest.TORQUE_INC, SwsRequest.SLIP_INC),
            {
                "SWS_requestTorqueInc": DigitalStatus.ON,
                "SWS_requestSlipInc": DigitalStatus.ON,
            },
        ),
    ),
)
def test_sws_axis_combinations_are_seen_as_one_input_state(
    sws_cluster, driver_request_types, expected
):
    sws = sws_cluster.sws

    sws.press_button(SwsButton.RIGHT_TOP)
    sws_cluster.run_for(75)
    sws.release_button(SwsButton.RIGHT_TOP)
    sws_cluster.run_for(300)

    # Assert both inputs before advancing the firmware. Opposing buttons must
    # be rejected together; independent axes may both remain asserted.
    for driver_request_type in driver_request_types:
        sws.assert_request(driver_request_type)
    sws_cluster.run_for(125)

    driver_request = sws.can.latest("SWS_driverRequest", bus="veh")
    assert driver_request is not None
    for signal, state in expected.items():
        assert getattr(driver_request, signal) == state


def test_sws_reverse_combination_does_not_emit_intermediary_race_or_launch(
    sws_cluster,
):
    sws = sws_cluster.sws

    sws.assert_request(SwsRequest.REVERSE)
    sws_cluster.run_for(350)

    request = sws.can.latest("SWS_driverRequest", bus="veh")
    assert request is not None
    assert request.SWS_requestRaceMode == DigitalStatus.OFF
    assert request.SWS_requestLaunchControl == DigitalStatus.OFF
    assert request.SWS_requestReverse == DigitalStatus.OFF

    sws_cluster.run_for(350)
    request = sws.can.latest("SWS_driverRequest", bus="veh")
    assert request is not None
    assert request.SWS_requestRaceMode == DigitalStatus.OFF
    assert request.SWS_requestLaunchControl == DigitalStatus.OFF
    assert request.SWS_requestReverse == DigitalStatus.ON


def _drain_can(sws):
    frames = []
    while (event := sws.can.recv("veh")) is not None:
        frames.append((event.timestamp_ns, event.packet.id, event.packet.payload))
    return frames


def test_rtic_preserves_fractional_timer_steps(sws_cluster):
    sws = sws_cluster.sws
    sws.run_for(750, unit="us")
    assert sws.tick_count == 0
    sws.run_for(250, unit="us")
    assert sws.tick_count == 1
    sws.run_for(1500, unit="us")
    assert sws.tick_count == 2
    sws.run_for(500, unit="us")
    assert sws.tick_count == 3


def test_rtic_coarse_steps_match_every_timer_interrupt(sws_cluster):
    sws = sws_cluster.sws
    sws.press_button(SwsButton.LEFT_TOP)
    sws.run_for(750)
    assert sws.tick_count == 750
    coarse = _drain_can(sws)
    assert [at for at, frame_id, _ in coarse if frame_id == 0x8F] == [
        ms * 1_000_000 for ms in range(50, 751, 50)
    ]

    sws.reset()
    sws.press_button(SwsButton.LEFT_TOP)
    for _ in range(750):
        sws.run_for(1)
    assert _drain_can(sws) == coarse


def test_rtic_can_interrupt_queues_before_timer_and_counts_overflow(sws_cluster):
    sws = sws_cluster.sws
    for _ in range(40):
        assert sws.can.send("veh", 0x651, bytes([2, 0x3E, 0, 0, 0, 0, 0, 0]))
    sws.run_for(0)
    assert sws.tick_count == 0
    assert sws.rx_dropped == 8
    assert _drain_can(sws) == []

    # Each timer invocation services at most three transmit mailboxes; the
    # diagnostic FIFO must retain all 32 accepted responses in order.
    sws.run_for(11)
    replies = _drain_can(sws)
    assert len(replies) == 32
    assert all(frame_id == 0x691 and data[:3] == bytes([2, 0x7E, 0])
               for _, frame_id, data in replies)
    assert [at for at, _, _ in replies] == [
        (index // 3 + 1) * 1_000_000 for index in range(32)
    ]


def test_rtic_uds_reset_keeps_reply_and_external_input(sws_cluster):
    sws = sws_cluster.sws
    sws.press_button(SwsButton.LEFT_TOP)
    assert sws.can.send("veh", 0x651, bytes([2, 0x11, 1, 0, 0, 0, 0, 0]))
    sws.run_for(9)
    assert sws.tick_count == 9
    assert sws.reset_count == 0
    sws.run_for(1)
    assert sws.tick_count == 0
    assert sws.reset_count == 1
    assert _drain_can(sws) == [(1_000_000, 0x691, bytes([2, 0x51, 1, 0, 0, 0, 0, 0]))]

    sws.run_for(200)
    debug_id = sws.can.tx_message("SWS_inputDebugStatus", bus="veh").id
    debug_frames = [data for _, frame_id, data in _drain_can(sws) if frame_id == debug_id]
    assert debug_frames
    debug = sws.can.decode(
        sws.can.tx_message("SWS_inputDebugStatus", bus="veh"),
        CanPacket.from_payload(debug_id, debug_frames[-1]),
    )
    assert debug.SWS_leftTop == DigitalStatus.ON
