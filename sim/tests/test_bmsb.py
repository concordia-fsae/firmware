import pytest

from sim.models.components.battery_source import BatterySourceModel
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec
from sim.infra.models import SimpleCanComponent
from sim.models.controllers.bmsb import (
    AnalogInput,
    DigitalIo,
    DigitalStatus,
    PrechargeContactorState,
)
from sim.models.controllers.bmsw import BmswSimpleCluster
from sim.models.controllers.vcpdu import VcpduSimpleModel
from sim.models.controllers.bmsb.fixtures import bmsb_cluster

PRECHARGE_TIME_MS = 1400


def _enable_bmsb_inputs(bmsb) -> None:
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.TSMS_CHG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        bmsb.set_digital_io(input_, True)


def _configure_bmsb_contactor_inputs(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    bmsw = BmswSimpleCluster.for_platform(
        bmsb.can,
        bmsb_cluster.hardware,
    )
    bmsw.add_to(bmsb_cluster)
    bmsb_cluster.reset()
    _enable_bmsb_inputs(bmsb)
    bmsb.set_digital_io(DigitalIo.VPACK_DIAG, False)
    bmsb.set_analog_input(AnalogInput.VPACK, 350.0)
    return bmsb, bmsw


def _check_contactor_state(bmsb, expected_state):
    expected_outputs = {
        PrechargeContactorState.OPEN: (False, False),
        PrechargeContactorState.PRECHARGE_CLOSED: (False, True),
        PrechargeContactorState.PRECHARGE_HVP_CLOSED: (True, True),
        PrechargeContactorState.HVP_CLOSED: (True, False),
    }
    information = bmsb.can.latest("BMSB_information", bus="veh")
    assert information is not None
    assert information.BMSB_packContactorState == expected_state
    assert expected_state in expected_outputs
    air_expected, precharge_expected = expected_outputs[expected_state]
    assert bmsb.get_digital_io(DigitalIo.AIR) == air_expected
    assert bmsb.get_digital_io(DigitalIo.PCHG) == precharge_expected


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
    DcLoadModel.current_output.bind_to(bmsb.__class__.pack_current_input()).bind(
        bmsb, load
    )
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
    assert bmsb.get_analog_input(bmsb.AnalogInput.VPACK) == pytest.approx(
        350.0, abs=0.2
    )
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
    assert bmsb.get_analog_input(bmsb.AnalogInput.CS) == pytest.approx(-0.25, abs=0.001)

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
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)

    request.set(VCREAR_requestContactorsOpen=DigitalStatus.ON)
    bmsb_cluster.run_for(120)

    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)


def test_bmsb_opens_contactors_when_tsms_chg_is_removed(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    BmswSimpleCluster.for_platform(
        bmsb.can,
        bmsb_cluster.hardware,
    ).add_to(bmsb_cluster)
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)
    charger = charger_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=0.0,
    )
    bmsb_cluster.reset()
    _enable_bmsb_inputs(bmsb)
    bmsb.set_digital_io(DigitalIo.VPACK_DIAG, False)
    bmsb.set_analog_input(AnalogInput.VPACK, 350.0)
    bmsb_cluster.run_for(1000)
    charger.set(BRUSA513_dcBusVoltage=340.0)
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    bmsb_cluster.run_for(10000)

    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)


def test_bmsb_recloses_contactors_when_tsms_chg_returns(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    BmswSimpleCluster.for_platform(
        bmsb.can,
        bmsb_cluster.hardware,
    ).add_to(bmsb_cluster)
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)
    charger = charger_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=0.0,
    )
    bmsb_cluster.reset()
    _enable_bmsb_inputs(bmsb)
    bmsb.set_digital_io(DigitalIo.VPACK_DIAG, False)
    bmsb.set_analog_input(AnalogInput.VPACK, 350.0)
    bmsb_cluster.run_for(1000)
    charger.set(BRUSA513_dcBusVoltage=340.0)
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    bmsb_cluster.run_for(10000)
    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, True)
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)


def test_bmsb_tsms_debounce_keeps_and_clears_the_can_state(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)
    charger = charger_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=0.0,
    )
    _configure_bmsb_contactor_inputs(bmsb_cluster)
    bmsb_cluster.run_for(1000)
    charger.set(BRUSA513_dcBusVoltage=340.0)
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)
    io_status = bmsb.can.latest("BMSB_ioStatus", bus="veh")
    assert io_status is not None
    assert io_status.BMSB_tsmsChg == DigitalStatus.ON

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    # A dropout at the debounce boundary must not clear the stable state.
    bmsb_cluster.run_for(25)
    io_status = bmsb.can.latest("BMSB_ioStatus", bus="veh")
    assert io_status is not None
    assert io_status.BMSB_tsmsChg == DigitalStatus.ON
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, True)
    bmsb_cluster.run_for(100)

    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    bmsb_cluster.run_for(100)

    io_status = bmsb.can.latest("BMSB_ioStatus", bus="veh")
    assert io_status is not None
    assert io_status.BMSB_tsmsChg == DigitalStatus.OFF
    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)


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

    charger.set(BRUSA513_dcBusVoltage=0.0)
    bmsb_cluster.run_for(2000)
    _check_contactor_state(bmsb, PrechargeContactorState.PRECHARGE_CLOSED)

    charger.set(BRUSA513_dcBusVoltage=340.0)
    bmsb_cluster.run_for(1000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)


