import pytest
from dataclasses import dataclass

from sim.models.controllers.sws import SwsSimpleModel
from sim.models.controllers.vcfront import VcfrontSimpleModel
from sim.models.controllers.bmsb import BmsbSimpleModel
from sim.models.controllers.vcpdu import Vn9008Channel
from sim.models.controllers.vcpdu.fixtures import vcpdu_cluster


def test_vcpdu_boots_and_cycles_to_glv_on(vcpdu_cluster):
    VehicleState = vcpdu_cluster.vcpdu.can.enums.VehicleState
    observed = []

    _run_vcpdu_to_glv_on(vcpdu_cluster, observed)

    assert observed == [
        VehicleState.INIT,
        VehicleState.ON_GLV,
    ]


@dataclass
class VcpduHvOnSetup:
    cluster: object
    vcpdu: object
    tsms_status: object


@dataclass
class VcpduTsRunSetup:
    cluster: object
    vcpdu: object
    tsms_status: object
    contactor_state: object
    run_request: object
    brake_position: object


@dataclass
class VcpduSimpleSources:
    bmsb: BmsbSimpleModel | None = None
    sws: SwsSimpleModel | None = None
    vcfront: VcfrontSimpleModel | None = None


@pytest.fixture
def vcpdu_hv_on(vcpdu_cluster):
    vcpdu = vcpdu_cluster.vcpdu
    sources = _add_vcpdu_simple_sources(vcpdu_cluster, bmsb=True)
    assert sources.bmsb is not None
    DigitalStatus = sources.bmsb.can.enums.DigitalStatus
    VehicleState = vcpdu.can.enums.VehicleState

    tsms_status = sources.bmsb.periodic_io_status(
        period=10,
        BMSB_tsmsChg=DigitalStatus.OFF,
    )
    vcpdu_cluster.add_component(sources.bmsb)

    _run_vcpdu_to_glv_on(vcpdu_cluster)

    tsms_status.set(BMSB_tsmsChg=DigitalStatus.ON)
    vcpdu.run_until_vehicle_state(
        VehicleState.ON_HV,
        timeout=500,
        step=20,
        message="vcpdu should enter ON_HV when TSMS closes",
    )
    return VcpduHvOnSetup(vcpdu_cluster, vcpdu, tsms_status)


def test_vcpdu_enters_hv_on_when_tsms_closes(vcpdu_hv_on):
    vcpdu = vcpdu_hv_on.vcpdu
    VehicleState = vcpdu.can.enums.VehicleState

    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV


def test_vcpdu_exits_hv_when_tsms_opens(vcpdu_hv_on):
    vcpdu_cluster = vcpdu_hv_on.cluster
    tsms_status = vcpdu_hv_on.tsms_status
    vcpdu = vcpdu_hv_on.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    VehicleState = vcpdu.can.enums.VehicleState

    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV

    tsms_status.set(BMSB_tsmsChg=DigitalStatus.OFF)
    vcpdu_cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_GLV


@pytest.fixture
def vcpdu_ts_run_inputs(vcpdu_cluster):
    vcpdu = vcpdu_cluster.vcpdu
    sources = _add_vcpdu_simple_sources(
        vcpdu_cluster,
        bmsb=True,
        sws=True,
        vcfront=True,
    )
    assert sources.bmsb is not None
    assert sources.sws is not None
    assert sources.vcfront is not None
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    PrechargeContactorState = vcpdu.can.enums.PrechargeContactorState
    VehicleState = vcpdu.can.enums.VehicleState

    tsms_status = sources.bmsb.periodic_io_status(
        period=10,
        BMSB_tsmsChg=DigitalStatus.OFF,
    )
    contactor_state = sources.bmsb.periodic_pack_contactor_state(
        PrechargeContactorState.HVP_CLOSED,
        period=10,
    )
    run_request = sources.sws.periodic_driver_request(
        period=10,
        SWS_requestRun=DigitalStatus.OFF,
    )
    brake_position = sources.vcfront.periodic_pedal_position(
        period=10,
        VCFRONT_brakePosition=0,
    )
    vcpdu_cluster.add_components(sources.bmsb, sources.sws, sources.vcfront)

    _run_vcpdu_to_glv_on(vcpdu_cluster)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_GLV

    tsms_status.set(BMSB_tsmsChg=DigitalStatus.ON)
    vcpdu.run_until_vehicle_state(
        VehicleState.ON_HV,
        timeout=500,
        step=20,
        message="vcpdu should enter ON_HV before TS_RUN inputs are applied",
    )

    return VcpduTsRunSetup(
        vcpdu_cluster,
        vcpdu,
        tsms_status,
        contactor_state,
        run_request,
        brake_position,
    )


