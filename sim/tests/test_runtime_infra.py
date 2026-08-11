import pytest

from sim.infra.rig import DataPath
from sim.models.components.dc_load import DcLoadModel
from sim.models.controllers.vcpdu.fixtures import vcpdu_cluster


def dc_loads(cluster):
    return tuple(
        component
        for component in cluster.components
        if isinstance(component, DcLoadModel)
    )


def test_runtime_timer_streams_are_timestamped_after_scheduler_runs(vcpdu_cluster):
    vcpdu_cluster.run_for(20)

    for load in dc_loads(vcpdu_cluster):
        timer_stream = load.voltage_input_channel
        assert isinstance(timer_stream, DataPath)
        event = vcpdu_cluster.comm.timer.latest_event(
            timer_stream,
            node="vcpdu",
        )
        assert event is not None
        assert event.timestamp_ns > 0
        assert vcpdu_cluster.comm.timer.records(timer_stream)[-1].path == timer_stream


def test_dc_loads_auto_bind_voltage_and_current_paths(
    vcpdu_cluster,
):
    vcpdu_cluster.run_for(20)

    for load in dc_loads(vcpdu_cluster):
        assert vcpdu_cluster.vcpdu.datapaths.inputs(load.current_output_channel)
        current_records = vcpdu_cluster.dataroutes.records(load.current_output_channel)
        assert current_records
        assert current_records[-1].payload == pytest.approx(load.output_current)
        assert load.output_current == pytest.approx(
            load.input_voltage / load.load_spec.resistance_ohms
        )
