import pytest

from sim.models.components.battery_source import BatterySourceModel
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec
from sim.infra.models import SimpleCanComponent
from sim.models.controllers.bmsb import AnalogInput, DigitalIo, DigitalStatus
from sim.models.controllers.bmsw import BmswSimpleCluster
from sim.models.controllers.vcpdu import VcpduSimpleModel
from sim.models.controllers.bmsb.fixtures import bmsb_cluster


def _enable_bmsb_inputs(bmsb) -> None:
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.TSMS_CHG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        bmsb.set_digital_io(input_, True)


def test_bmsb_hvdc_load_reports_voltage_sag_and_current(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    battery = next(
        component
        for component in bmsb_cluster.components
        if isinstance(component, BatterySourceModel)
    )
    voltage_path = battery.terminal_voltage_output_channel
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


def test_bmsb_respects_vcrear_contactor_open_request(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    BmswSimpleCluster.for_platform(
        bmsb.can,
        bmsb_cluster.hardware,
    ).add_to(bmsb_cluster)
    vcpdu = VcpduSimpleModel(bmsb.can)
    bmsb_cluster.add_component(vcpdu)
    pm_source = SimpleCanComponent(bmsb.can)
    elcon_source = SimpleCanComponent(bmsb.can, buses=("privbms",))
    bmsb_cluster.add_components(pm_source, elcon_source)
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.TSMS_CHG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        bmsb.set_digital_io(input_, True)
    request = vcpdu.can_component.periodic_send(
        "VCREAR_request",
        period=10,
        VCREAR_requestContactorsOpen=DigitalStatus.OFF,
    )
    charger = vcpdu.can_component.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=340.0,
    )
    pm_source.periodic_send(
        "PM100DX_criticalData",
        period=10,
        PM100DX_tractiveSystemVoltage=350.0,
    )
    elcon_source.periodic_send(
        "ELCON_criticalData",
        bus="privbms",
        period=10,
        ELCON_busVoltage=350.0,
    )
    bmsb_cluster.reset()
    _enable_bmsb_inputs(bmsb)
    bmsb_cluster.run_for(3000)
    assert bmsb.get_digital_io(DigitalIo.AIR)
    assert not bmsb.get_digital_io(DigitalIo.PCHG)

    request.set(VCREAR_requestContactorsOpen=DigitalStatus.ON)
    bmsb_cluster.run_for(20)

    assert not bmsb.get_digital_io(DigitalIo.AIR)
    assert not bmsb.get_digital_io(DigitalIo.PCHG)


def test_bmsb_opens_contactors_when_tsms_chg_is_removed(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    BmswSimpleCluster.for_platform(
        bmsb.can,
        bmsb_cluster.hardware,
    ).add_to(bmsb_cluster)
    vcpdu = VcpduSimpleModel(bmsb.can)
    bmsb_cluster.add_component(vcpdu)
    pm_source = SimpleCanComponent(bmsb.can)
    elcon_source = SimpleCanComponent(bmsb.can, buses=("privbms",))
    bmsb_cluster.add_components(pm_source, elcon_source)
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.TSMS_CHG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        bmsb.set_digital_io(input_, True)
    vcpdu.can_component.periodic_send(
        "VCREAR_request",
        period=10,
        VCREAR_requestContactorsOpen=DigitalStatus.OFF,
    )
    vcpdu.can_component.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=340.0,
    )
    pm_source.periodic_send(
        "PM100DX_criticalData",
        period=10,
        PM100DX_tractiveSystemVoltage=350.0,
    )
    elcon_source.periodic_send(
        "ELCON_criticalData",
        bus="privbms",
        period=10,
        ELCON_busVoltage=350.0,
    )
    bmsb_cluster.reset()
    _enable_bmsb_inputs(bmsb)
    bmsb_cluster.run_for(3000)
    assert bmsb.get_digital_io(DigitalIo.AIR)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    bmsb_cluster.run_for(10000)

    assert not bmsb.get_digital_io(DigitalIo.AIR)
    assert not bmsb.get_digital_io(DigitalIo.PCHG)


def test_bmsb_brusa_precharge_requires_more_than_ninety_five_percent_pack_voltage(
    bmsb_cluster,
):
    bmsb = bmsb_cluster.bmsb
    BmswSimpleCluster.for_platform(
        bmsb.can,
        bmsb_cluster.hardware,
    ).add_to(bmsb_cluster)
    can_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(can_source)
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.TSMS_CHG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        bmsb.set_digital_io(input_, True)
    charger = can_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=0.0,
    )
    bmsb_cluster.reset()
    _enable_bmsb_inputs(bmsb)
    bmsb.set_digital_io(DigitalIo.VPACK_DIAG, False)
    bmsb.set_analog_input(AnalogInput.VPACK, 350.0)
    bmsb_cluster.run_for(1000)
    assert not bmsb.get_digital_io(DigitalIo.AIR)
    assert bmsb.get_digital_io(DigitalIo.PCHG)

    charger.set(BRUSA513_dcBusVoltage=0.0)
    bmsb_cluster.run_for(2000)
    assert not bmsb.get_digital_io(DigitalIo.AIR)
    assert bmsb.get_digital_io(DigitalIo.PCHG)

    charger.set(BRUSA513_dcBusVoltage=340.0)
    bmsb_cluster.run_for(1000)
    assert bmsb.get_digital_io(DigitalIo.AIR)
    assert not bmsb.get_digital_io(DigitalIo.PCHG)