@pytest.mark.parametrize(
    (
        "message",
        "signal",
        "initial_voltage",
        "completion_voltage",
    ),
    [
        pytest.param(
            "BRUSA513_criticalData",
            "BRUSA513_dcBusVoltage",
            0.0,
            340.0,
            id="brusa",
        ),
        pytest.param(
            "PM100DX_criticalData",
            "PM100DX_tractiveSystemVoltage",
            350.0,
            350.0,
            id="pm100dx",
        ),
    ],
)
def test_bmsb_voltage_source_completes_precharge(
    bmsb_cluster,
    message,
    signal,
    initial_voltage,
    completion_voltage,
):
    bmsb = bmsb_cluster.bmsb
    source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(source)
    periodic = source.periodic_send(
        message,
        period=10,
        **{signal: initial_voltage},
    )
    _configure_bmsb_contactor_inputs(bmsb_cluster)

    bmsb_cluster.run_for(PRECHARGE_TIME_MS)
    _check_contactor_state(bmsb, PrechargeContactorState.PRECHARGE_CLOSED)

    periodic.set(**{signal: completion_voltage})
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)


def test_bmsb_precharge_does_not_complete_without_bus_voltage(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)
    charger_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=0.0,
    )
    _configure_bmsb_contactor_inputs(bmsb_cluster)

    bmsb_cluster.run_for(10000)

    _check_contactor_state(bmsb, PrechargeContactorState.PRECHARGE_CLOSED)


def test_bmsb_opens_contactors_when_voltage_source_times_out(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)

    _configure_bmsb_contactor_inputs(bmsb_cluster)
    for _ in range(8):
        charger_source.send(
            "BRUSA513_criticalData",
            BRUSA513_dcBusVoltage=350.0,
        )
        bmsb_cluster.run_for(500)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)

    bmsb_cluster.run_for(10000)

    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)


def test_bmsb_reports_contactor_io_states_across_hv_cycle(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)
    charger_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=340.0,
    )
    _configure_bmsb_contactor_inputs(bmsb_cluster)

    # INIT/GLV: TSMS is open and both physical contactor outputs stay open.
    bmsb.set_digital_io(DigitalIo.TSMS_CHG, False)
    bmsb_cluster.run_for(100)
    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)
    assert not bmsb.get_digital_io(DigitalIo.AIR)
    assert not bmsb.get_digital_io(DigitalIo.PCHG)

    # HV precharge: PCHG closes while AIR remains open.
    bmsb.set_digital_io(DigitalIo.TSMS_CHG, True)
    bmsb_cluster.run_for(PRECHARGE_TIME_MS)
    _check_contactor_state(bmsb, PrechargeContactorState.PRECHARGE_CLOSED)
    assert not bmsb.get_digital_io(DigitalIo.AIR)
    assert bmsb.get_digital_io(DigitalIo.PCHG)

    # RUN: precharge completes and AIR closes while PCHG opens.
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)
    assert bmsb.get_digital_io(DigitalIo.AIR)
    assert not bmsb.get_digital_io(DigitalIo.PCHG)


def test_bmsb_opens_contactors_when_workers_disconnect(bmsb_cluster):
    bmsb = bmsb_cluster.bmsb
    charger_source = SimpleCanComponent(bmsb.can)
    bmsb_cluster.add_component(charger_source)
    charger = charger_source.periodic_send(
        "BRUSA513_criticalData",
        period=10,
        BRUSA513_dcBusVoltage=0.0,
    )
    _, bmsw = _configure_bmsb_contactor_inputs(bmsb_cluster)
    bmsb_cluster.run_for(1000)
    charger.set(BRUSA513_dcBusVoltage=350.0)
    bmsb_cluster.run_for(3000)
    _check_contactor_state(bmsb, PrechargeContactorState.HVP_CLOSED)

    node_name = bmsw.model._cluster_node_name
    assert node_name is not None
    bmsb_cluster.disable_node(node_name)
    bmsb_cluster.run_for(10000)

    _check_contactor_state(bmsb, PrechargeContactorState.OPEN)
