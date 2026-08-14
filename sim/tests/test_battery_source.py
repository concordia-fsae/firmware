import math

import pytest

from sim.infra.rig import ClusterRig, DataPath, ModelRig
from sim.models.components.battery_source import BatterySourceModel, BatterySourceSpec
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec


class ScalarSink(ModelRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.values: list[float] = []
        self.add_scalar_input(path, send=self._send)

    def _send(self, value: float) -> bool:
        self.values.append(value)
        return True


@pytest.mark.parametrize(
    "kwargs",
    [
        {"voltage": -1.0},
        {"voltage": math.inf},
        {"voltage": math.nan},
        {"voltage": 12.0, "internal_resistance_ohms": -1.0},
        {"voltage": 12.0, "capacity_amp_hours": 0.0},
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

    cluster.run_for(3)

    assert load.output_current == pytest.approx(5.97, abs=0.01)
    assert battery.voltage == pytest.approx(11.94, abs=0.01)
    assert sink.values[0] == pytest.approx(12.0)
    assert sink.values[-1] == pytest.approx(11.94, abs=0.01)
