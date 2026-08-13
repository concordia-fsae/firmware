from __future__ import annotations

import os
from dataclasses import dataclass, field
from enum import Enum
from typing import Protocol

from .datapath import ComponentDataPathBinding, DataPathLink
from .cluster import ClusterRig
from .model import ComponentRig
from .node import NodeRig


class ClusterSpecFactory(Protocol):
    name: str

    def rig(self) -> ClusterRig: ...


class RigNodeSpec(Protocol):
    name: object
    components: tuple[ComponentSpec, ...]

    def rig(self) -> object: ...


@dataclass(frozen=True)
class NodeSpec:
    name: object
    rig_class: type[NodeRig]
    hardware: str | None = None
    components: tuple[ComponentSpec, ...] = ()

    def rig(self) -> NodeRig:
        return self.rig_class(self.library_path())

    def library_path(self) -> str | None:
        env_prefix = rig_node_name(self.name).upper()
        if self.hardware is not None:
            hardware_env = f"{env_prefix}_{self.hardware.upper()}_SIM_LIB"
            library_path = os.environ.get(hardware_env)
            if library_path is not None:
                return library_path
            raise RuntimeError(
                f"missing {hardware_env} for {self.name} {self.hardware} model"
            )
        return os.environ.get(f"{env_prefix}_SIM_LIB")


@dataclass(frozen=True)
class ComponentSpec:
    rig_class: type[ComponentRig]
    parameters: dict[str, object] = field(default_factory=dict)
    bindings: tuple[ComponentDataPathBinding, ...] = ()

    def rig(self) -> ComponentRig:
        return self.rig_class(**self.parameters)


@dataclass(frozen=True)
class ClusterSpec:
    name: str
    nodes: tuple[RigNodeSpec, ...]
    hardware: str | None = None
    features: frozenset[str] = frozenset()
    datapath_links: tuple[DataPathLink, ...] = ()

    def rig(self) -> ClusterRig:
        nodes = {}
        components: list[tuple[str, ComponentSpec, ComponentRig]] = []
        for node in self.nodes:
            node_name = rig_node_name(node.name)
            nodes[node_name] = node.rig()
            components.extend(
                (node_name, component, component.rig()) for component in node.components
            )

        rig = ClusterRig(
            name=self.name,
            hardware=self.hardware,
            features=self.features,
            components=tuple(component_rig for _, _, component_rig in components),
            **nodes,
        )
        for owner_node, component, component_rig in components:
            owner = rig.nodes[owner_node]
            owner.configure_model_outputs_for(component_rig)
            for binding in component.bindings:
                binding.bind(owner, component_rig)
        rig.comm.connect_node_interfaces()
        for link in self.datapath_links:
            rig.dataroutes.connect(
                link.path,
                source_node=link.source_node,
                sink_node=link.sink_node,
            )
        rig.reset()
        return rig

    def has_feature(self, feature: str) -> bool:
        return feature in self.features


def rig_node_name(name: object) -> str:
    if isinstance(name, Enum):
        return name.name.lower()
    return str(name)


class ClusterCatalog:
    def __init__(self, *clusters: ClusterSpecFactory) -> None:
        if not clusters:
            raise ValueError("ClusterCatalog requires at least one cluster")
        self.clusters = tuple(clusters)
        self._clusters_by_name = {cluster.name: cluster for cluster in self.clusters}
        if len(self._clusters_by_name) != len(self.clusters):
            raise ValueError("ClusterCatalog cluster names must be unique")

    @property
    def names(self) -> tuple[str, ...]:
        return tuple(cluster.name for cluster in self.clusters)

    def get(self, name: str) -> ClusterSpecFactory:
        try:
            return self._clusters_by_name[name]
        except KeyError as exc:
            raise KeyError(
                f"cluster {name!r} is not declared; expected one of {', '.join(self.names)}"
            ) from exc

    def selected(
        self,
        env_var: str = "SIM_CLUSTERS",
    ) -> tuple[ClusterSpecFactory, ...]:
        raw_names = os.environ.get(env_var)
        if raw_names is None:
            return self.clusters
        names = tuple(name.strip() for name in raw_names.split(",") if name.strip())
        return tuple(self.get(name) for name in names) if names else self.clusters

    def pytest_cases(
        self,
        *,
        cluster_env_var: str = "SIM_CLUSTERS",
    ) -> tuple[ClusterSpecFactory, ...]:
        return self.selected(cluster_env_var)
