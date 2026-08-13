import math

import pytest

from sim.infra.rig import DataPath, TimerChannelEvent
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec


@pytest.mark.parametrize(
    "kwargs",
    [
        {},
        {"resistance_ohms": math.inf},
        {"inductance_henrys": math.inf},
        {"capacitance_farads": math.inf},
        {"resistance_ohms": 0.0, "inductance_henrys": math.inf},
        {"resistance_ohms": math.inf, "capacitance_farads": 0.0},
    ],
)
def test_dc_load_spec_rejects_invalid_or_empty_components(kwargs):
    with pytest.raises(ValueError):
        DcLoadSpec(**kwargs)


@pytest.mark.parametrize(
    "kwargs",
    [
        {"resistance_ohms": -1.0},
        {"inductance_henrys": -1.0},
        {"capacitance_farads": -1.0},
        {"resistance_ohms": math.nan},
        {"inductance_henrys": math.nan},
        {"capacitance_farads": math.nan},
    ],
)
def test_dc_load_spec_rejects_invalid_values(kwargs):
    with pytest.raises(ValueError):
        DcLoadSpec(**kwargs)


@pytest.mark.parametrize(
    ("inductance_henrys", "capacitance_farads"),
    [
        (None, None),
        (0.0, 0.0),
        (math.inf, math.inf),
        (0.0, math.inf),
        (math.inf, 0.0),
    ],
)
def test_zero_or_infinite_lc_components_are_not_present(
    inductance_henrys,
    capacitance_farads,
):
    load = DcLoadModel(
        voltage_input_channel=DataPath.component(object(), "voltage"),
        load_spec=DcLoadSpec(
            resistance_ohms=2.0,
            inductance_henrys=inductance_henrys,
            capacitance_farads=capacitance_farads,
        ),
        scheduler_period=1,
        scheduler_unit="ms",
    )
    event = TimerChannelEvent()
    event.value = 8.0

    assert load._set_voltage_from_timer(event)
    load.run_for(1)

    assert load.output_current == pytest.approx(4.0)


def test_dc_load_lrc_spec_updates_current_every_scheduler_step():
    load = DcLoadModel(
        voltage_input_channel=DataPath.component(object(), "voltage"),
        load_spec=DcLoadSpec(
            resistance_ohms=2.0,
            inductance_henrys=4.0,
            capacitance_farads=0.001,
        ),
        scheduler_period=1,
        scheduler_unit="ms",
    )
    event = TimerChannelEvent()
    event.value = 8.0

    assert load._set_voltage_from_timer(event)
    load.run_for(1)

    assert load.output_current == pytest.approx(12.002)

    load.run_for(1)

    assert load.output_current == pytest.approx(4.004)


def test_resistive_dc_load_without_period_updates_when_voltage_changes():
    load = DcLoadModel(
        voltage_input_channel=DataPath.component(object(), "voltage"),
        load_spec=DcLoadSpec(resistance_ohms=2.0),
    )
    event = TimerChannelEvent()
    event.value = 8.0

    assert load._set_voltage_from_timer(event)
    assert load.output_current == pytest.approx(4.0)

    event.value = 0.0

    assert load._set_voltage_from_timer(event)
    assert load.output_current == pytest.approx(0.0)
