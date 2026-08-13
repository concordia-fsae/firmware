from sim.models.controllers.sws import SwsSimpleModel
from sim.models.controllers.vcfront import VcfrontSimpleModel
from sim.models.vehicle.fixtures import vehicle_cluster


def test_vcpdu_hsd_power_controls_vehicle_controller_online_state(vehicle_cluster):
    VehicleState = vehicle_cluster.vcpdu.can.enums.VehicleState
    SleepFollowerState = vehicle_cluster.vcpdu.can.enums.SleepFollowerState
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
