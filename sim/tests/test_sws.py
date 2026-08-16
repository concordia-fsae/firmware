import pytest

from rig import RustSchedulerCallbacks
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
