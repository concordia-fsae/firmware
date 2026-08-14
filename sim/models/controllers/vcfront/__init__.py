from sim.infra.rig import (
    TimerInterface,
    extend_model_class,
    load_generated_enums,
    load_generated_module,
)
from .extensions import VcfrontPytestHelpers
from .simple import VcfrontSimpleModel


def _load_generated() -> None:
    if "VcfrontModel" in globals():
        return

    model = load_generated_module(
        "VCFRONT_MODEL_PY",
        "//sim/models/controllers/vcfront:vcfront-py",
        "vcfront_generated_model",
    )
    enums = _load_generated_enums()

    globals()["AnalogInput"] = enums.AnalogInput
    globals()["DigitalIo"] = enums.DigitalIo
    globals()["DigitalOutput"] = enums.DigitalOutput
    globals()["Fault"] = enums.Fault
    globals()["TimerChannel"] = enums.TimerChannel
    globals()["TimerPort"] = enums.TimerPort
    VcfrontPytestHelpers.AnalogInput = enums.AnalogInput

    class VcfrontModel(extend_model_class(model.VcfrontModel, VcfrontPytestHelpers)):
        timer = TimerInterface(enums.TimerPort, enums.TimerChannel)

    globals()["VcfrontModel"] = VcfrontModel


def _load_generated_enums():
    return load_generated_enums(
        "VCFRONT_ENUMS_PY",
        "//sim/models/controllers/vcfront:enums-py",
        "vcfront_generated_enums",
        globals(),
    )


def __getattr__(name: str):
    if name == "PLATFORM_VARIANTS":
        from sim.models.platforms import PLATFORM_VARIANTS

        globals()["PLATFORM_VARIANTS"] = PLATFORM_VARIANTS
        return PLATFORM_VARIANTS
    if name == "VCFRONT_CLUSTERS":
        from .variants import VCFRONT_CLUSTERS

        globals()["VCFRONT_CLUSTERS"] = VCFRONT_CLUSTERS
        return VCFRONT_CLUSTERS
    _load_generated_enums()
    if name in globals():
        return globals()[name]
    if name in _GENERATED_EXPORTS:
        _load_generated()
        return globals()[name]
    raise AttributeError(name)


_GENERATED_EXPORTS = {
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "TimerChannel",
    "TimerPort",
    "VcfrontModel",
    "VCFRONT_CLUSTERS",
    "PLATFORM_VARIANTS",
}

__all__ = [
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "TimerChannel",
    "TimerPort",
    "VCFRONT_CLUSTERS",
    "VcfrontSimpleModel",
    "VcfrontModel",
    "PLATFORM_VARIANTS",
]
