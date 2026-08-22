"""Generic cluster configuration shared by Rig cluster implementations."""

from __future__ import annotations

from dataclasses import dataclass, field

from .contracts import Interface
from .node_config import DataflowConfig, SchedulerConfig


@dataclass(frozen=True)
class ClusterConfig:
    """Generic scheduler, dataflow, and interface configuration for a cluster."""

    scheduler: SchedulerConfig = field(default_factory=SchedulerConfig)
    dataflow: DataflowConfig = field(default_factory=DataflowConfig)
    interfaces: tuple[Interface, ...] = ()


__all__ = ["ClusterConfig"]
