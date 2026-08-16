"""Public Python interface for the firmware runtime binding.

The modules in this package are implementation details. Firmware models and
tests should import runtime adapters from this module so the package remains a
stable boundary when the implementation is reorganized.

Exports are loaded lazily because peripheral bindings are also imported by
the timer and SPI modules. This keeps the public façade independent of module
initialization order while retaining normal ``from ... import Symbol`` usage.
"""

from importlib import import_module
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from .cluster import (
        ClusterCanComms,
        ClusterComms,
        ClusterDataRoutes,
        ClusterSpiComms,
        ClusterTimerComms,
        FirmwareClusterRig,
    )
    from .node import FirmwareNodeRig
    from .runtime import FirmwareRuntime
    from .peripheral import (
        PeripheralBinding,
        PeripheralInterface,
        peripheral_datapath,
        require_peripheral_binding,
    )

_EXPORTS = {
    "ClusterCanComms": ("cluster", "ClusterCanComms"),
    "ClusterComms": ("cluster", "ClusterComms"),
    "ClusterDataRoutes": ("cluster", "ClusterDataRoutes"),
    "ClusterSpiComms": ("cluster", "ClusterSpiComms"),
    "ClusterTimerComms": ("cluster", "ClusterTimerComms"),
    "FirmwareClusterRig": ("cluster", "FirmwareClusterRig"),
    "FirmwareNodeRig": ("node", "FirmwareNodeRig"),
    "FirmwareRuntime": ("runtime", "FirmwareRuntime"),
    "PeripheralBinding": ("peripheral", "PeripheralBinding"),
    "PeripheralInterface": ("peripheral", "PeripheralInterface"),
    "peripheral_datapath": ("peripheral", "peripheral_datapath"),
    "require_peripheral_binding": ("peripheral", "require_peripheral_binding"),
}


def __getattr__(name: str) -> Any:
    try:
        module_name, attribute_name = _EXPORTS[name]
    except KeyError as error:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}") from error
    value = getattr(import_module(f"{__name__}.{module_name}"), attribute_name)
    globals()[name] = value
    return value

__all__ = [
    "ClusterCanComms",
    "ClusterComms",
    "ClusterDataRoutes",
    "ClusterSpiComms",
    "ClusterTimerComms",
    "FirmwareClusterRig",
    "FirmwareNodeRig",
    "FirmwareRuntime",
    "PeripheralBinding",
    "PeripheralInterface",
    "peripheral_datapath",
    "require_peripheral_binding",
]