@pytest.mark.parametrize(
    ("brake_position", "expected_ts_run"),
    [
        pytest.param(0, False, id="run-request-without-brake"),
        pytest.param(12, True, id="run-request-with-brake"),
    ],
)
def test_vcpdu_enters_ts_run_only_when_driver_requests_run_with_brake_applied(
    vcpdu_ts_run_inputs,
    brake_position,
    expected_ts_run,
):
    setup = vcpdu_ts_run_inputs
    vcpdu = setup.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    VehicleState = vcpdu.can.enums.VehicleState

    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV

    setup.brake_position.set(VCFRONT_brakePosition=brake_position)
    setup.run_request.set(SWS_requestRun=DigitalStatus.ON)

    if expected_ts_run:
        vcpdu.run_until_vehicle_state(
            VehicleState.TS_RUN,
            timeout=500,
            step=20,
            message="vcpdu should enter TS_RUN with run request and brake applied",
        )
    else:
        setup.cluster.run_for(250)
        assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV

    setup.run_request.set(SWS_requestRun=DigitalStatus.OFF)
    setup.brake_position.set(VCFRONT_brakePosition=0)
    setup.cluster.run_for(250)
    expected_state = VehicleState.TS_RUN if expected_ts_run else VehicleState.ON_HV
    assert vcpdu.latest_vehicle_state() == expected_state


def test_vcpdu_exits_ts_run_when_tsms_opens(vcpdu_ts_run_inputs):
    setup = vcpdu_ts_run_inputs
    vcpdu = setup.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    VehicleState = vcpdu.can.enums.VehicleState

    setup.brake_position.set(VCFRONT_brakePosition=12)
    setup.run_request.set(SWS_requestRun=DigitalStatus.ON)
    vcpdu.run_until_vehicle_state(
        VehicleState.TS_RUN,
        timeout=500,
        step=20,
        message="vcpdu should enter TS_RUN before testing TSMS exit",
    )

    setup.tsms_status.set(BMSB_tsmsChg=DigitalStatus.OFF)
    vcpdu.run_until_vehicle_state(
        VehicleState.ON_GLV,
        timeout=500,
        step=20,
        message="vcpdu should exit TS_RUN when TSMS opens",
    )


def test_vcpdu_enters_ts_run_only_after_contactors_close(vcpdu_ts_run_inputs):
    setup = vcpdu_ts_run_inputs
    vcpdu = setup.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    PrechargeContactorState = vcpdu.can.enums.PrechargeContactorState
    VehicleState = vcpdu.can.enums.VehicleState

    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV

    setup.contactor_state.set(
        BMSB_packContactorState=PrechargeContactorState.OPEN,
    )
    setup.brake_position.set(VCFRONT_brakePosition=12)
    setup.run_request.set(SWS_requestRun=DigitalStatus.ON)

    setup.cluster.run_for(250)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV

    setup.contactor_state.set(
        BMSB_packContactorState=PrechargeContactorState.HVP_CLOSED,
    )
    vcpdu.run_until_vehicle_state(
        VehicleState.TS_RUN,
        timeout=500,
        step=20,
        message="vcpdu should enter TS_RUN once contactors close",
    )


