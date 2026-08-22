import pytest

from sim.models.controllers.vcfront import (
    AnalogInput,
    AppsState,
    BppcState,
    DigitalStatus,
    Fault,
    TimerChannel,
    VehicleState,
)
from sim.models.controllers.vcfront.fixtures import vcfront_cluster
from sim.models.controllers.vcpdu import VcpduSimpleModel
from sim.models.controllers.sws import SwsSimpleModel


@pytest.mark.parametrize(
    "brake_pressure_voltage",
    [
        pytest.param(0.10, id="shorted-brake-pressure-sensor"),
        pytest.param(3.00, id="open-brake-pressure-sensor"),
    ],
)
def test_brake_pedal_faults(vcfront_cluster, brake_pressure_voltage):
    vcfront = vcfront_cluster.vcfront
    message = vcfront.can.tx_message("VCFRONT_pedalPosition", bus="veh")
    vcfront.set_analog_input(AnalogInput.BR_PR, brake_pressure_voltage)
    vcfront_cluster.run_for(20, step=10)

    faulted = vcfront.get_fault(Fault.BRAKE_SENSOR)
    assert faulted, (
        "expected brake sensor fault for "
        f"brake_pressure_voltage={brake_pressure_voltage:.3f}"
    )
    signals = vcfront.can.latest(message)
    assert (
        signals is not None
    ), f"expected CAN TX message {message.name} on {message.bus_name}"
    assert signals.VCFRONT_brakePosition == 0


@pytest.mark.parametrize(
    "channel,fault,voltage",
    [
        pytest.param(
            AnalogInput.APPS_P1, Fault.APPS1_SENSOR, 0.30, id="shorted-apps1-sensor"
        ),
        pytest.param(
            AnalogInput.APPS_P1, Fault.APPS1_SENSOR, 2.60, id="open-apps1-sensor"
        ),
        pytest.param(
            AnalogInput.APPS_P2, Fault.APPS2_SENSOR, 0.40, id="shorted-apps2-sensor"
        ),
        pytest.param(
            AnalogInput.APPS_P2, Fault.APPS2_SENSOR, 2.60, id="open-apps2-sensor"
        ),
    ],
)
def test_apps_pedal_faults(vcfront_cluster, channel, fault, voltage):
    vcfront = vcfront_cluster.vcfront
    message = vcfront.can.tx_message("VCFRONT_pedalPosition", bus="veh")
    vcfront.set_analog_input(channel, voltage)
    vcfront_cluster.run_for(20, step=10)

    faulted = vcfront.get_fault(fault)
    assert faulted, f"expected {fault.name} for {channel.name} voltage={voltage:.3f}"
    signals = vcfront.can.latest(message)
    assert (
        signals is not None
    ), f"expected CAN TX message {message.name} on {message.bus_name}"
    assert signals.VCFRONT_acceleratorPosition == 0
    if channel == AnalogInput.APPS_P1:
        assert signals.VCFRONT_apps1 == 0
    else:
        assert signals.VCFRONT_apps2 == 0


@pytest.mark.parametrize(
    "accelerator_position,brake_position",
    [
        pytest.param(0, 0, id="pedals-released"),
        pytest.param(50, 50, id="pedals-half"),
        pytest.param(100, 100, id="pedals-full"),
    ],
)
def test_pedal_position_sensors_report_on_can(
    vcfront_cluster, accelerator_position, brake_position
):
    vcfront = vcfront_cluster.vcfront
    vcfront.set_accelerator_position(accelerator_position)
    vcfront.set_brake_position(brake_position)
    vcfront_cluster.run_for(20, step=10)

    signals = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert signals is not None
    assert signals.VCFRONT_apps1 == accelerator_position
    assert signals.VCFRONT_apps2 == accelerator_position
    assert signals.VCFRONT_acceleratorPosition == accelerator_position
    assert signals.VCFRONT_brakePosition == brake_position


@pytest.mark.parametrize(
    "state",
    [
        pytest.param(VehicleState.INIT, id="init"),
        pytest.param(VehicleState.ON_GLV, id="on-glv"),
        pytest.param(VehicleState.ON_HV, id="on-hv"),
        pytest.param(VehicleState.SLEEP, id="sleep"),
    ],
)
def test_torque_request_stays_zero_when_accelerator_is_pressed_outside_ts_run(
    vcfront_cluster,
    state,
):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vehicle_state_periodic = vcpdu.periodic_vehicle_state(state, period=20)
    vcfront_cluster.add_component(vcpdu)

    vcfront.set_brake_position(0)
    vcfront.set_accelerator_position(0)
    vcfront_cluster.run_for(30, step=10)

    vcfront.set_accelerator_position(50)
    vehicle_state_periodic.set(VCPDU_vehicleState=state)
    vcfront_cluster.run_for(100, step=10)

    torque = vcfront.can.latest("VCFRONT_torqueManager", bus="veh")
    debug = vcfront.can.latest("VCFRONT_tractionControlDebug", bus="veh")
    assert torque is not None
    assert debug is not None
    assert debug.VCFRONT_torqueDriverInput > 0
    assert vcfront.latest_torque_request() == 0


def test_torque_request_follows_accelerator_in_ts_run(vcfront_cluster):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vcpdu.periodic_vehicle_state(VehicleState.TS_RUN, period=20)
    vcfront_cluster.add_component(vcpdu)

    vcfront.set_brake_position(0)
    vcfront.set_accelerator_position(0)
    vcfront_cluster.run_until(
        lambda: vcfront.latest_torque_request() == 0,
        timeout=500,
        step=10,
        message="VCFRONT torque request should start at zero in TS_RUN",
    )

    vcfront.set_accelerator_position(50)
    vcfront_cluster.run_until(
        lambda: _positive(vcfront.latest_torque_request()),
        timeout=500,
        step=10,
        message=(
            "VCFRONT torque request should become non-zero when accelerator is "
            "pressed in TS_RUN"
        ),
    )

    vcfront.set_accelerator_position(0)
    vcfront_cluster.run_until(
        lambda: vcfront.latest_torque_request() == 0,
        timeout=500,
        step=10,
        message="VCFRONT torque request should return to zero when accelerator is "
        "released",
    )


