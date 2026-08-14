import pytest

from sim.models.components.battery_source import BatterySourceModel
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec
from sim.models.controllers.bmsb.fixtures import bmsb_cluster


def test_bmsb_hvdc_load_reports_voltage_sag_and_current(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    battery = next(
        component
        for component in bmsb_cluster.components
        if isinstance(component, BatterySourceModel)
    )
    voltage_path = battery.voltage_output_channel
    current_path = DcLoadModel.current_output_channel("bmsb-load")
    load = DcLoadModel(
        voltage_input_channel=voltage_path,
        current_output_channel=current_path,
        load_spec=DcLoadSpec(resistance_ohms=3.45),
    )
    DcLoadModel.current_output.bind_to(
        bmsb.__class__.pack_current_input()
    ).bind(bmsb, load)
    bmsb_cluster.add_component(load)
    load_node = load._cluster_node_name
    assert load_node is not None

    def latest_measurements() -> tuple[float, float]:
        critical = bmsb.can.latest("BMSB_criticalData", bus="veh")
        debug = bmsb.can.latest("BMSB_informationDebug", bus="veh")
        assert critical is not None
        assert debug is not None
        return bmsb.get_analog_input(bmsb.AnalogInput.VPACK), critical.BMSB_packCurrent

    bmsb_cluster.disable_node(load_node)
    bmsb_cluster.run_for(10000)
    assert bmsb.get_analog_input(bmsb.AnalogInput.VPACK) == pytest.approx(350.0, abs=0.2)
    voltage, current = latest_measurements()
    assert voltage == pytest.approx(350.0, abs=0.2)
    assert current == pytest.approx(0.0, abs=0.1)

    bmsb_cluster.enable_node(load_node)
    bmsb_cluster.run_for(10000)
    loaded_voltage, loaded_current = latest_measurements()

    assert loaded_voltage == pytest.approx(345.0, abs=0.2)
    assert loaded_current == pytest.approx(100.0, abs=0.1)
    assert load.output_current == pytest.approx(100.0, abs=0.1)
    assert bmsb.get_analog_input(bmsb.AnalogInput.VPACK) == pytest.approx(
        loaded_voltage, abs=0.2
    )
    assert bmsb.get_analog_input(bmsb.AnalogInput.CS) == pytest.approx(
        -0.25, abs=0.001
    )

    bmsb_cluster.reset_to_initial_topology()
    bmsb_cluster.run_for(10000)
    recovered_voltage, recovered_current = latest_measurements()

    assert recovered_voltage == pytest.approx(350.0, abs=0.2)
    assert recovered_current == pytest.approx(0.0, abs=0.1)