@pytest.mark.parametrize(
    "contactor_state_name",
    [
        pytest.param("SNA", id="sna"),
        pytest.param("OPEN", id="open"),
        pytest.param("PRECHARGE_CLOSED", id="precharge-closed"),
        pytest.param("PRECHARGE_HVP_CLOSED", id="precharge-hvp-closed"),
    ],
)
def test_vcpdu_does_not_cycle_from_hv_on_to_ts_run_without_hvp_contactors_closed(
    vcpdu_ts_run_inputs,
    contactor_state_name,
):
    setup = vcpdu_ts_run_inputs
    vcpdu = setup.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    PrechargeContactorState = vcpdu.can.enums.PrechargeContactorState
    VehicleState = vcpdu.can.enums.VehicleState
    observed = []

    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV

    setup.contactor_state.set(
        BMSB_packContactorState=getattr(PrechargeContactorState, contactor_state_name),
    )
    setup.brake_position.set(VCFRONT_brakePosition=12)
    setup.run_request.set(SWS_requestRun=DigitalStatus.ON)

    for _ in range(50):
        setup.cluster.run_for(10)
        vcpdu.record_latest_vehicle_state(observed)

    assert VehicleState.TS_RUN not in observed
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_HV


@pytest.mark.parametrize(
    "brake_position",
    [
        pytest.param(0, id="brake-released"),
        pytest.param(12, id="brake-applied"),
    ],
)
def test_vcpdu_stays_in_ts_run_after_additional_driver_run_request(
    vcpdu_ts_run_inputs,
    brake_position,
):
    setup = vcpdu_ts_run_inputs
    vcpdu = setup.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    VehicleState = vcpdu.can.enums.VehicleState

    setup.brake_position.set(VCFRONT_brakePosition=12)
    setup.run_request.set(SWS_requestRun=DigitalStatus.ON)
    vcpdu.run_until_vehicle_state(
        VehicleState.TS_RUN,
        timeout=500,
        step=20,
        message="vcpdu should enter TS_RUN before additional driver run request",
    )
    setup.cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN

    setup.run_request.set(SWS_requestRun=DigitalStatus.OFF)
    setup.brake_position.set(VCFRONT_brakePosition=brake_position)
    setup.cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN

    setup.run_request.set(SWS_requestRun=DigitalStatus.ON)
    setup.cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.TS_RUN


def _run_vcpdu_to_glv_on(vcpdu_cluster, observed: list | None = None) -> None:
    VehicleState = vcpdu_cluster.vcpdu.can.enums.VehicleState
    vcpdu = vcpdu_cluster.vcpdu

    vcpdu.run_until_vehicle_state(
        VehicleState.INIT,
        timeout=500,
        step=10,
        message="vcpdu should report INIT while booting",
    )
    if observed is not None:
        vcpdu.record_latest_vehicle_state(observed)
    vcpdu.run_until_vehicle_state(
        VehicleState.ON_GLV,
        timeout=500,
        step=10,
        message="vcpdu should boot to ON_GLV",
    )
    if observed is not None:
        vcpdu.record_latest_vehicle_state(observed)


