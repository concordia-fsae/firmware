from sim.infra.rig import TimerInterface, extend_model_class, load_generated_module

from .extensions import VcfrontPytestHelpers

_model = load_generated_module(
    "VCFRONT_MODEL_PY",
    "//sim/models/controllers/vcfront:vcfront-py",
    "vcfront_generated_model",
)
_enums = load_generated_module(
    "VCFRONT_ENUMS_PY",
    "//sim/models/controllers/vcfront:enums-py",
    "vcfront_generated_enums",
)

AnalogInput = _enums.AnalogInput
DigitalIo = _enums.DigitalIo
DigitalOutput = _enums.DigitalOutput
Fault = _enums.Fault
TimerChannel = _enums.TimerChannel
TimerPort = _enums.TimerPort
VcfrontPytestHelpers.AnalogInput = AnalogInput


class VcfrontModel(extend_model_class(_model.VcfrontModel, VcfrontPytestHelpers)):
    timer = TimerInterface(TimerPort, TimerChannel)


from sim.models.platforms import PLATFORM_VARIANTS

from .variants import VCFRONT_CLUSTERS

__all__ = [
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "TimerChannel",
    "TimerPort",
    "VCFRONT_CLUSTERS",
    "VcfrontModel",
    "PLATFORM_VARIANTS",
]
