"""Generic Rig cluster composition and Python dataflow routing."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Generic, TypeVar

from .cluster_config import ClusterConfig
from .contracts import RigElement, RigRuntime
from .datapath import (
    DataPath,
    DataPathKey,
    DataPathLink,
    DataPathRecord,
    DataPathSink,
    DataPathSource,
    FanoutDataPath,
    datapath_key,
)
from .model import ComponentRig, ModelRig
from .time import duration_to_ns, run_until


NodeT = TypeVar("NodeT", bound=ModelRig)


@dataclass(frozen=True)
class _Route:
    source_node: str
    source: DataPathSource[object]
    sinks: tuple[DataPathSink[object], ...]


class DataflowRoutes:
    """Generic Python dataflow edges owned by a :class:`ClusterRig`.

    Backend runtimes may provide a faster native route implementation, but a
    cluster always has this explicit edge model as its portable baseline.
    """

    def __init__(self, cluster: ClusterRig) -> None:
        self._cluster = cluster
        self._fanouts: dict[DataPathKey, FanoutDataPath[object]] = {}
        self._paths: dict[DataPathKey, DataPath] = {}
        self._links: list[DataPathLink] = []
        self._route_cache: dict[DataPathKey, tuple[_Route, ...]] = {}
        self._latest_records: dict[tuple[str, DataPathKey], DataPathRecord[object]] = {}
        self._ordered_paths_cache: tuple[DataPath, ...] | None = None

    @property
    def paths(self) -> tuple[DataPath, ...]:
        return tuple(self._paths.values())

    def register(self, path: DataPath) -> None:
        self._fanout(path)

    def connect(
        self,
        path: DataPath,
        *,
        source_node: str,
        sink_node: str | None = None,
    ) -> None:
        self._require_node(source_node)
        if sink_node is not None:
            self._require_node(sink_node)
        link = DataPathLink(path, source_node, sink_node)
        if link not in self._links:
            self._ensure_node_graph_is_acyclic((*self._links, link))
            self._links.append(link)
            self._route_cache.clear()
            self._ordered_paths_cache = None
        self._fanout(path)

    def connect_available_paths(self, *, exclude=None) -> None:
        for source_node, node in self._cluster._rig_nodes.items():
            for output in node.datapaths.outputs():
                if exclude is not None and exclude(output.path):
                    continue
                sink_nodes = tuple(
                    sink_node
                    for sink_node, sink in self._cluster._rig_nodes.items()
                    if sink_node != source_node and sink.datapaths.inputs(output.path)
                )
                if sink_nodes:
                    for sink_node in sink_nodes:
                        self.connect(
                            output.path,
                            source_node=source_node,
                            sink_node=sink_node,
                        )
                else:
                    self.connect(output.path, source_node=source_node)

    def route(self) -> None:
        """Drain ready edges until all same-step dependent edges settle."""
        paths = self._ordered_paths()
        while True:
            progressed = False
            for path in paths:
                progressed = self._route_path(path) or progressed
            if not progressed:
                return

    def _route(self, path: DataPath | None = None) -> None:
        paths = (path,) if path is not None else self._ordered_paths()
        for path_name in paths:
            self._route_path(path_name)

    def reset(self) -> None:
        for fanout in self._fanouts.values():
            fanout.clear()
        self._latest_records.clear()
        self._route_cache.clear()

    def clear(self, path: DataPath) -> None:
        self._fanout(path).clear()
        key = datapath_key(path)
        self._latest_records = {
            record_key: record
            for record_key, record in self._latest_records.items()
            if record_key[1] != key
        }

    def records(self, path: DataPath) -> tuple[DataPathRecord[object], ...]:
        return tuple(self._fanout(path).records)

    def reversed_records(self, path: DataPath):
        return reversed(self._fanout(path).records)

    def latest_record(
        self,
        path: DataPath,
        *,
        source_node: str | None = None,
    ) -> DataPathRecord[object] | None:
        key = datapath_key(path)
        if source_node is not None:
            return self._latest_records.get((source_node, key))
        records = [
            record
            for (record_source, record_key), record in self._latest_records.items()
            if record_key == key and record_source == record.source
        ]
        return max(records, key=lambda record: record.timestamp_ns, default=None)

    def has_python_routes(self) -> bool:
        return bool(self._links)

    def _route_path(self, path: DataPath) -> bool:
        progressed = False
        fanout = self._fanout(path)
        for route in self._routes_for_path(path):
            if not self._cluster.node_online(route.source_node):
                continue
            while route.source.pending():
                pending = route.source.pending()
                payloads = (
                    route.source.recv_many(pending)
                    if route.source.recv_many is not None
                    else self._single_payload(route.source.recv())
                )
                if not payloads:
                    break
                progressed = True
                for payload in payloads:
                    record = DataPathRecord(
                        route.source.node,
                        route.source.path,
                        payload,
                        self._cluster.elapsed_ns,
                    )
                    fanout.records.append(record)
                    self._latest_records[(route.source_node, datapath_key(path))] = (
                        record
                    )
                for sink in route.sinks:
                    self._send(sink, payloads)
        return progressed

    def _routes_for_path(self, path: DataPath) -> tuple[_Route, ...]:
        links_by_source: dict[str, list[DataPathLink]] = {}
        path_key = datapath_key(path)
        if path_key in self._route_cache:
            return self._route_cache[path_key]

        for link in self._links:
            if datapath_key(link.path) == path_key:
                links_by_source.setdefault(link.source_node, []).append(link)

        routes: list[_Route] = []
        for source_node, links in links_by_source.items():
            node = self._cluster._rig_nodes[source_node]
            outputs = node.datapaths.outputs(links[0].path)
            if not outputs:
                raise KeyError(
                    f"node {source_node!r} has no output for datapath {links[0].path!r}"
                )
            output = outputs[0]
            sinks: list[DataPathSink[object]] = []
            for link in links:
                if link.sink_node is None:
                    continue
                sink_node = self._cluster._rig_nodes[link.sink_node]
                inputs = sink_node.datapaths.inputs(link.path)
                if not inputs:
                    raise KeyError(
                        f"node {link.sink_node!r} has no input for datapath {link.path!r}"
                    )
                sinks.extend(
                    DataPathSink(
                        link.sink_node,
                        input_.path,
                        input_.send,
                        input_.send_many,
                    )
                    for input_ in inputs
                )
            routes.append(
                _Route(
                    source_node,
                    DataPathSource(
                        source_node,
                        output.path,
                        output.pending,
                        output.recv,
                        output.recv_many,
                    ),
                    tuple(sinks),
                )
            )
        self._route_cache[path_key] = tuple(routes)
        return self._route_cache[path_key]

    @staticmethod
    def _single_payload(payload: object | None) -> tuple[object, ...]:
        return () if payload is None else (payload,)

    @staticmethod
    def _send(sink: DataPathSink[object], payloads: tuple[object, ...]) -> None:
        if sink.send_many is not None:
            accepted = sink.send_many(payloads)
            if accepted != len(payloads):
                raise RuntimeError(
                    f"datapath sink {sink.node!r} accepted {accepted} of {len(payloads)} payloads"
                )
            return
        for payload in payloads:
            sink.send(payload)

    def _fanout(self, path: DataPath) -> FanoutDataPath[object]:
        key = datapath_key(path)
        if key not in self._paths:
            self._paths[key] = path
            self._ordered_paths_cache = None
        return self._fanouts.setdefault(key, FanoutDataPath())

    def _ordered_paths(self) -> tuple[DataPath, ...]:
        if self._ordered_paths_cache is not None:
            return self._ordered_paths_cache

        path_by_key = {datapath_key(path): path for path in self.paths}
        source_nodes_by_key: dict[DataPathKey, set[str]] = {}
        sink_nodes_by_key: dict[DataPathKey, set[str]] = {}
        for path in self.paths:
            key = datapath_key(path)
            for node_name, node in self._cluster._rig_nodes.items():
                if node.datapaths.outputs(path):
                    source_nodes_by_key.setdefault(key, set()).add(node_name)
                if node.datapaths.inputs(path):
                    sink_nodes_by_key.setdefault(key, set()).add(node_name)

        dependencies: dict[DataPathKey, set[DataPathKey]] = {
            key: set() for key in path_by_key
        }
        dependents: dict[DataPathKey, set[DataPathKey]] = {
            key: set() for key in path_by_key
        }
        for before_key, sink_nodes in sink_nodes_by_key.items():
            for after_key, source_nodes in source_nodes_by_key.items():
                if before_key == after_key or sink_nodes.isdisjoint(source_nodes):
                    continue
                dependencies[after_key].add(before_key)
                dependents[before_key].add(after_key)

        ready = [key for key in path_by_key if not dependencies[key]]
        queued = set(ready)
        ordered: list[DataPathKey] = []
        while ready:
            key = ready.pop(0)
            queued.discard(key)
            if key in ordered:
                continue
            ordered.append(key)
            for dependent in sorted(dependents[key], key=repr):
                dependencies[dependent].discard(key)
                if (
                    not dependencies[dependent]
                    and dependent not in ordered
                    and dependent not in queued
                ):
                    ready.append(dependent)
                    queued.add(dependent)

        if len(ordered) != len(path_by_key):
            cyclic = ", ".join(repr(key) for key, deps in dependencies.items() if deps)
            raise ValueError(f"datapath route graph contains a cycle: {cyclic}")

        self._ordered_paths_cache = tuple(path_by_key[key] for key in ordered)
        return self._ordered_paths_cache

    def _require_node(self, name: str) -> None:
        if name not in self._cluster._rig_nodes:
            raise KeyError(f"node {name!r} is not in this rig")

    @staticmethod
    def _ensure_node_graph_is_acyclic(links: tuple[DataPathLink, ...]) -> None:
        """Reject feedback edges before Python routing can spin forever.

        Rust dataflow compilation already rejects cyclic algorithm graphs. The
        portable Python edge engine needs the same invariant at its topology
        boundary, including the otherwise easy-to-miss self-loop case.
        """
        edges: dict[str, set[str]] = {}
        for link in links:
            if link.sink_node is None:
                continue
            edges.setdefault(link.source_node, set()).add(link.sink_node)
            edges.setdefault(link.sink_node, set())

        visiting: set[str] = set()
        visited: set[str] = set()

        def visit(node: str) -> None:
            if node in visiting:
                raise ValueError(
                    "datapath route graph contains a cycle involving " f"node {node!r}"
                )
            if node in visited:
                return
            visiting.add(node)
            for dependent in sorted(edges[node]):
                visit(dependent)
            visiting.remove(node)
            visited.add(node)

        for node in sorted(edges):
            visit(node)


class Rig(Generic[NodeT]):
    """Typed container boundary implemented by every Rig backend."""

    nodes: dict[str, NodeT]

    def node(self, name: str) -> NodeT:
        try:
            return self.nodes[name]
        except KeyError as exc:
            raise KeyError(f"node {name!r} is not in this rig") from exc


class ClusterRig(Rig[NodeT], Generic[NodeT]):
    """Backend-independent cluster of Rig nodes and components.

    The cluster owns topology and lifecycle. A backend may override the
    protected hooks to provide native scheduling and interface registration,
    while the topology, online state, reset behavior, and public time API stay
    in this generic implementation.
    """

    def __init__(
        self,
        *,
        configuration: ClusterConfig | None = None,
        name: str | None = None,
        hardware: str | None = None,
        features: frozenset[str] | set[str] | tuple[str, ...] = frozenset(),
        components: tuple[ComponentRig, ...] = (),
        connect: bool = True,
        **nodes: NodeT,
    ) -> None:
        if not nodes and not components:
            raise ValueError("ClusterRig requires at least one node or component")
        self.configuration = configuration or ClusterConfig()
        self.name = name or "cluster"
        self.hardware = hardware
        self.features = frozenset(features)
        self.nodes = dict(nodes)
        self.components = tuple(components)
        elements = (*self.nodes.values(), *self.components)
        self._require_elements(elements)
        self._component_nodes = {
            f"__component_{index}": component
            for index, component in enumerate(self.components)
        }
        self._base_component_count = len(self.components)
        self._rig_nodes = {**self.nodes, **self._component_nodes}
        self._online_nodes = {name: True for name in self._rig_nodes}
        self.elapsed_ns = 0
        self._runtime: RigRuntime | None = None
        self.dataroutes = self._create_dataroutes()
        self._initialize_backend()
        self._attach_nodes()
        if connect:
            self.connect()

    def _create_dataroutes(self) -> DataflowRoutes:
        return DataflowRoutes(self)

    def _initialize_backend(self) -> None:
        """Initialize an optional backend after topology has been created."""

    @property
    def runtime(self) -> RigRuntime | None:
        """Backend runtime, when this Rig provides one."""
        return self._runtime

    def _connect_interfaces(self) -> None:
        self.dataroutes.connect_available_paths()

    def _reset_backend(self) -> None:
        """Reset backend-owned state before generic routes reconnect."""

    def _add_backend_node(self, name: str, node: ModelRig) -> None:
        del name, node

    def _run_backend(self, duration_ns: int, step_ns: int) -> None:
        elapsed_ns = 0
        while elapsed_ns < duration_ns:
            delta_ns = min(step_ns, duration_ns - elapsed_ns)
            for name, node in self._rig_nodes.items():
                if self._online_nodes[name]:
                    node.run_for(delta_ns, unit="ns")
            self.elapsed_ns += delta_ns
            elapsed_ns += delta_ns
            self.dataroutes.route()

    def _set_backend_node_online(self, name: str, online: bool) -> None:
        del name, online

    def _has_python_routes(self) -> bool:
        return self.dataroutes.has_python_routes()

    def connect(self) -> None:
        self._connect_interfaces()

    def _attach_nodes(self) -> None:
        for name, node in self._rig_nodes.items():
            node.attach_to(self, name)

    def __getattr__(self, name: str) -> ModelRig:
        try:
            return self.nodes[name]
        except KeyError as exc:
            raise AttributeError(name) from exc

    def reset(self) -> None:
        for node in self._rig_nodes.values():
            node.reset()
        self._online_nodes = {name: True for name in self._rig_nodes}
        self.elapsed_ns = 0
        self._reset_backend()
        self.dataroutes.reset()
        self.connect()

    def reset_to_initial_topology(self) -> None:
        if len(self.components) != self._base_component_count:
            for name in tuple(self._component_nodes):
                index = int(name.removeprefix("__component_"))
                if index < self._base_component_count:
                    continue
                component = self._component_nodes.pop(name)
                component.detach_from()
                self._rig_nodes.pop(name, None)
                self._online_nodes.pop(name, None)
            self.components = self.components[: self._base_component_count]
        self.reset()

    def add_component(self, component: ComponentRig) -> ComponentRig:
        return self.add_components(component)[0]

    def add_components(self, *components: ComponentRig) -> tuple[ComponentRig, ...]:
        if self.elapsed_ns != 0:
            raise RuntimeError(
                "components must be added before a cluster starts running"
            )
        self._require_elements(components)
        start_index = len(self.components)
        self.components = (*self.components, *components)
        for offset, component in enumerate(components):
            name = f"__component_{start_index + offset}"
            self._component_nodes[name] = component
            self._rig_nodes[name] = component
            self._online_nodes[name] = True
            component.attach_to(self, name)
            self._add_backend_node(name, component)
        self.connect()
        return components

    def has_feature(self, feature: str) -> bool:
        return feature in self.features

    def run_for(
        self,
        duration: int | float,
        *,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
    ) -> None:
        duration_ns = duration_to_ns(duration, unit=unit)
        step_ns = duration_to_ns(step, unit=step_unit or unit)
        if step_ns <= 0:
            raise ValueError(f"step must be positive, got {step}")
        self._run_backend(duration_ns, step_ns)

    def run_until(
        self,
        predicate,
        *,
        timeout: int | float,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
        message: str | None = None,
    ) -> int:
        return run_until(
            lambda delta_ns: self.run_for(
                delta_ns, unit="ns", step=delta_ns, step_unit="ns"
            ),
            predicate,
            timeout_ns=duration_to_ns(timeout, unit=unit),
            step_ns=duration_to_ns(step, unit=step_unit or unit),
            message=message,
        )

    def set_node_online(self, name: str, online: bool) -> None:
        if name not in self._rig_nodes:
            raise KeyError(f"node {name!r} is not in this rig")
        was_online = self._online_nodes[name]
        self._online_nodes[name] = online
        if was_online != online:
            self._set_backend_node_online(name, online)

    def disable_node(self, name: str) -> None:
        self.set_node_online(name, False)

    def enable_node(self, name: str) -> None:
        self.set_node_online(name, True)

    def node_online(self, name: str) -> bool:
        if name not in self._rig_nodes:
            raise KeyError(f"node {name!r} is not in this rig")
        return self._online_nodes[name]

    @staticmethod
    def _require_elements(elements: tuple[object, ...]) -> None:
        invalid = tuple(
            type(element).__name__
            for element in elements
            if not isinstance(element, RigElement)
        )
        if invalid:
            raise TypeError(
                "ClusterRig elements must implement the RigElement contract; "
                f"invalid elements: {', '.join(invalid)}"
            )


__all__ = ["ClusterRig", "DataflowRoutes", "Rig"]
