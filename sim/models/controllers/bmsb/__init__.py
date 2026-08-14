from sim.infra.rig import (
    TimerInterface,
    extend_model_class,
    load_generated_enums,
    load_generated_module,
)

from .extensions import BmsbModelExtensions
from .simple import BmsbSimpleModel


def _load_generated() -> None:
    if "BmsbModel" in globals():
        return

    model = load_generated_module(
        "BMSB_MODEL_PY",
        "//sim/models/controllers/bmsb:bmsb-py",
        "bmsb_generated_model",
    )
    enums = _load_generated_enums()

    globals()["AnalogInput"] = enums.AnalogInput
    globals()["DigitalInput"] = enums.DigitalInput
    globals()["DigitalIo"] = enums.DigitalIo
    globals()["DigitalOutput"] = enums.DigitalOutput
    globals()["Fault"] = enums.Fault
    globals()["TimerChannel"] = enums.TimerChannel
    globals()["TimerPort"] = enums.TimerPort

    class BmsbModel(extend_model_class(model.BmsbModel, BmsbModelExtensions)):
        AnalogInput = enums.AnalogInput
        DigitalInput = enums.DigitalInput
        timer = TimerInterface(enums.TimerPort, enums.TimerChannel)

    globals()["BmsbModel"] = BmsbModel


def _load_generated_enums():
    return load_generated_enums(
        "BMSB_ENUMS_PY",
        "//sim/models/controllers/bmsb:enums-py",
        "bmsb_generated_enums",
        globals(),
    )


def __getattr__(name: str):
    if name == "PLATFORM_VARIANTS":
        from sim.models.platforms import PLATFORM_VARIANTS

        globals()["PLATFORM_VARIANTS"] = PLATFORM_VARIANTS
        return PLATFORM_VARIANTS
    if name == "BMSB_CLUSTERS":
        from .variants import BMSB_CLUSTERS

        globals()["BMSB_CLUSTERS"] = BMSB_CLUSTERS
        return BMSB_CLUSTERS
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
    "Fault",
    "TimerChannel",
    "TimerPort",
    "BmsbModel",
    "BMSB_CLUSTERS",
    "PLATFORM_VARIANTS",
}

__all__ = [
    "AnalogInput",
    "DigitalInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "TimerChannel",
    "TimerPort",
    "PLATFORM_VARIANTS",
    "BMSB_CLUSTERS",
    "BmsbSimpleModel",
    "BmsbModel",
]
