"""Public contracts shared by simulation infrastructure and model implementations.

These protocols are the dependency boundary for the rig.  Infrastructure may
orchestrate an object through one of these contracts, but must not depend on a
concrete model or peripheral implementation.
"""

from __future__ import annotations

from typing import Protocol, TypeVar

from .datapath import DataPath, DataPathLink, ModelDataPaths


PayloadT = TypeVar("PayloadT")


class Scheduler(Protocol):
    """Owner of time progression for one schedulable item."""

    elapsed_ns: int

    def reset(self) -> None: ...

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None: ...


class Dataflow(Protocol):
    """Typed ingress/egress registration owned by a model or component."""

    @property
    def datapaths(self) -> ModelDataPaths: ...

    def configure_datapath(self, path: DataPath) -> None: ...

    def supports_datapath(self, path: DataPath) -> bool: ...


class Node(Scheduler, Dataflow, Protocol):
    """A schedulable model boundary that can participate in a cluster."""

    def set_online(self, online: bool) -> None: ...


class Component(Node, Protocol):
    """A node-owned submodel whose interfaces are bound by its owner."""

    def configure_owner(self, owner: Node) -> None: ...


class Interface(Protocol):
    """Model-facing namespace that creates and validates datapaths."""

    def supports(self, path: DataPath) -> bool: ...


class Peripheral(Interface, Protocol):
    """A peripheral implementation owned by the model exposing it."""

    def send_payload(self, path: DataPath, payload: PayloadT) -> bool: ...

    def recv(self, path: DataPath) -> PayloadT | None: ...


class Algorithm(Protocol):
    """A schedulable dataflow operation owned by its registering item."""

    owner_node: int
    sort_key: tuple[int, int, int]

    def run(self) -> bool: ...


class Edge(Protocol):
    """A named connection between an output and an optional input owner."""

    @property
    def link(self) -> DataPathLink: ...

    def connect(self, path: DataPath) -> None: ...


__all__ = [
    "Algorithm",
    "Component",
    "Dataflow",
    "Edge",
    "Interface",
    "Node",
    "Peripheral",
    "Scheduler",
]