@pytest.mark.parametrize(
    ("controller", "wake_state_name"),
    [
        pytest.param("sws", "SNA", id="sws-sna"),
        pytest.param("sws", "NOK_TO_SLEEP", id="sws-nok-to-sleep"),
        pytest.param("sws", "ALARM", id="sws-alarm"),
        pytest.param("vcfront", "SNA", id="vcfront-sna"),
        pytest.param("vcfront", "NOK_TO_SLEEP", id="vcfront-nok-to-sleep"),
        pytest.param("vcfront", "ALARM", id="vcfront-alarm"),
    ],
)
def test_vcpdu_sleeps_then_wakes_from_waking_controller_sleepable_state(
    vcpdu_cluster,
    controller,
    wake_state_name,
):
    VehicleState = vcpdu_cluster.vcpdu.can.enums.VehicleState
    SleepFollowerState = vcpdu_cluster.vcpdu.can.enums.SleepFollowerState
    vcpdu = vcpdu_cluster.vcpdu
    observed = []
    wake_state = getattr(SleepFollowerState, wake_state_name)
    sources = _add_vcpdu_simple_sources(vcpdu_cluster, sws=True, vcfront=True)
    assert sources.sws is not None
    assert sources.vcfront is not None

    sleepable_inputs = {
        "sws": sources.sws.periodic_sleepable(
            SleepFollowerState.OK_TO_SLEEP,
            period=100,
        ),
        "vcfront": sources.vcfront.periodic_sleepable(
            SleepFollowerState.OK_TO_SLEEP,
            period=100,
        ),
    }
    vcpdu_cluster.add_components(sources.sws, sources.vcfront)

    vcpdu.run_until_vehicle_state(
        VehicleState.ON_GLV,
        timeout=500,
        step=20,
        message="vcpdu should boot to ON_GLV before it can sleep",
    )
    vcpdu.record_latest_vehicle_state(observed)

    vcpdu_cluster.run_for(100)
    assert vcpdu.latest_vehicle_state() == VehicleState.ON_GLV

    vcpdu.run_until_vehicle_state(
        VehicleState.SLEEP,
        timeout=16 * 60000,
        step=1000,
        fast_forward=True,
        message="vcpdu should enter SLEEP when all waking controllers are OK to sleep",
    )
    vcpdu.record_latest_vehicle_state(observed)

    sleepable_inputs[controller].set(**{f"{controller.upper()}_sleepable": wake_state})
    before_wake = len(observed)
    vcpdu.run_until_vehicle_state(
        VehicleState.INIT,
        timeout=1000,
        step=20,
        message=f"vcpdu should wake to INIT from {controller} {wake_state.name}",
    )
    vcpdu.record_latest_vehicle_state(observed)
    vcpdu.run_until_vehicle_state(
        VehicleState.ON_GLV,
        timeout=1000,
        step=20,
        message=f"vcpdu should wake from {controller} {wake_state.name}",
    )
    vcpdu.record_latest_vehicle_state(observed)

    assert observed[before_wake:] == [
        VehicleState.INIT,
        VehicleState.ON_GLV,
    ]


@pytest.mark.parametrize(
    "hsd_channel",
    [
        pytest.param(Vn9008Channel.PUMP, id="pump"),
        pytest.param(Vn9008Channel.FAN, id="fan"),
    ],
)
def test_driver_cooling_request_toggles_and_latches_load_current(
    vcpdu_cluster,
    hsd_channel,
):
    vcpdu = vcpdu_cluster.vcpdu
    DigitalStatus = vcpdu.can.enums.DigitalStatus
    sources = _add_vcpdu_simple_sources(vcpdu_cluster, sws=True)
    assert sources.sws is not None
    driver_request = sources.sws.periodic_driver_request(
        period=20,
    )
    vcpdu_cluster.add_component(sources.sws)
    request_signal = _driver_request_signal(vcpdu, hsd_channel)

    vcpdu_cluster.run_until(
        lambda: vcpdu.latest_hsd_duty_cycle(hsd_channel) == 0
        and vcpdu.latest_hsd_current(hsd_channel) == 0,
        timeout=500,
        step=20,
        message="VCPDU HSD load should report off before a driver request",
    )

    driver_request.set(**{request_signal: DigitalStatus.ON})
    vcpdu_cluster.run_until(
        lambda: _positive(vcpdu.latest_hsd_duty_cycle(hsd_channel))
        and _positive(vcpdu.latest_hsd_current(hsd_channel)),
        timeout=1000,
        step=20,
        message="VCPDU HSD load should report non-zero current when requested on",
    )

    driver_request.set(**{request_signal: DigitalStatus.OFF})
    vcpdu_cluster.run_for(250)
    assert _positive(vcpdu.latest_hsd_duty_cycle(hsd_channel))
    assert _positive(vcpdu.latest_hsd_current(hsd_channel))

    driver_request.set(**{request_signal: DigitalStatus.ON})
    vcpdu_cluster.run_until(
        lambda: vcpdu.latest_hsd_duty_cycle(hsd_channel) == 0
        and vcpdu.latest_hsd_current(hsd_channel) == 0,
        timeout=1000,
        step=20,
        message="VCPDU HSD load should report zero current after a second driver request toggles it off",
    )


