import pytest

from sim.models.components.drivetrain import DrivetrainModel
from sim.models.controllers.bmsb import (
    BmsbDrivetrainSimpleModel,
    DigitalIo,
    DigitalStatus,
    PrechargeContactorState,
)
from sim.models.controllers.bmsw import BmswSimpleCluster
from sim.models.controllers.vcpdu import SleepFollowerState, VehicleState
from sim.models.controllers.sws import SwsSimpleModel
from sim.models.controllers.vcfront import VcfrontSimpleModel
from sim.models.test import InputTriggeredScalarSink
from sim.models.vehicle.fixtures import vehicle_cluster


def _configure_vehicle_bmsb(bmsb) -> None:
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        bmsb.set_digital_io(input_, True)
    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    bmsb.set_digital_io(DigitalIo.VPACK_DIAG, False)


def _add_vehicle_test_inputs(vehicle_cluster):
    bmsb = vehicle_cluster.bmsb
    vcpdu = vehicle_cluster.vcpdu
    sws = SwsSimpleModel(vcpdu.can)
    run_request = sws.periodic_driver_request(
        period=10,
        SWS_requestRun=DigitalStatus.OFF,
    )
    torque_sink = InputTriggeredScalarSink(
        DrivetrainModel.mechanical_torque_output_channel("vehicle")
    )
    vehicle_cluster.add_components(
        BmswSimpleCluster.for_platform(
            bmsb.can,
            vehicle_cluster.hardware,
        ).model,
        sws,
        torque_sink,
        BmsbDrivetrainSimpleModel(
            bmsb.can,
            terminal_voltage=350.0,
        ),
    )
    return run_request, torque_sink


def _assert_no_torque(vehicle_cluster, torque_sink) -> None:
    vehicle_cluster.run_for(50)
    assert not torque_sink.values or torque_sink.values[-1] == pytest.approx(0.0)


def test_vcpdu_hsd_power_controls_vehicle_controller_online_state(vehicle_cluster):
    vcpdu = vehicle_cluster.vcpdu
    sws = SwsSimpleModel(vcpdu.can)
    vcfront = VcfrontSimpleModel(vcpdu.can)
    sws.periodic_sleepable(SleepFollowerState.OK_TO_SLEEP, period=100)
    vcfront.periodic_sleepable(
        SleepFollowerState.OK_TO_SLEEP,
        period=100,
    )
    vehicle_cluster.add_components(sws, vcfront)

    vehicle_cluster.run_until(
        lambda: not vehicle_cluster.vcfront.is_online()
        and not vehicle_cluster.vcrear.is_online(),
        timeout=350,
        step=10,
        message="vcpdu worker power cycle should depower vehicle controllers",
    )
    vehicle_cluster.run_until(
        lambda: vcpdu.latest_vehicle_state() == VehicleState.ON_GLV
        and vehicle_cluster.vcfront.is_online()
        and vehicle_cluster.vcrear.is_online(),
        timeout=750,
        step=10,
        message="vcpdu should repower vcrear after worker power cycle",
    )
    assert vehicle_cluster.vcfront.is_online()
    assert vehicle_cluster.vcrear.is_online()


def test_vehicle_drivetrain_only_outputs_torque_in_ts_run(vehicle_cluster):
    bmsb = vehicle_cluster.bmsb
    vcpdu = vehicle_cluster.vcpdu
    vcfront = vehicle_cluster.vcfront

    _configure_vehicle_bmsb(bmsb)
    run_request, torque_sink = _add_vehicle_test_inputs(vehicle_cluster)
    vehicle_cluster.run_for(750)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_GLV
    _assert_no_torque(vehicle_cluster, torque_sink)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, True)
    vehicle_cluster.run_for(3000)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV
    information = bmsb.can.latest("BMSB_information", bus="veh")
    assert information is not None
    assert information.BMSB_packContactorState == PrechargeContactorState.HVP_CLOSED
    _assert_no_torque(vehicle_cluster, torque_sink)

    # The state transition must be caused by the normal driver sequence:
    # brake applied, run request asserted, then accelerator applied.
    vcfront.set_brake_position(12)
    run_request.set(SWS_requestRun=DigitalStatus.ON)
    vehicle_cluster.run_for(750)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN

    vcfront.set_brake_position(0)
    vcfront.set_accelerator_position(0.0)
    vehicle_cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN

    vcfront.set_accelerator_position(50.0)
    vehicle_cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN
    torque_request = vcfront.latest_torque_request()
    assert torque_request is not None
    assert torque_request > 0
    motor_command = vehicle_cluster.vcrear.can.latest("VCREAR_mcCommand", bus="veh")
    assert motor_command is not None
    assert motor_command.VCREAR_torqueCommand > 0
    vehicle_cluster.run_for(50)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN
    assert torque_sink.values[-1] > 0

    critical = bmsb.can.latest("BMSB_criticalData", bus="veh")
    assert critical is not None
    assert critical.BMSB_packCurrent > 0
