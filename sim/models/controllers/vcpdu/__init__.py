from sim.infra.rig import (
    SpiInterface,
    TimerInterface,
    extend_model_class,
    load_generated_enums,
    load_generated_module,
)

from .extensions import VcpduModelExtensions
from .simple import VcpduSimpleModel


def _load_generated() -> None:
    if "VcpduModel" in globals():
        return

    model = load_generated_module(
        "VCPDU_MODEL_PY",
        "//sim/models/controllers/vcpdu:vcpdu-py",
        "vcpdu_generated_model",
    )
    enums = _load_generated_enums()

    globals()["AnalogInput"] = enums.AnalogInput
    globals()["DigitalIo"] = enums.DigitalIo
    globals()["DigitalOutput"] = enums.DigitalOutput
    globals()["Fault"] = enums.Fault
    globals()["SpiDevice"] = enums.SpiDevice
    globals()["TimerChannel"] = enums.TimerChannel
    globals()["TimerPort"] = enums.TimerPort
    globals()["Tps2hb16abIc"] = enums.Tps2hb16abIc
    globals()["Tps2hb16abOutput"] = enums.Tps2hb16abOutput
    globals()["Vn9008Channel"] = enums.Vn9008Channel
    VcpduModelExtensions.AnalogInput = enums.AnalogInput
    VcpduModelExtensions.DigitalIo = enums.DigitalIo
    VcpduModelExtensions.SpiDevice = enums.SpiDevice
    VcpduModelExtensions.Tps2hb16abIc = enums.Tps2hb16abIc
    VcpduModelExtensions.Tps2hb16abOutput = enums.Tps2hb16abOutput
    VcpduModelExtensions.Vn9008Channel = enums.Vn9008Channel

    class VcpduModel(extend_model_class(model.VcpduModel, VcpduModelExtensions)):
        timer = TimerInterface(enums.TimerPort, enums.TimerChannel)
        spi = SpiInterface(enums.SpiDevice)

    globals()["VcpduModel"] = VcpduModel


def _load_generated_enums():
    return load_generated_enums(
        "VCPDU_ENUMS_PY",
        "//sim/models/controllers/vcpdu:enums-py",
        "vcpdu_generated_enums",
        globals(),
    )


def __getattr__(name: str):
    if name == "PLATFORM_VARIANTS":
        from sim.models.platforms import PLATFORM_VARIANTS

        globals()["PLATFORM_VARIANTS"] = PLATFORM_VARIANTS
        return PLATFORM_VARIANTS
    if name == "VCPDU_CLUSTERS":
        from .variants import VCPDU_CLUSTERS

        globals()["VCPDU_CLUSTERS"] = VCPDU_CLUSTERS
        return VCPDU_CLUSTERS
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
    "SpiDevice",
    "TimerChannel",
    "TimerPort",
    "Tps2hb16abIc",
    "Tps2hb16abOutput",
    "Vn9008Channel",
    "VcpduModel",
    "VCPDU_CLUSTERS",
    "PLATFORM_VARIANTS",
}

__all__ = [
    "AnalogInput",
    "DigitalIo",
    "DigitalOutput",
    "Fault",
    "SpiDevice",
    "TimerChannel",
    "TimerPort",
    "Tps2hb16abIc",
    "Tps2hb16abOutput",
    "Vn9008Channel",
    "PLATFORM_VARIANTS",
    "VCPDU_CLUSTERS",
    "VcpduSimpleModel",
    "VcpduModel",
]