@pytest.mark.parametrize(
    "brake_position,expected_gear",
    [
        pytest.param(0, "FORWARD", id="brake-released"),
        pytest.param(50, "REVERSE", id="brake-pressed"),
    ],
)
def test_reverse_requires_brake_pedal(vcfront_cluster, brake_position, expected_gear):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vcpdu.periodic_vehicle_state(VehicleState.TS_RUN, period=20)
    sws = SwsSimpleModel(vcfront.can)
    sws.periodic_driver_request(
        period=20,
        SWS_requestReverse=DigitalStatus.ON,
    )
    vcfront_cluster.add_component(vcpdu)
    vcfront_cluster.add_component(sws)

    vcfront.set_accelerator_position(0)
    vcfront.set_brake_position(brake_position)
    vcfront_cluster.run_for(200, step=10)

    torque_manager = vcfront.can.latest("VCFRONT_torqueManager", bus="veh")
    assert torque_manager is not None
    assert torque_manager.VCFRONT_gear.name == expected_gear


def test_zero_vehicle_and_wheel_speed_preserves_torque_request_in_ts_run(
    vcfront_cluster,
):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vcpdu.periodic_vehicle_state(VehicleState.TS_RUN, period=20)
    sws = SwsSimpleModel(vcfront.can)
    driver_request = sws.periodic_driver_request(
        period=20,
        SWS_requestRaceMode=DigitalStatus.OFF,
        SWS_requestTractionControl=DigitalStatus.ON,
    )
    vcfront_cluster.add_component(vcpdu)
    vcfront_cluster.add_component(sws)

    vcfront.set_brake_position(0)
    vcfront.set_accelerator_position(0)
    vcfront_cluster.run_until(
        lambda: vcfront.latest_torque_request() == 0,
        timeout=500,
        step=10,
        message="VCFRONT torque request should start at zero in TS_RUN",
    )
    vcfront.set_brake_position(100)
    vcfront_cluster.run_for(100, step=10)
    driver_request.set(SWS_requestRaceMode=DigitalStatus.ON)
    vcfront_cluster.run_for(100, step=10)
    vcfront.set_brake_position(0)
    for channel in (TimerChannel._1, TimerChannel._2):
        vcfront._timer_peripherals.send(
            vcfront.timer.capture_events(channel),
            value=0.0,
        )
    vcfront.set_accelerator_position(50)
    vcfront_cluster.run_for(200, step=10)

    debug = vcfront.can.latest("VCFRONT_tractionControlDebug", bus="veh")
    assert debug is not None
    assert debug.VCFRONT_torqueDriverInput > 0
    assert vcfront.latest_torque_request() > 0


@pytest.mark.parametrize(
    "apps1_position,apps2_position",
    [
        pytest.param(80, 0, id="apps1-high-apps2-low"),
        pytest.param(0, 80, id="apps1-low-apps2-high"),
    ],
)
def test_apps_disagreement_reports_error_and_recovers(
    vcfront_cluster, apps1_position, apps2_position
):
    vcfront = vcfront_cluster.vcfront

    vcfront.set_accelerator_position(50)
    vcfront.set_brake_position(0)
    vcfront_cluster.run_for(60, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.OK
    assert status.VCFRONT_acceleratorPosition == 50

    vcfront.set_apps1_position(apps1_position)
    vcfront.set_apps2_position(apps2_position)
    vcfront_cluster.run_for(50, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.DISAGREEMENT
    assert status.VCFRONT_acceleratorPosition == 0

    vcfront.set_accelerator_position(50)
    vcfront_cluster.run_for(30, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.OK
    assert status.VCFRONT_acceleratorPosition == 50


def test_bppc_fault_latches_until_accelerator_is_released(vcfront_cluster):
    vcfront = vcfront_cluster.vcfront

    vcfront.set_accelerator_position(0)
    vcfront.set_brake_position(0)
    vcfront_cluster.run_for(20, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.OK
    assert status.VCFRONT_acceleratorPosition == 0
    assert status.VCFRONT_bppcState == BppcState.OK

    vcfront.set_accelerator_position(50)
    vcfront_cluster.run_for(40, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.OK
    assert status.VCFRONT_acceleratorPosition == 50
    assert status.VCFRONT_bppcState == BppcState.OK

    vcfront.set_brake_position(50)
    vcfront_cluster.run_for(30, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.OK
    assert status.VCFRONT_acceleratorPosition == 50
    assert status.VCFRONT_brakePosition == 50
    assert status.VCFRONT_bppcState == BppcState.FAULT

    vcfront.set_brake_position(0)
    vcfront_cluster.run_for(20, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_bppcState == BppcState.FAULT_LATCHED

    vcfront_cluster.run_for(10, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_bppcState == BppcState.ERROR
    assert status.VCFRONT_acceleratorPosition == 50

    vcfront.set_accelerator_position(0)
    vcfront_cluster.run_for(30, step=10)

    status = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert status is not None
    assert status.VCFRONT_acceleratorState == AppsState.OK
    assert status.VCFRONT_acceleratorPosition == 0
    assert status.VCFRONT_bppcState == BppcState.OK


def _positive(value):
    return value is not None and value > 0
