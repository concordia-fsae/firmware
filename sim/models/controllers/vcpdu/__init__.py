from sim.infra.rig import SpiInterface, TimerInterface, extend_model_class, load_generated_module

from .extensions import VcpduModelExtensions

_model = load_generated_module(
    "VCPDU_MODEL_PY",
    "//sim/models/controllers/vcpdu:vcpdu-py",
    "vcpdu_generated_model",
)
_enums = load_generated_module(
    "VCPDU_ENUMS_PY",
    "//sim/models/controllers/vcpdu:enums-py",
    "vcpdu_generated_enums",
)

AnalogInput = _enums.AnalogInput
DigitalIo = _enums.DigitalIo
DigitalOutput = _enums.DigitalOutput
Fault = _enums.Fault
SpiDevice = _enums.SpiDevice
TimerChannel = _enums.TimerChannel
TimerPort = _enums.TimerPort
Vn9008Channel = _enums.Vn9008Channel
VcpduModelExtensions.AnalogInput = AnalogInput
VcpduModelExtensions.Vn9008Channel = Vn9008Channel


class VcpduModel(extend_model_class(_model.VcpduModel, VcpduModelExtensions)):
    timer = TimerInterface(TimerPort, TimerChannel)
    spi = SpiInterface(SpiDevice)

from sim.models.platforms import PLATFORM_VARIANTS

from .variants import VCPDU_CLUSTERS

__all__ = [
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "SpiDevice",
    "TimerChannel",
    "TimerPort",
    "Vn9008Channel",
    "PLATFORM_VARIANTS",
    "VCPDU_CLUSTERS",
    "VcpduModel",
]
