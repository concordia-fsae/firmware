import ctypes
import math
import multiprocessing as mp

import pytest

from sim.infra.rig import ClusterRig, DataPath, ModelRig
from sim.models.components.battery_source import BatterySourceModel, BatterySourceSpec
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec
from sim.models.test import FakeNode


class ScalarSink(ModelRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.values: list[float] = []
        self.add_scalar_input(path, send=self._send)

    def _send(self, value: float) -> bool:
        self.values.append(value)
        return True


def _run_native_feedback_dataflow_once(result_queue) -> None:
    from sim.infra.rig.runtime import _RustClusterRuntime

    runtime = _RustClusterRuntime()
    runtime.add_node("battery", FakeNode())
    runtime.add_node("load", FakeNode())
    register_battery = runtime.bind_symbol(
        "rig_model_register_battery_source",
        [
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float,
        ],
        ctypes.c_bool,
    )
    register_load = runtime.bind_symbol(
        "rig_model_register_dc_load",
        [
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_uint64,
        ],
        ctypes.c_bool,
    )
    assert register_battery(
        ctypes.c_uint32(runtime.node_index("battery")),
        ctypes.c_uint32(10),
        ctypes.c_float(12.0),
        ctypes.c_float(0.01),
        ctypes.c_float(1.0),
        ctypes.c_float(0.0),
        ctypes.c_float(0.0),
        ctypes.c_float(0.0),
        ctypes.c_float(0.0),
    )
    assert register_load(
        ctypes.c_uint32(runtime.node_index("load")),
        ctypes.c_uint32(11),
        ctypes.c_uint32(12),
        ctypes.c_float(1.0),
        ctypes.c_float(0.0),
        ctypes.c_float(0.0),
        ctypes.c_uint64(0),
    )
    source_count, source_recv_many, _ = runtime.noop_scalar_route_abi
    assert runtime.add_scalar_input_route(
        source_node="battery",
        source_route_id=10,
        source_count=source_count,
        source_recv_many=source_recv_many,
        sink_node="load",
        sink_route_id=11,
    )
    assert runtime.add_scalar_state_route(
        source_node="load",
        route_id=12,
        source_count=source_count,
        source_recv_many=source_recv_many,
        sink_node="battery",
        sink_route_id=13,
    )

    runtime.run_for(1, 1, route=False)
    result_queue.put(runtime.elapsed_ns())


def test_native_feedback_dataflow_chain_terminates_from_python_abi():
    context = mp.get_context("spawn")
    result_queue = context.Queue()
    process = context.Process(
        target=_run_native_feedback_dataflow_once,
        args=(result_queue,),
    )
    process.start()
    process.join(timeout=5)
    if process.is_alive():
        process.terminate()
        process.join(timeout=1)
    assert process.exitcode == 0
    assert result_queue.get(timeout=1) == 1


@pytest.mark.parametrize(
    "kwargs",
    [
        {"voltage": -1.0},
        {"voltage": math.inf},
        {"voltage": math.nan},
        {"voltage": 12.0, "internal_resistance_ohms": -1.0},
        {"voltage": 12.0, "capacity_amp_hours": 0.0},
        {"voltage": 12.0, "rc1_resistance_ohms": -1.0},
        {"voltage": 12.0, "rc1_capacitance_farads": math.inf},
        {"voltage": 12.0, "rc2_resistance_ohms": math.nan},
    ],
)
def test_battery_source_spec_rejects_invalid_values(kwargs):
    with pytest.raises(ValueError):
        BatterySourceSpec(**kwargs)


def test_battery_source_routes_nominal_voltage_from_native_model():
    path = BatterySourceModel.voltage_output_channel(object())
    battery = BatterySourceModel(
        voltage_output_channel=path,
        source_spec=BatterySourceSpec(voltage=12.0),
    )
    sink = ScalarSink(path)
    cluster = ClusterRig(battery=battery, sink=sink)

    cluster.run_for(1)

    assert sink.values == pytest.approx([12.0])
    assert battery.voltage == pytest.approx(12.0)
    assert cluster.dataroutes.latest_record(
        path, source_node="battery"
    ).payload == pytest.approx(12.0)


def test_battery_source_voltage_sags_under_resistive_load():
    voltage_path = BatterySourceModel.voltage_output_channel(object())
    current_path = DcLoadModel.current_output_channel(object())
    battery = BatterySourceModel(
        voltage_output_channel=voltage_path,
        source_spec=BatterySourceSpec(
            voltage=12.0,
            internal_resistance_ohms=0.01,
        ),
        current_drain_channels=(current_path,),
    )
    load = DcLoadModel(
        voltage_input_channel=voltage_path,
        current_output_channel=current_path,
        load_spec=DcLoadSpec(resistance_ohms=2.0),
    )
    sink = ScalarSink(voltage_path)
    cluster = ClusterRig(battery=battery, load=load, sink=sink)

    cluster.run_for(20)

    assert load.output_current == pytest.approx(5.97, abs=0.01)
    assert battery.voltage == pytest.approx(11.94, abs=0.01)
    assert sink.values[0] == pytest.approx(12.0)
    assert sink.values[-1] == pytest.approx(11.94, abs=0.01)


def test_battery_source_two_rc_branches_settle_at_100_hz():
    voltage_path = BatterySourceModel.voltage_output_channel(object())
    current_path = DcLoadModel.current_output_channel(object())
    battery = BatterySourceModel(
        voltage_output_channel=voltage_path,
        source_spec=BatterySourceSpec(
            voltage=12.0,
            rc1_resistance_ohms=0.1,
            rc1_capacitance_farads=1.0,
            rc2_resistance_ohms=0.2,
            rc2_capacitance_farads=2.0,
        ),
        current_drain_channels=(current_path,),
    )
    load = DcLoadModel(
        voltage_input_channel=voltage_path,
        current_output_channel=current_path,
        load_spec=DcLoadSpec(resistance_ohms=2.0),
    )
    cluster = ClusterRig(battery=battery, load=load)

    cluster.run_for(10)
    first_voltage = battery.voltage
    expected_first_voltage = 12.0 - 6.0 * (
        0.1 * (1.0 - math.exp(-0.009 / 0.1)) + 0.2 * (1.0 - math.exp(-0.009 / 0.4))
    )
    assert first_voltage == pytest.approx(expected_first_voltage, abs=0.002)

    cluster.run_for(10)
    assert battery.voltage < first_voltage
    assert load.output_current == pytest.approx(battery.voltage / 2.0, abs=0.002)