@pytest.mark.parametrize(
    ("bms_safety_enabled", "imd_safety_enabled"),
    [
        pytest.param(True, True, id="no-fault"),
        pytest.param(True, False, id="imd-fault"),
        pytest.param(False, True, id="bms-fault"),
    ],
)
def test_bmsb_faults_sets_safety_fault(
    vcpdu_cluster,
    bms_safety_enabled,
    imd_safety_enabled,
):
    vcpdu = vcpdu_cluster.vcpdu
    sources = _add_vcpdu_simple_sources(vcpdu_cluster, bmsb=True)
    assert sources.bmsb is not None
    ShutdownCircuitStatus = sources.bmsb.can.enums.ShutdownCircuitStatus
    DigitalStatus = sources.bmsb.can.enums.DigitalStatus
    enabled_status = {
        True: DigitalStatus.ON,
        False: DigitalStatus.OFF,
    }
    expected_status = {
        True: ShutdownCircuitStatus.CLOSED,
        False: ShutdownCircuitStatus.OPEN,
    }

    sources.bmsb.periodic_io_status(
        period=10,
        BMSB_bmsStatusMem=enabled_status[bms_safety_enabled],
        BMSB_imdStatusMem=enabled_status[imd_safety_enabled],
    )
    vcpdu_cluster.add_component(sources.bmsb)

    vcpdu.can.run_until_signal_eq(
        "VCPDU_vehicleState",
        "VCPDU_bmsbSafetyStatus",
        expected_status[bms_safety_enabled],
        bus="veh",
        timeout=1000,
        step=20,
        message_on_timeout="vcpdu should report BMSB safety status from BMSB_ioStatus",
    )
    vcpdu.can.run_until_signal_eq(
        "VCPDU_vehicleState",
        "VCPDU_imdSafetyStatus",
        expected_status[imd_safety_enabled],
        bus="veh",
        timeout=1000,
        step=20,
        message_on_timeout="vcpdu should report IMD safety status from BMSB_ioStatus",
    )

    status = vcpdu.latest_vehicle_state_message()

    assert status.VCPDU_bmsbSafetyStatus == expected_status[bms_safety_enabled]
    assert status.VCPDU_imdSafetyStatus == expected_status[imd_safety_enabled]


def _driver_request_signal(vcpdu, hsd_channel) -> str:
    if int(hsd_channel) == int(vcpdu.Vn9008Channel.PUMP):
        return "SWS_requestTestPump"
    if int(hsd_channel) == int(vcpdu.Vn9008Channel.FAN):
        return "SWS_requestTestFan"
    raise ValueError(f"unsupported HSD channel {hsd_channel!r}")


def _positive(value: float | None) -> bool:
    return value is not None and value > 0


def _add_vcpdu_simple_sources(
    vcpdu_cluster,
    *,
    bmsb: bool = False,
    sws: bool = False,
    vcfront: bool = False,
) -> VcpduSimpleSources:
    can = vcpdu_cluster.vcpdu.can
    sources = VcpduSimpleSources(
        bmsb=BmsbSimpleModel(can) if bmsb else None,
        sws=SwsSimpleModel(can) if sws else None,
        vcfront=VcfrontSimpleModel(can) if vcfront else None,
    )
    return sources
