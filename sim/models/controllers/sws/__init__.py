from sim.infra.rig import (
    TimerInterface,
    extend_model_class,
    load_generated_enums,
    load_generated_module,
)

from .simple import SwsSimpleModel


def _load_generated() -> None:
    if "SwsModel" in globals():
        return

    model = load_generated_module(
        "SWS_MODEL_PY",
        "//sim/models/controllers/sws:sws-py",
        "sws_generated_model",
    )
    enums = _load_generated_enums()

    globals()["AnalogInput"] = enums.AnalogInput
    globals()["DigitalInput"] = enums.DigitalInput
    globals()["DigitalIo"] = enums.DigitalIo
    globals()["DigitalOutput"] = enums.DigitalOutput
    globals()["TimerChannel"] = enums.TimerChannel
    globals()["TimerPort"] = enums.TimerPort

    class SwsModel(extend_model_class(model.SwsModel)):
        timer = TimerInterface(enums.TimerPort, enums.TimerChannel)

    globals()["SwsModel"] = SwsModel


def _load_generated_enums():
    return load_generated_enums(
        "SWS_ENUMS_PY",
        "//sim/models/controllers/sws:enums-py",
        "sws_generated_enums",
        globals(),
    )


def __getattr__(name: str):
    if name == "PLATFORM_VARIANTS":
        from sim.models.platforms import PLATFORM_VARIANTS

        globals()["PLATFORM_VARIANTS"] = PLATFORM_VARIANTS
        return PLATFORM_VARIANTS
    if name == "SWS_CLUSTERS":
        from .variants import SWS_CLUSTERS

        globals()["SWS_CLUSTERS"] = SWS_CLUSTERS
        return SWS_CLUSTERS
    _load_generated_enums()
    if name in globals():
        return globals()[name]
    if name in _GENERATED_EXPORTS:
        _load_generated()
        return globals()[name]
    raise AttributeError(name)


_GENERATED_EXPORTS = {
    "AnalogInput",
    "DigitalInput",
    "DigitalIo",
    "DigitalOutput",
    "TimerChannel",
    "TimerPort",
    "SwsModel",
    "SWS_CLUSTERS",
    "PLATFORM_VARIANTS",
}

__all__ = [
    "AnalogInput",
    "DigitalInput",
    "DigitalIo",
    "DigitalOutput",
    "TimerChannel",
    "TimerPort",
    "PLATFORM_VARIANTS",
    "SWS_CLUSTERS",
    "SwsSimpleModel",
    "SwsModel",
]
