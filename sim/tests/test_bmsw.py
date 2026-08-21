from __future__ import annotations

import pytest

from sim.models.controllers.bmsw import BMSW_CLUSTERS
from sim.models.pytest import cluster_rig_fixture


bmsw_cluster = cluster_rig_fixture(BMSW_CLUSTERS)


def _faults(bmsw, node_id: int):
    faults = bmsw.can.latest(f"BMSW{node_id}_faults", bus="veh")
    assert faults is not None
    return faults


def test_bmsw_nominal_segment_has_no_battery_faults(bmsw_cluster):
    bmsw_cluster.run_for(5000)

    assert set(bmsw_cluster.nodes) == {"bmsw0"}
    for node_id, bmsw in enumerate(bmsw_cluster.nodes.values()):
        faults = _faults(bmsw, node_id)
        for signal in (
            "analogRef5vHwError",
            "packVoltageHwError",
            "insufficientThermistors",
            "thermistorDisconnected",
            "cellDisconnected",
            "cellUndervoltage",
            "cellOvervoltage",
            "cellOvertemp",
        ):
            assert getattr(faults, f"BMSW{node_id}_{signal}") == 0


@pytest.mark.parametrize(
    ("input_name", "value", "signal", "duration_ms"),
    (
        ("cell", 1.8, "cellUndervoltage", 5000),
        ("cell", 4.4, "cellOvervoltage", 5000),
        ("cell", 0.0, "cellDisconnected", 30000),
        ("temperature", 65.0, "cellOvertemp", 5000),
        ("temperature", 0.0, "thermistorDisconnected", 5000),
    ),
)
def test_bmsw_identifies_sensor_faults(
    bmsw_cluster,
    input_name: str,
    value: float,
    signal: str,
    duration_ms: int,
):
    bmsw = bmsw_cluster.bmsw0
    segment = bmsw_cluster.components[0]
    if input_name == "cell":
        segment.set_cell_voltage(0, value)
    else:
        segment.set_temperature(0, value)

    bmsw_cluster.run_for(duration_ms, step=100)

    faults = _faults(bmsw, 0)
    assert getattr(faults, f"BMSW0_{signal}") == 1
