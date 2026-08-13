import pytest

from sim.infra.rig import DataPath, TimerChannelEvent
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec


@pytest.mark.parametrize(
    "kwargs",
    [
        {},
        {"resistance_ohms": 0.0},
        {"inductance_henrys": 0.0},
        {"capacitance_farads": 0.0},
    ],
)
def test_dc_load_spec_rejects_invalid_or_empty_components(kwargs):
    with pytest.raises(ValueError):
        DcLoadSpec(**kwargs)


def test_dc_load_lrc_spec_updates_current_every_scheduler_step():
    load = DcLoadModel(
        voltage_input_channel=DataPath.component(object(), "voltage"),
        load_spec=DcLoadSpec(
            resistance_ohms=2.0,
            inductance_henrys=4.0,
            capacitance_farads=0.001,
        ),
    )
    event = TimerChannelEvent()
    event.value = 8.0

    assert load._set_voltage_from_timer(event)
    load.run_for(1)

    assert load.output_current == pytest.approx(12.002)

    load.run_for(1)

    assert load.output_current == pytest.approx(4.004)
