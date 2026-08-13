import pytest

from sim.models.controllers.vcrear.fixtures import vcrear_cluster


def brake_light_state(vcrear):
    return vcrear.can.latest_signal(
        "VCREAR_outputState",
        "VCREAR_brakeLightState",
        bus="veh",
    )


def send_vcfront_brake_position(vcrear, brake_position: float) -> bool:
    message = vcrear.can.message("VCFRONT_pedalPosition", bus="veh")
    return vcrear.can.send(
        message,
        VCFRONT_brakePosition=brake_position,
    )


@pytest.mark.parametrize(
    "brake_position,brake_light_on",
    [
        pytest.param(0, False, id="brake-light-off-at-zero-percent"),
        pytest.param(10, False, id="brake-light-off-at-threshold"),
        pytest.param(12, True, id="brake-light-on-above-threshold"),
        pytest.param(100, True, id="brake-light-on-at-full-pedal"),
    ],
)
def test_brake_light_follows_vcfront_brake_position_can(
    vcrear_cluster,
    brake_position,
    brake_light_on,
):
    vcrear = vcrear_cluster.vcrear
    BrakeLightState = vcrear.can.enums.BrakeLightState
    expected_state = BrakeLightState.ON if brake_light_on else BrakeLightState.OFF

    assert send_vcfront_brake_position(vcrear, brake_position)
    vcrear_cluster.run_until(
        lambda: brake_light_state(vcrear) == expected_state,
        timeout=250,
        message=f"vcrear brake light should become {expected_state.name}",
    )


def test_brake_light_faults_when_vcfront_pedal_position_goes_mia(vcrear_cluster):
    vcrear = vcrear_cluster.vcrear
    BrakeLightState = vcrear.can.enums.BrakeLightState

    assert send_vcfront_brake_position(vcrear, 50)
    vcrear_cluster.run_until(
        lambda: brake_light_state(vcrear) == BrakeLightState.ON,
        timeout=250,
        message="vcrear brake light should turn on while vcfront pedal position is present",
    )

    vcrear_cluster.run_until(
        lambda: brake_light_state(vcrear) == BrakeLightState.FAULT,
        timeout=1200,
        step=10,
        message="vcrear brake light should fault after vcfront pedal position goes MIA",
    )
    assert brake_light_state(vcrear) == BrakeLightState.FAULT
