import math

import pytest

from sim.bindings.firmware.runtime import FirmwareClusterRig
from rig import DataPath
from sim.models.components.battery_source import BatterySourceModel, BatterySourceSpec
from sim.models.components.drivetrain import DrivetrainModel, DrivetrainSpec
from rig.model_fixtures import InputTriggeredScalarSink, ScalarSourceModel


def _run_drivetrain(*, voltage: float, torque: float, spec: DrivetrainSpec):
    voltage_path = DataPath.component(object(), "terminal_voltage")
    torque_path = DataPath.component(object(), "torque_request")
    mechanical_torque_path = DataPath.component(object(), "mechanical_torque")
    current_path = DataPath.component(object(), "current_draw")
    voltage_feedback_path = DataPath.component(object(), "bus_voltage")

    torque_source = ScalarSourceModel(torque_path)
    torque_source.values.append(torque)
    battery = BatterySourceModel(
        terminal_voltage_output_channel=voltage_path,
        source_spec=BatterySourceSpec(
            voltage=voltage,
            internal_resistance_ohms=0.1,
        ),
        current_drain_channels=(current_path,),
    )
    drivetrain = DrivetrainModel(
        terminal_voltage_input_channel=voltage_path,
        bus_voltage_output_channel=voltage_feedback_path,
        torque_request_input_channel=torque_path,
        mechanical_torque_output_channel=mechanical_torque_path,
        current_draw_output_channel=current_path,
        drivetrain_spec=spec,
    )
    torque_sink = InputTriggeredScalarSink(mechanical_torque_path)
    current_sink = InputTriggeredScalarSink(current_path)
    voltage_feedback_sink = InputTriggeredScalarSink(voltage_feedback_path)
    cluster = FirmwareClusterRig(
        battery=battery,
        request=torque_source,
        drivetrain=drivetrain,
        torque=torque_sink,
        current=current_sink,
        voltage_feedback=voltage_feedback_sink,
    )
    cluster.run_for(20)
    return (
        battery,
        torque_sink.values,
        current_sink.values,
        voltage_feedback_sink.values,
    )


def test_drivetrain_keeps_feedback_periodic_without_periodic_compute():
    _, torque, current, voltage_feedback = _run_drivetrain(
        voltage=350.0,
        torque=100.0,
        spec=DrivetrainSpec(
            max_torque_nm=200.0,
            torque_constant_nm_per_amp=1.0,
            scheduler_period_ms=5.0,
        ),
    )

    # Feedback is a periodic CAN-facing contract.  The native state update is
    # separately input-triggered, so unchanged inputs must not suppress the
    # periodic torque/current/voltage feedback publications.
    assert len(torque) == 4
    assert len(current) == 4
    assert len(voltage_feedback) == 4
    # Initial ingress events reach the event transform before the first
    # periodic feedback tick.  Publication cadence remains periodic while the
    # computed value is event-triggered.
    assert torque == pytest.approx([100.0, 100.0, 100.0, 100.0])
    assert current == pytest.approx([100.0, 100.0, 100.0, 100.0])
    assert voltage_feedback == pytest.approx([350.0, 340.0, 340.0, 340.0], abs=0.1)


def test_drivetrain_converts_terminal_voltage_and_torque_to_current_and_mechanical_torque():
    battery, torque, current, voltage_feedback = _run_drivetrain(
        voltage=350.0,
        torque=100.0,
        spec=DrivetrainSpec(
            max_torque_nm=200.0,
            torque_constant_nm_per_amp=1.0,
        ),
    )

    assert torque[-1] == pytest.approx(100.0)
    assert current[-1] == pytest.approx(100.0)
    assert battery.voltage == pytest.approx(340.0, abs=0.1)
    assert voltage_feedback[-1] == pytest.approx(340.0, abs=0.1)


def test_drivetrain_preserves_torque_direction_and_limits_power():
    battery, torque, current, _ = _run_drivetrain(
        voltage=100.0,
        torque=-100.0,
        spec=DrivetrainSpec(
            max_torque_nm=200.0,
            torque_constant_nm_per_amp=1.0,
            max_power_w=5_000.0,
        ),
    )

    assert torque[-1] == pytest.approx(-52.63158, abs=0.01)
    assert current[-1] == pytest.approx(52.63158, abs=0.01)
    assert battery.voltage == pytest.approx(94.73684, abs=0.02)


@pytest.mark.parametrize(
    "kwargs",
    [
        {"max_torque_nm": 0.0, "torque_constant_nm_per_amp": 1.0},
        {"max_torque_nm": math.inf, "torque_constant_nm_per_amp": 1.0},
        {"max_torque_nm": 1.0, "torque_constant_nm_per_amp": 0.0},
        {"max_torque_nm": 1.0, "torque_constant_nm_per_amp": 1.0, "efficiency": 0.0},
        {"max_torque_nm": 1.0, "torque_constant_nm_per_amp": 1.0, "max_power_w": 0.0},
    ],
)
def test_drivetrain_spec_rejects_invalid_values(kwargs):
    with pytest.raises(ValueError):
        DrivetrainSpec(**kwargs)
