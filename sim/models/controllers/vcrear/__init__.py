from sim.infra.rig import TimerInterface, extend_model_class, load_generated_module

from .extensions import VcrearPytestHelpers

_model = load_generated_module(
    "VCREAR_MODEL_PY",
    "//sim/models/controllers/vcrear:vcrear-py",
    "vcrear_generated_model",
)
_enums = load_generated_module(
    "VCREAR_ENUMS_PY",
    "//sim/models/controllers/vcrear:enums-py",
    "vcrear_generated_enums",
)

AnalogInput = _enums.AnalogInput
DigitalIo = _enums.DigitalIo
DigitalOutput = _enums.DigitalOutput
Fault = _enums.Fault
TimerChannel = _enums.TimerChannel
TimerPort = _enums.TimerPort


class VcrearModel(extend_model_class(_model.VcrearModel, VcrearPytestHelpers)):
    timer = TimerInterface(TimerPort, TimerChannel)


from sim.models.platforms import PLATFORM_VARIANTS

from .variants import VCREAR_CLUSTERS

__all__ = [
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "TimerChannel",
    "TimerPort",
    "VCREAR_CLUSTERS",
    "VcrearModel",
    "PLATFORM_VARIANTS",
]
