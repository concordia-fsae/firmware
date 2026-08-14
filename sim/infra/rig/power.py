from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from enum import Enum, auto

from .datapath import DataPath
from .dataflow import DataflowEvent


class PowerDataPath(Enum):
    POWER = auto()
    CONTROL = auto()


@dataclass(frozen=True)
class PowerControlEvent(DataflowEvent):
    enabled: bool
    timestamp_ns: int = 0


@dataclass(frozen=True)
class PowerControlPath:
    path: DataPath
    connect: Callable[[object], None]


class PowerInterface:
    @staticmethod
    def _control_datapath(source: object) -> DataPath:
        return DataPath.named(PowerDataPath.POWER, source, PowerDataPath.CONTROL)

    @staticmethod
    def connect_node_input(node: object, path: PowerControlPath) -> None:
        node.datapaths.add_input(
            path.path,
            send=lambda event: PowerInterface._set_node_power(node, event),
        )

    @staticmethod
    def _set_node_power(node: object, event: object) -> bool:
        if not isinstance(event, PowerControlEvent):
            raise TypeError(
                f"power control datapaths require PowerControlEvent payloads, got {type(event).__name__}"
            )
        node.set_online(event.enabled)
        return True
