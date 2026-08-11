from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass

from .datapath import DataPath


@dataclass(frozen=True)
class PowerControlEvent:
    enabled: bool


@dataclass(frozen=True)
class PowerControlPath:
    path: DataPath
    connect: Callable[[object], None]


class PowerInterface:
    @staticmethod
    def _control_datapath(source: object) -> DataPath:
        return DataPath(("power", source, "control"))

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
