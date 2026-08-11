from sim.infra.rig import TimerInterface, extend_model_class, load_generated_module
from .extensions import VcrearPytestHelpers
from .simple import VcrearSimpleModel


def _load_generated() -> None:
    if "VcrearModel" in globals():
        return

    model = load_generated_module(
        "VCREAR_MODEL_PY",
        "//sim/models/controllers/vcrear:vcrear-py",
        "vcrear_generated_model",
    )
    enums = load_generated_module(
        "VCREAR_ENUMS_PY",
        "//sim/models/controllers/vcrear:enums-py",
        "vcrear_generated_enums",
    )

    globals()["AnalogInput"] = enums.AnalogInput
    globals()["DigitalIo"] = enums.DigitalIo
    globals()["DigitalOutput"] = enums.DigitalOutput
    globals()["Fault"] = enums.Fault
    globals()["TimerChannel"] = enums.TimerChannel
    globals()["TimerPort"] = enums.TimerPort

    class VcrearModel(extend_model_class(model.VcrearModel, VcrearPytestHelpers)):
        timer = TimerInterface(enums.TimerPort, enums.TimerChannel)

    globals()["VcrearModel"] = VcrearModel


def __getattr__(name: str):
    if name == "PLATFORM_VARIANTS":
        from sim.models.platforms import PLATFORM_VARIANTS

        globals()["PLATFORM_VARIANTS"] = PLATFORM_VARIANTS
        return PLATFORM_VARIANTS
    if name == "VCREAR_CLUSTERS":
        from .variants import VCREAR_CLUSTERS

        globals()["VCREAR_CLUSTERS"] = VCREAR_CLUSTERS
        return VCREAR_CLUSTERS
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
    "VcrearModel",
    "VCREAR_CLUSTERS",
    "PLATFORM_VARIANTS",
}

__all__ = [
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "TimerChannel",
    "TimerPort",
    "VCREAR_CLUSTERS",
    "VcrearSimpleModel",
    "VcrearModel",
    "PLATFORM_VARIANTS",
]
