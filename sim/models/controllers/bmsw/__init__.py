from rig import extend_model_class, load_generated_enums, load_generated_module

from .extensions import BmswModelExtensions
from .segment import BmsSegmentModel, BmsSegmentPort
from .simple import (
    BMSW_WORKER_COUNT_BY_PLATFORM,
    BmswSimpleCluster,
    BmswSimpleModel,
)


def _load_generated() -> None:
    if "BmswModel" in globals():
        return
    model = load_generated_module(
        "BMSW_MODEL_PY",
        "//sim/models/controllers/bmsw:bmsw-py",
        "bmsw_generated_model",
    )
    enums = load_generated_enums(
        "BMSW_ENUMS_PY",
        "//sim/models/controllers/bmsw:enums-py",
        "bmsw_generated_enums",
        globals(),
    )
    for name in ("AnalogInput", "DigitalInput", "DigitalIo", "Fault"):
        globals()[name] = getattr(enums, name)
    class BmswModel(extend_model_class(model.BmswModel, BmswModelExtensions)):
        AnalogInput = enums.AnalogInput
        DigitalInput = enums.DigitalInput
        DigitalIo = enums.DigitalIo

    globals()["BmswModel"] = BmswModel


def __getattr__(name: str):
    if name == "BMSW_CLUSTERS":
        from .variants import BMSW_CLUSTERS

        globals()["BMSW_CLUSTERS"] = BMSW_CLUSTERS
        return BMSW_CLUSTERS
    if name in {"BmswModel", "AnalogInput", "DigitalInput", "DigitalIo", "Fault"}:
        _load_generated()
        return globals()[name]
    raise AttributeError(name)

__all__ = [
    "BMSW_WORKER_COUNT_BY_PLATFORM",
    "BmswSimpleCluster",
    "BmswSimpleModel",
    "BmsSegmentModel",
    "BmsSegmentPort",
    "BmswModel",
    "AnalogInput",
    "DigitalInput",
    "DigitalIo",
    "Fault",
    "BMSW_CLUSTERS",
]
