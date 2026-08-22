"""Public contracts shared by simulation infrastructure and model implementations.

These protocols are the dependency boundary for the rig. Infrastructure may
orchestrate an object through one of these contracts, but must not depend on a
concrete model or backend implementation.
"""

from __future__ import annotations

from collections.abc import Mapping
from typing import Generic, Protocol, TypeVar, runtime_checkable

from .datapath import DataPath, DataPathLink, ModelDataPaths


OwnerT = TypeVar("OwnerT", bound="Node")
NodeT = TypeVar("NodeT", bound="Node")


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


@runtime_checkable
class RigElement(Node, Protocol):
    """Typed element that can be attached to a generic Rig container."""

    _cluster_node_name: str | None

    def attach_to(self, rig: Cluster, name: str) -> None: ...

    def detach_from(self) -> None: ...

    def reset(self) -> None: ...


class RigInterface(Generic[OwnerT], Protocol):
    """Typed model-facing interface implemented by a backend binding."""

    def attach(self, owner: OwnerT) -> None: ...

    def supports(self, path: DataPath) -> bool: ...


class Model(Node, Protocol):
    """A Rig-owned model boundary, implemented by Python or native code."""

    def is_online(self) -> bool: ...


class Component(Node, Protocol, Generic[OwnerT]):
    """A node-owned submodel whose interfaces are bound by its owner."""

    def configure_owner(self, owner: OwnerT) -> None: ...


class RigRuntime(Protocol):
    """Backend-neutral runtime owned by a :class:`Cluster`.

    Firmware bindings may extend this contract with peripheral operations, but
    lifecycle, scalar dataflow, scheduling, waits, and clock ownership remain
    generic Rig responsibilities.
    """

    def bind_symbol(
        self,
        name: str,
        argtypes: list[object] | None = None,
        restype: object | None = None,
    ): ...

    def reset(self) -> None: ...

    def add_node(self, name: str, node, *, online: bool = True) -> None: ...

    def add_scalar_route(self, **kwargs) -> bool: ...

    def add_scalar_sink_route(self, **kwargs) -> bool: ...

    def add_scalar_input_route(self, **kwargs) -> bool: ...

    def add_scalar_state_route(self, **kwargs) -> bool: ...

    def add_scalar_state_sink(self, **kwargs) -> bool: ...

    def add_scalar_transform_algorithm(self, **kwargs) -> bool: ...

    def compile_dataflow_graph(self) -> bool: ...

    def run_for(
        self, duration_ns: int, step_ns: int, *, route: bool = True
    ) -> None: ...

    def set_node_online(self, name: str, online: bool) -> None: ...

    def node_index(self, node: str) -> int | None: ...

    def elapsed_ns(self) -> int: ...

    def node_elapsed_ns(self, name: str) -> int: ...

    def node_elapsed_ns_values(self) -> dict[str, int]: ...

    def run_until_dataflow_wait(
        self,
        handle: int,
        *,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None: ...

    def cancel_dataflow_wait(self, handle: int) -> None: ...


class Cluster(Scheduler, Protocol):
    """Generic Rig container coordinating nodes and dataflow edges."""

    @property
    def nodes(self) -> Mapping[str, Model]: ...

    @property
    def runtime(self) -> RigRuntime | None: ...

    def run_until(
        self,
        predicate,
        *,
        timeout: int | float,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
        message: str | None = None,
    ) -> int: ...

    def node_online(self, name: str) -> bool: ...


class Interface(Protocol):
    """Model-facing namespace that creates and validates datapaths."""

    def supports(self, path: DataPath) -> bool: ...


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
    "Cluster",
    "Component",
    "Dataflow",
    "Edge",
    "Interface",
    "Model",
    "Node",
    "RigElement",
    "RigInterface",
    "RigRuntime",
    "Scheduler",
]
