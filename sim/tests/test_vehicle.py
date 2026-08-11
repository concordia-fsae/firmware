from sim.models.vehicle.fixtures import vehicle_cluster


def test_vcpdu_hsd_power_controls_vehicle_controller_online_state(vehicle_cluster):
    VehicleState = vehicle_cluster.vcpdu.can.enums.VehicleState
    SleepFollowerState = vehicle_cluster.vcpdu.can.enums.SleepFollowerState
    vcpdu = vehicle_cluster.vcpdu

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

    assert vcpdu.send_all_waking_controllers_sleepable(SleepFollowerState.OK_TO_SLEEP)
    vcpdu.allow_sleep()
    vehicle_cluster.run_until(
        lambda: vcpdu.latest_vehicle_state() == VehicleState.SLEEP,
        timeout=500,
        step=10,
        message="vcpdu should enter sleep",
    )
    vehicle_cluster.run_for(50, step=10)
    assert vehicle_cluster.vcfront.is_online()
    assert vehicle_cluster.vcrear.is_online()

    assert vcpdu.send_waking_controller_sleepable(
        "vcfront", SleepFollowerState.NOK_TO_SLEEP
    )
    vehicle_cluster.run_until(
        lambda: vcpdu.latest_vehicle_state() == VehicleState.ON_GLV,
        timeout=1000,
        step=10,
        message="vcpdu should wake back to ON_GLV",
    )
    assert vehicle_cluster.vcfront.is_online()
    assert vehicle_cluster.vcrear.is_online()
