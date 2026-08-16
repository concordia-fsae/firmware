from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

from rig.datapath import DataPath


class PeripheralInterface(IntEnum):
    TIMER_DUTY = 1
    TIMER_FREQUENCY = 2
    SPI_TRANSACTION = 3
    TIMER_CAPTURE = 4


@dataclass(frozen=True)
class PeripheralBinding:
    interface: PeripheralInterface
    channel: int | None = None
    port: int | None = None
    device: int | None = None


def peripheral_datapath(path: DataPath, binding: PeripheralBinding) -> DataPath:
    return DataPath(path.parts, metadata=binding, key=path.key)


def require_peripheral_binding(path: DataPath) -> PeripheralBinding:
    binding = path.metadata
    if binding is None:
        raise ValueError(f"datapath {path!r} is not backed by a controller peripheral")
    return binding
