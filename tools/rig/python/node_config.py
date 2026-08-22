"""Generic node configuration shared by Python and native Rig nodes."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field

from .contracts import Interface
from .datapath import DataPath


@dataclass(frozen=True)
class SchedulerConfig:
    """Scheduler information supplied to a Rig node."""

    period_ns: int | None = None
    callback: Callable | None = None

    def __post_init__(self) -> None:
        if self.period_ns is not None and self.period_ns <= 0:
            raise ValueError(f"scheduler period must be positive, got {self.period_ns}")


@dataclass(frozen=True)
class DataflowConfig:
    """Declared dataflow surface for a node."""

    inputs: tuple[DataPath, ...] = ()
    outputs: tuple[DataPath, ...] = ()


@dataclass(frozen=True)
class NodeConfig:
    """Generic configuration passed to any Rig node implementation."""

    scheduler: SchedulerConfig = field(default_factory=SchedulerConfig)
    dataflow: DataflowConfig = field(default_factory=DataflowConfig)
    interfaces: tuple[Interface, ...] = ()


__all__ = ["DataflowConfig", "NodeConfig", "SchedulerConfig"]
