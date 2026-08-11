import pytest

from sim.models.controllers.vcpdu import Vn9008Channel
from sim.models.controllers.vcpdu.fixtures import vcpdu_cluster


def test_vcpdu_boots_and_cycles_to_glv_on(vcpdu_cluster):
    VehicleState = vcpdu_cluster.vcpdu.can.enums.VehicleState
    observed = []

    def record_state():
        return (
            vcpdu_cluster.vcpdu.record_latest_vehicle_state(observed)
            == VehicleState.ON_GLV
        )

    vcpdu_cluster.run_until(
        record_state,
        timeout=500,
        step=10,
        message="vcpdu should boot to ON_GLV",
    )

    assert observed == [
        VehicleState.INIT,
        VehicleState.ON_GLV,
    ]


@pytest.mark.parametrize(
    ("controller", "wake_state_name"),
    [
        pytest.param("sws", "SNA", id="sws-sna"),
        pytest.param("sws", "NOK_TO_SLEEP", id="sws-nok-to-sleep"),
        pytest.param("sws", "ALARM", id="sws-alarm"),
        pytest.param("vcfront", "SNA", id="vcfront-sna"),
        pytest.param("vcfront", "NOK_TO_SLEEP", id="vcfront-nok-to-sleep"),
        pytest.param("vcfront", "ALARM", id="vcfront-alarm"),
    ],
)
def test_vcpdu_sleeps_then_wakes_from_waking_controller_sleepable_state(
    vcpdu_cluster,
    controller,
    wake_state_name,
):
    VehicleState = vcpdu_cluster.vcpdu.can.enums.VehicleState
    SleepFollowerState = vcpdu_cluster.vcpdu.can.enums.SleepFollowerState
    vcpdu = vcpdu_cluster.vcpdu
    observed = []
    wake_state = getattr(SleepFollowerState, wake_state_name)

    controllers = vcpdu.waking_sleepable_controllers()
    assert controllers == ("sws", "vcfront")
    sleepable_inputs = vcpdu.periodic_all_waking_controllers_sleepable(
        SleepFollowerState.OK_TO_SLEEP,
        period=100,
    )
    vcpdu_cluster.add_components(*sleepable_inputs.values())

    vcpdu_cluster.run_until(
        lambda: vcpdu.record_latest_vehicle_state(observed) == VehicleState.ON_GLV,
        timeout=500,
        step=20,
        message="vcpdu should boot to ON_GLV before it can sleep",
    )

    vcpdu_cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_GLV

    vcpdu_cluster.run_until(
        lambda: vcpdu.record_latest_vehicle_state(observed) == VehicleState.SLEEP,
        timeout=16 * 60000,
        step=10000,
        message="vcpdu should enter SLEEP when all waking controllers are OK to sleep",
    )

    sleepable_inputs[controller].set(**{f"{controller.upper()}_sleepable": wake_state})
    before_wake = len(observed)
    vcpdu_cluster.run_until(
        lambda: vcpdu.record_latest_vehicle_state(observed) == VehicleState.ON_GLV,
        timeout=1000,
        step=20,
        message=f"vcpdu should wake from {controller} {wake_state.name}",
    )

    assert observed[before_wake:] == [
        VehicleState.INIT,
        VehicleState.ON_GLV,
    ]


@pytest.mark.parametrize(
    "hsd_channel",
    [
        pytest.param(Vn9008Channel.PUMP, id="pump"),
        pytest.param(Vn9008Channel.FAN, id="fan"),
    ],
)
def test_driver_cooling_request_toggles_and_latches_load_current(
    vcpdu_cluster,
    hsd_channel,
):
    vcpdu = vcpdu_cluster.vcpdu

    vcpdu_cluster.run_until(
        lambda: vcpdu.latest_hsd_duty_cycle(hsd_channel) == 0
        and vcpdu.latest_hsd_current(hsd_channel) == 0,
        timeout=500,
        step=20,
        message="VCPDU HSD load should report off before a driver request",
    )

    assert vcpdu.request_test_hsd(hsd_channel, True)
    vcpdu_cluster.run_until(
        lambda: _positive(vcpdu.latest_hsd_duty_cycle(hsd_channel))
        and _positive(vcpdu.latest_hsd_current(hsd_channel)),
        timeout=1000,
        step=20,
        message="VCPDU HSD load should report non-zero current when requested on",
    )

    assert vcpdu.request_test_hsd(hsd_channel, False)
    vcpdu_cluster.run_for(250)
    assert _positive(vcpdu.latest_hsd_duty_cycle(hsd_channel))
    assert _positive(vcpdu.latest_hsd_current(hsd_channel))

    assert vcpdu.request_test_hsd(hsd_channel, True)
    vcpdu_cluster.run_until(
        lambda: vcpdu.latest_hsd_duty_cycle(hsd_channel) == 0
        and vcpdu.latest_hsd_current(hsd_channel) == 0,
        timeout=1000,
        step=20,
        message="VCPDU HSD load should report zero current after a second driver request toggles it off",
    )


def _positive(value: float | None) -> bool:
    return value is not None and value > 0
