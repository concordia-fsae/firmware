import pytest

from sim.models.controllers.vcfront import (
    AnalogInput,
    AppsState,
    BppcState,
    DigitalStatus,
    Fault,
    TorqueManagerState,
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
        lambda: (
            vcfront.latest_torque_request is not None
            and vcfront.latest_torque_request() > 0
        ),
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


# 2027 EV.4.7.2 a) BPPC torque disable
def test_bppc_disables_torque(vcfront_cluster):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vcpdu.periodic_vehicle_state(VehicleState.TS_RUN, period=20)
    vcfront_cluster.add_component(vcpdu)

    vcfront.set_brake_position(0)
    vcfront.set_accelerator_position(0)

    vcfront_cluster.run_for(30, step=10)
    vcfront.set_accelerator_position(26)

    vcfront_cluster.run_until(
        lambda: vcfront.latest_torque_request() > 0,
        timeout=160,
        step=10,
        message="VCFRONT torque request should be positive",
    )
    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    torque_manager = vcfront.can.latest("VCFRONT_torqueManager", bus="veh")
    debug = vcfront.can.latest("VCFRONT_tractionControlDebug", bus="veh")
    assert pedal is not None
    assert torque_manager is not None
    assert debug is not None
    assert torque_manager.VCFRONT_torqueManagerState == TorqueManagerState.ACTIVE
    assert pedal.VCFRONT_acceleratorState == AppsState.OK
    assert pedal.VCFRONT_acceleratorPosition > 25
    assert pedal.VCFRONT_bppcState == BppcState.OK
    assert pedal.VCFRONT_brakePosition == 0
    assert debug.VCFRONT_torqueDriverInput > 0
    assert vcfront.latest_torque_request() > 0

    vcfront.set_brake_position(50)

    vcfront_cluster.run_until(
        lambda: _bppc_state_is_inhibiting_torque(vcfront, BppcState.FAULT),
        timeout=160,
        step=10,
        message="VCFRONT torque request should drop to zero when BPPC is triggered",
    )
    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    torque_manager = vcfront.can.latest("VCFRONT_torqueManager", bus="veh")
    debug = vcfront.can.latest("VCFRONT_tractionControlDebug", bus="veh")
    assert pedal is not None
    assert torque_manager is not None
    assert debug is not None
    assert torque_manager.VCFRONT_torqueManagerState == TorqueManagerState.ACTIVE
    assert pedal.VCFRONT_acceleratorState == AppsState.OK
    assert pedal.VCFRONT_acceleratorPosition > 25
    assert pedal.VCFRONT_brakePosition == 50
    assert pedal.VCFRONT_bppcState == BppcState.FAULT
    assert debug.VCFRONT_torqueDriverInput == 0
    assert valid_torque_output(vcfront, pedal)


# 2027 EV.4.7.2 a) BPPC entry condition
@pytest.mark.parametrize(
    "brake_position,accelerator_position,expected_bppc_state",
    [
        pytest.param(0, 0, BppcState.OK),
        pytest.param(9, 0, BppcState.OK),
        pytest.param(11, 0, BppcState.OK),
        pytest.param(100, 0, BppcState.OK),
        pytest.param(0, 24, BppcState.OK),
        pytest.param(9, 26, BppcState.OK),
        pytest.param(10, 26, BppcState.OK),
        pytest.param(11, 24, BppcState.OK),
        pytest.param(11, 25, BppcState.OK),
        pytest.param(11, 26, BppcState.FAULT),
        pytest.param(50, 50, BppcState.FAULT),
        pytest.param(100, 100, BppcState.FAULT),
    ],
)
def test_bppc_entry_depends_on_brake_and_accelerator_positions(
    vcfront_cluster, brake_position, accelerator_position, expected_bppc_state
):
    vcfront = vcfront_cluster.vcfront

    vcfront.set_brake_position(0)
    vcfront.set_accelerator_position(0)
    vcfront_cluster.run_for(30, step=10)

    vcfront.set_brake_position(brake_position)
    vcfront.set_accelerator_position(accelerator_position)

    vcfront_cluster.run_for(100, step=10)

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_acceleratorState == AppsState.OK
    assert pedal.VCFRONT_acceleratorPosition == accelerator_position
    assert pedal.VCFRONT_bppcState == expected_bppc_state
    assert abs(pedal.VCFRONT_brakePosition - brake_position) <= 1


# 2027 EV.4.7.2 b) BPPC exit condition
@pytest.mark.parametrize(
    "brake_position,accelerator_position,expected_bppc_state",
    [
        pytest.param(0, 6, BppcState.ERROR),
        pytest.param(0, 5, BppcState.ERROR),
        pytest.param(0, 4, BppcState.OK),
        pytest.param(50, 6, BppcState.ERROR),
        pytest.param(50, 5, BppcState.ERROR),
        pytest.param(50, 4, BppcState.OK),
    ],
)
def test_bppc_fault_remains_until_accelerator_is_below_5_percent(
    vcfront_cluster, brake_position, accelerator_position, expected_bppc_state
):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vcpdu.periodic_vehicle_state(VehicleState.TS_RUN, period=20)
    vcfront_cluster.add_component(vcpdu)

    vcfront.set_brake_position(50)
    vcfront.set_accelerator_position(50)
    vcfront_cluster.run_until(
        lambda: _bppc_state_is_inhibiting_torque(vcfront, BppcState.FAULT),
        timeout=160,
        step=10,
        message="BPPC should fault and inhibit torque in TS_RUN",
    )

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_acceleratorState == AppsState.OK
    assert pedal.VCFRONT_acceleratorPosition == 50
    assert pedal.VCFRONT_brakePosition == 50
    assert pedal.VCFRONT_bppcState == BppcState.FAULT

    vcfront.set_brake_position(brake_position)
    vcfront.set_accelerator_position(accelerator_position)

    if expected_bppc_state == BppcState.OK:
        vcfront_cluster.run_until(
            lambda: _latest_bppc_state(vcfront) == BppcState.OK,
            timeout=160,
            step=10,
            message="BPPC should recover below 5% APPS",
        )
    else:
        vcfront_cluster.run_for(100, step=10)

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    torque_manager = vcfront.can.latest("VCFRONT_torqueManager", bus="veh")
    assert pedal is not None
    assert torque_manager is not None
    assert torque_manager.VCFRONT_torqueManagerState == TorqueManagerState.ACTIVE
    assert pedal.VCFRONT_acceleratorState == AppsState.OK
    assert abs(pedal.VCFRONT_brakePosition - brake_position) <= 1
    assert pedal.VCFRONT_bppcState == expected_bppc_state
    assert valid_torque_output(vcfront, pedal)


# 2027 EV.4.7.2 b) BPPC exit condition and torque recovery
def test_bppc_exit(vcfront_cluster):
    vcfront = vcfront_cluster.vcfront
    vcpdu = VcpduSimpleModel(vcfront.can)
    vcpdu.periodic_vehicle_state(VehicleState.TS_RUN, period=20)
    vcfront_cluster.add_component(vcpdu)

    # Trigger BPPC fault
    vcfront.set_brake_position(50)
    vcfront.set_accelerator_position(50)

    vcfront_cluster.run_until(
        lambda: _bppc_state_is_inhibiting_torque(vcfront, BppcState.FAULT),
        timeout=160,
        step=10,
        message="BPPC should fault and inhibit torque in TS_RUN",
    )

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_acceleratorPosition == 50
    assert pedal.VCFRONT_brakePosition == 50
    assert pedal.VCFRONT_bppcState == BppcState.FAULT
    assert valid_torque_output(vcfront, pedal)

    # Release brake while keeping accelerator pressed
    vcfront.set_brake_position(0)
    vcfront_cluster.run_for(20, step=10)

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_bppcState == BppcState.FAULT_LATCHED
    assert valid_torque_output(vcfront, pedal)
    assert pedal.VCFRONT_acceleratorPosition == 50
    assert pedal.VCFRONT_brakePosition == 0

    # Accelerator remains above 5%: BPPC must inhibit torque
    vcfront.set_accelerator_position(6)

    vcfront_cluster.run_until(
        lambda: _bppc_state_is_inhibiting_torque(vcfront, BppcState.ERROR),
        timeout=160,
        step=10,
        message="BPPC ERROR should continue inhibiting torque",
    )

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_bppcState == BppcState.ERROR
    assert valid_torque_output(vcfront, pedal)

    # Accelerator drops below 5%: BPPC clears
    vcfront.set_accelerator_position(4)

    vcfront_cluster.run_until(
        lambda: _latest_bppc_state(vcfront) == BppcState.OK,
        timeout=160,
        step=10,
        message="BPPC should recover below 5% APPS",
    )

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_bppcState == BppcState.OK

    # Verify driver can request non-zero torque again
    vcfront.set_accelerator_position(50)

    vcfront_cluster.run_until(
        lambda: vcfront.latest_torque_request() > 0,
        timeout=500,
        step=10,
        message="Torque request should recover after BPPC clears",
    )

    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal is not None
    assert pedal.VCFRONT_bppcState == BppcState.OK
    assert vcfront.latest_torque_request() > 0


def _latest_bppc_state(vcfront):
    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    if pedal is None:
        return None

    return pedal.VCFRONT_bppcState


def _bppc_state_is_inhibiting_torque(vcfront, expected_state):
    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    torque_manager = vcfront.can.latest("VCFRONT_torqueManager", bus="veh")

    if pedal is None or torque_manager is None:
        return False

    return (
        pedal.VCFRONT_bppcState == expected_state
        and torque_manager.VCFRONT_torqueManagerState == TorqueManagerState.ACTIVE
        and vcfront.latest_torque_request() == 0
    )


def valid_torque_output(vcfront, pedal):
    if pedal is None:
        return False

    if pedal.VCFRONT_bppcState != BppcState.OK:
        return vcfront.latest_torque_request() == 0

    return True


def test_gps_diagnostics_report_unavailable_without_receiver_data(vcfront_cluster):
    vcfront_cluster.run_for(200, step=10)
    signals = vcfront_cluster.vcfront.can.latest("VCFRONT_gpsDiagnostics", bus="veh")
    assert signals is not None
    assert signals.VCFRONT_gpsQualityIndicator == 0
    assert signals.VCFRONT_gpsFixDimension == 0
    assert signals.VCFRONT_gpsInvalidTime == 1
    assert signals.VCFRONT_gpsInvalidDate == 1
    assert not vcfront_cluster.vcfront.get_fault(Fault.GNSS_TRANSCEIVER_BROKEN)


def _nmea_burst(*payloads):
    frames = []
    for payload in payloads:
        checksum = 0
        for byte in payload.encode("ascii"):
            checksum ^= byte
        frames.append(f"${payload}*{checksum:02X}\r\n")
    return "".join(frames).encode("ascii")


def test_gps_uart_stream_acquires_fix_and_updates_can_at_10hz(vcfront_cluster):
    from sim.models.controllers.vcfront import UartPort

    vcfront = vcfront_cluster.vcfront

    def latest(message):
        signals = vcfront.can.latest(f"VCFRONT_{message}", bus="veh")
        assert signals is not None, message
        return signals

    # Receiver startup: well-formed NMEA with empty navigation fields and no fix.
    empty = _nmea_burst(
        "GNGGA,125958.000,,,,,0,00,,,,,,,",
        "GNRMC,125958.000,V,,,,,,,011126,,,N",
        "GNGSA,A,1,,,,,,,,,,,,,1.2,0.7,1.0,1",
    )
    received_bytes = 0
    for sample in range(10):
        assert vcfront.uart.receive(UartPort.GPS, empty) == len(empty)
        received_bytes += len(empty)
        vcfront_cluster.run_for(100, step=10)
        diagnostics = latest("gpsDiagnostics")
        assert diagnostics.event.timestamp_ns == (sample + 1) * 100_000_000
        assert diagnostics.VCFRONT_gpsQualityIndicator == 0
        assert diagnostics.VCFRONT_gpsFixDimension == 1
        assert diagnostics.VCFRONT_gpsInvalidTime == 1
        assert diagnostics.VCFRONT_gpsInvalidDate == 1
        position = latest("gpsPosition")
        assert position.VCFRONT_lat == pytest.approx(0, abs=0.00001)
        assert position.VCFRONT_lon == pytest.approx(0, abs=0.00001)

    assert latest("faults").VCFRONT_gpsDataNotValid == 1
    assert latest("gpsSentenceCounts").VCFRONT_gpsSentenceGgaCount == 10

    # Exercise SPS, differential, PPS, and receiver DR=6 -> CAN DR=4. DR is
    # valid with GSA no-fix; the other methods require a 2D/3D satellite fix.
    for sample in range(50):
        quality, can_quality, dimension, mode = (
            (1, 1, 3, "A"),
            (2, 2, 2, "D"),
            (3, 3, 3, "D"),
            (6, 4, 1, "E"),
        )[sample % 4]
        seconds = 12 * 3600 + 59 * 60 + 58 + sample // 10
        hour, remainder = divmod(seconds, 3600)
        minute, second = divmod(remainder, 60)
        utc = f"{hour:02}{minute:02}{second:02}.{sample % 10}00"
        lat_minutes = 47 + sample / 100
        lon_minutes = sample / 100
        altitude = 95 + sample
        knots = 10 + sample / 10
        course = 120 + sample
        satellites = 8 + sample % 5
        burst = _nmea_burst(
            f"GNGGA,{utc},24{lat_minutes:07.4f},S,121{lon_minutes:07.4f},W,"
            f"{quality},{satellites:02},0.7,{altitude}.0,M,19.6,M,,0000",
            f"GNRMC,{utc},A,24{lat_minutes:07.4f},S,121{lon_minutes:07.4f},W,"
            f"{knots:.1f},{course}.0,011126,,,{mode}",
            f"GNGSA,A,{dimension},21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1",
        )
        # Poll once with an incomplete sentence, then complete this 100 ms burst.
        assert vcfront.uart.receive(UartPort.GPS, burst[:23]) == 23
        vcfront_cluster.run_for(10, step=10)
        assert vcfront.uart.receive(UartPort.GPS, burst[23:]) == len(burst) - 23
        received_bytes += len(burst)
        vcfront_cluster.run_for(90, step=10)

        position = latest("gpsPosition")
        expected_timestamp = (11 + sample) * 100_000_000
        assert position.event.timestamp_ns == expected_timestamp
        assert position.VCFRONT_lat == pytest.approx(
            -(24 + lat_minutes / 60), abs=0.00002
        )
        assert position.VCFRONT_lon == pytest.approx(
            -(121 + lon_minutes / 60), abs=0.00002
        )
        assert position.VCFRONT_alt == altitude
        heading = latest("gpsHeading")
        assert heading.event.timestamp_ns == expected_timestamp
        assert heading.VCFRONT_course == pytest.approx(course, abs=0.1)
        # lwgps uses 0.5144 m/s per knot; CAN truncates to 0.01 m/s.
        assert heading.VCFRONT_speed == pytest.approx(knots * 0.5144, abs=0.01)
        diagnostics = latest("gpsDiagnostics")
        assert diagnostics.event.timestamp_ns == expected_timestamp
        assert diagnostics.VCFRONT_gpsQualityIndicator == can_quality
        assert diagnostics.VCFRONT_gpsFixDimension == dimension
        assert diagnostics.VCFRONT_gpsSatellites == satellites
        assert diagnostics.VCFRONT_gpsInvalidTime == 0
        assert diagnostics.VCFRONT_gpsInvalidDate == 0

        # Date, sentence counters, UART diagnostics and faults publish at 1 Hz.
        if sample % 10 == 9:
            date = latest("gpsDate")
            assert date.event.timestamp_ns == expected_timestamp
            assert (date.VCFRONT_year, date.VCFRONT_month, date.VCFRONT_day) == (
                2026,
                11,
                1,
            )
            assert (date.VCFRONT_hour, date.VCFRONT_minute, date.VCFRONT_second) == (
                hour,
                minute,
                second,
            )
            assert date.VCFRONT_crcFailures == 0
            assert date.VCFRONT_invalidTransactions == 0
            assert date.VCFRONT_numberSamples == 3 * (10 + sample + 1)
            counts = latest("gpsSentenceCounts")
            assert counts.event.timestamp_ns == expected_timestamp
            assert counts.VCFRONT_gpsSentenceGgaCount == 10 + sample + 1
            assert counts.VCFRONT_gpsSentenceRmcCount == 10 + sample + 1
            assert counts.VCFRONT_gpsSentenceGsaCount == 10 + sample + 1
            errors = latest("gpsUartDiag")
            assert errors.VCFRONT_gpsUartOreCount == 0
            assert errors.VCFRONT_gpsUartFeCount == 0
            assert errors.VCFRONT_gpsUartNeCount == 0
            assert errors.VCFRONT_gpsUartPeCount == 0
            faults = latest("faults")
            assert faults.VCFRONT_gpsDeviceError == 0
            assert faults.VCFRONT_gpsOverrun == 0
            # Fault CAN latches any occurrence since its previous publication.
            # The first valid second includes the no-fix -> fix transition.
            assert faults.VCFRONT_gpsDataNotValid == (1 if sample == 9 else 0)

    # This stream also crosses the application's 10 kB circular DMA boundary.
    assert received_bytes > 10000


@pytest.mark.parametrize("partial_wait_ms", [10, 1000])
def test_gps_uart_partial_sentence_survives_polls(vcfront_cluster, partial_wait_ms):
    from sim.models.controllers.vcfront import UartPort

    vcfront = vcfront_cluster.vcfront
    sentence = _nmea_burst(
        "GNGGA,125958.000,2447.0000,N,12100.0000,E,1,08,0.7,95.0,M,19.6,M,,0000"
    )

    vcfront_cluster.run_for(1000 - partial_wait_ms, step=10)
    assert vcfront.uart.receive(UartPort.GPS, sentence[:23]) == 23
    vcfront_cluster.run_for(partial_wait_ms, step=10)
    date = vcfront.can.latest("VCFRONT_gpsDate", bus="veh")
    assert date is not None
    assert date.event.timestamp_ns == 1_000_000_000
    assert date.VCFRONT_numberSamples == 0
    assert date.VCFRONT_invalidTransactions == 0
    position = vcfront.can.latest("VCFRONT_gpsPosition", bus="veh")
    assert position.VCFRONT_lat == pytest.approx(0, abs=0.00001)
    assert position.VCFRONT_alt == 0

    assert vcfront.uart.receive(UartPort.GPS, sentence[23:]) == len(sentence) - 23
    vcfront_cluster.run_for(10, step=10)
    vcfront_cluster.run_for(90, step=10)
    position = vcfront.can.latest("VCFRONT_gpsPosition", bus="veh")
    assert position.event.timestamp_ns == 1_100_000_000
    assert position.VCFRONT_alt == 95
    vcfront_cluster.run_for(900, step=10)
    date = vcfront.can.latest("VCFRONT_gpsDate", bus="veh")
    assert date.event.timestamp_ns == 2_000_000_000
    assert date.VCFRONT_numberSamples == 1
    assert date.VCFRONT_invalidTransactions == 0
    counts = vcfront.can.latest("VCFRONT_gpsSentenceCounts", bus="veh")
    assert counts.VCFRONT_gpsSentenceGgaCount == 1
    assert counts.VCFRONT_gpsSentenceRmcCount == 0
    position = vcfront.can.latest("VCFRONT_gpsPosition", bus="veh")
    assert position.VCFRONT_lat == pytest.approx(24 + 47 / 60, abs=0.00002)
    assert position.VCFRONT_lon == pytest.approx(121, abs=0.00002)
    assert position.VCFRONT_alt == 95

    # Further polls cannot consume the completed sentence a second time.
    vcfront_cluster.run_for(1000, step=10)
    date = vcfront.can.latest("VCFRONT_gpsDate", bus="veh")
    assert date.event.timestamp_ns == 3_000_000_000
    assert date.VCFRONT_numberSamples == 1
    assert date.VCFRONT_invalidTransactions == 0


@pytest.mark.parametrize("irq_rate", [20, 50, 100, 500])
@pytest.mark.parametrize("error_code", [1, 2, 4, 8, 15])
def test_gps_uart_error_irq_burst_disables_port(vcfront_cluster, irq_rate, error_code):
    from sim.models.controllers.vcfront import DigitalIo, UartPort

    vcfront = vcfront_cluster.vcfront
    navigation = _nmea_burst(
        "GNGGA,125958.000,2447.0000,N,12100.0000,E,1,08,0.7,95.0,M,19.6,M,,0000",
        "GNRMC,125958.000,A,2447.0000,N,12100.0000,E,10.0,120.0,011126,,,A",
        "GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1",
    )
    assert vcfront.uart.receive(UartPort.GPS, navigation) == len(navigation)
    vcfront_cluster.run_for(100, step=10)
    diagnostics = vcfront.can.latest("VCFRONT_gpsDiagnostics", bus="veh")
    assert diagnostics.VCFRONT_gpsInvalidTime == 0

    # Start near a wall-second boundary: the limit uses a rolling window.
    vcfront_cluster.run_for(800, step=10)
    interval_ms = 1000 // irq_rate
    handled = 0
    for _ in range(irq_rate * 2):
        handled += vcfront.inject_gps_uart_error(error_code)
        vcfront_cluster.run_for(interval_ms, step=min(interval_ms, 10))
    assert handled == 19  # The twentieth error callback disables instead of recovering.
    assert not vcfront.uart.status(UartPort.GPS).active
    assert vcfront.get_fault(Fault.GNSS_TRANSCEIVER_BROKEN)
    assert not vcfront.get_digital_io(DigitalIo.MCU_UART_EN)
    assert vcfront.uart.receive(UartPort.GPS, navigation) == 0

    # A quiet period must not automatically rearm a noisy port.
    vcfront_cluster.run_for(2100, step=10)
    assert not vcfront.inject_gps_uart_error(error_code)
    assert not vcfront.uart.status(UartPort.GPS).active
    errors = vcfront.can.latest("VCFRONT_gpsUartDiag", bus="veh")
    for bit, signal in [(1, "Pe"), (2, "Ne"), (4, "Fe"), (8, "Ore")]:
        expected_count = 20 if error_code & bit else 0
        assert getattr(errors, f"VCFRONT_gpsUart{signal}Count") == expected_count
    diagnostics = vcfront.can.latest("VCFRONT_gpsDiagnostics", bus="veh")
    assert diagnostics.VCFRONT_gpsInvalidTime == 1
    assert diagnostics.VCFRONT_gpsQualityIndicator == 0
    faults = vcfront.can.latest("VCFRONT_faults", bus="veh")
    assert faults.VCFRONT_gpsDeviceError == 1
    assert faults.VCFRONT_gpsDataNotValid == 1
    assert faults.VCFRONT_gnssTransceiverBroken == 1
    assert vcfront.get_fault(Fault.GNSS_TRANSCEIVER_BROKEN)

    # Pedal processing and CAN scheduling continue after the UART is masked.
    vcfront.set_accelerator_position(50)
    vcfront_cluster.run_for(100, step=10)
    pedal = vcfront.can.latest("VCFRONT_pedalPosition", bus="veh")
    assert pedal.VCFRONT_acceleratorPosition == 50
    assert pedal.event.timestamp_ns >= 5_000_000_000


def test_gps_uart_sparse_errors_recover_without_accumulating_to_shutdown(
    vcfront_cluster,
):
    from sim.models.controllers.vcfront import UartPort

    vcfront = vcfront_cluster.vcfront
    sentence = _nmea_burst(
        "GNGGA,125958.000,2447.0000,N,12100.0000,E,1,08,0.7,95.0,M,19.6,M,,0000"
    )
    # Ten IRQs/s for three seconds exceeds the lifetime threshold, but never
    # reaches twenty in a rolling second. Each error must recover reception.
    for _ in range(30):
        assert vcfront.inject_gps_uart_error(15)
        assert vcfront.uart.status(UartPort.GPS).active
        assert vcfront.uart.receive(UartPort.GPS, sentence) == len(sentence)
        vcfront_cluster.run_for(100, step=10)
    counts = vcfront.can.latest("VCFRONT_gpsSentenceCounts", bus="veh")
    assert counts.VCFRONT_gpsSentenceGgaCount == 30
    assert not vcfront.get_fault(Fault.GNSS_TRANSCEIVER_BROKEN)
    faults = vcfront.can.latest("VCFRONT_faults", bus="veh")
    assert faults.VCFRONT_gnssTransceiverBroken == 0
    errors = vcfront.can.latest("VCFRONT_gpsUartDiag", bus="veh")
    assert errors.VCFRONT_gpsUartOreCount == 30
    assert errors.VCFRONT_gpsUartFeCount == 30
    assert errors.VCFRONT_gpsUartNeCount == 30
    assert errors.VCFRONT_gpsUartPeCount == 30


def test_gps_uart_zero_error_callbacks_do_not_disable_port(vcfront_cluster):
    from sim.models.controllers.vcfront import UartPort

    vcfront = vcfront_cluster.vcfront
    sentence = _nmea_burst(
        "GNGGA,125958.000,2447.0000,N,12100.0000,E,1,08,0.7,95.0,M,19.6,M,,0000"
    )
    assert vcfront.uart.receive(UartPort.GPS, sentence) == len(sentence)
    for _ in range(500):
        assert vcfront.inject_gps_uart_error(0)
        vcfront_cluster.run_for(2, step=2)
    assert vcfront.uart.status(UartPort.GPS).active
    counts = vcfront.can.latest("VCFRONT_gpsSentenceCounts", bus="veh")
    assert counts.VCFRONT_gpsSentenceGgaCount == 1
    errors = vcfront.can.latest("VCFRONT_gpsUartDiag", bus="veh")
    assert errors.VCFRONT_gpsUartOreCount == 0
    assert errors.VCFRONT_gpsUartFeCount == 0
    assert errors.VCFRONT_gpsUartNeCount == 0
    assert errors.VCFRONT_gpsUartPeCount == 0
