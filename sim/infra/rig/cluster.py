from __future__ import annotations

import pathlib
from dataclasses import dataclass
from typing import Generic, TypeVar

from .can import CanBusDescriptor, CanEvent, CanMessageDescriptor, RoutedCanEvent
from .datapath import (
    DataPath,
    DataPathLink,
    DataPathRecord,
    DataPathSink,
    DataPathSource,
    FanoutDataPath,
    datapath_key,
)
from .model import ComponentRig, ModelRig
from .peripherals import SpiTransaction, TimerChannelEvent
from .runtime import _RustClusterRuntime, _StandaloneRustRuntimeHost
from .time import duration_to_ns, run_until


PayloadT = TypeVar("PayloadT")


@dataclass(frozen=True)
class _DataPathRoute:
    source_node: str
    source: DataPathSource[object]
    sinks: tuple[DataPathSink[object], ...]


class ClusterDataRoutes:
    def __init__(self, cluster: ClusterRig) -> None:
        self._cluster = cluster
        self._fanouts: dict[str, FanoutDataPath[object]] = {}
        self._paths: dict[str, DataPath] = {}
        self._links: list[DataPathLink] = []
        self._route_cache: dict[str, tuple[_DataPathRoute, ...]] = {}
        self._latest_records: dict[tuple[str, str], DataPathRecord[object]] = {}

    def register(self, path: DataPath) -> None:
        self._fanout(path)

    def connect(
        self,
        path: DataPath,
        *,
        source_node: str,
        sink_node: str | None = None,
    ) -> None:
        if source_node not in self._cluster._rig_nodes:
            raise KeyError(f"source node {source_node!r} is not in this rig")
        if sink_node is not None and sink_node not in self._cluster._rig_nodes:
            raise KeyError(f"sink node {sink_node!r} is not in this rig")
        link = DataPathLink(
            path=path,
            source_node=source_node,
            sink_node=sink_node,
        )
        if link not in self._links:
            self._links.append(link)
            self._route_cache.clear()
        self._fanout(path)

    def connect_available_paths(self) -> None:
        for source_node, node in self._cluster._rig_nodes.items():
            for output in node.datapaths.outputs():
                sink_nodes = tuple(
                    sink_node
                    for sink_node, sink in self._cluster._rig_nodes.items()
                    if sink_node != source_node and sink.datapaths.inputs(output.path)
                )
                if not sink_nodes:
                    self.connect(output.path, source_node=source_node)
                    continue
                for sink_node in sink_nodes:
                    self.connect(
                        output.path,
                        source_node=source_node,
                        sink_node=sink_node,
                    )

    def route(self, path: DataPath | None = None) -> None:
        paths = (path,) if path is not None else self.paths
        for path_name in paths:
            self._route_path(path_name)

    def reset(self) -> None:
        for fanout in self._fanouts.values():
            fanout.clear()
        self._latest_records.clear()

    def clear(self, path: DataPath) -> None:
        self._fanout(path).clear()
        key = datapath_key(path)
        self._latest_records = {
            latest_key: record
            for latest_key, record in self._latest_records.items()
            if latest_key[1] != key
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

        latest = None
        for (node, record_key), record in self._latest_records.items():
            if record_key != key:
                continue
            if latest is None or record.timestamp_ns >= latest.timestamp_ns:
                latest = record
        return latest

    @property
    def paths(self) -> tuple[DataPath, ...]:
        return tuple(self._paths.values())

    def _fanout(self, path: DataPath) -> FanoutDataPath[object]:
        key = datapath_key(path)
        self._paths.setdefault(key, path)
        if key not in self._fanouts:
            self._fanouts[key] = FanoutDataPath()
        return self._fanouts[key]

    def _route_path(self, path: DataPath) -> None:
        fanout = self._fanout(path)
        for route in self._routes_for_path(path):
            if not self._cluster.node_online(route.source_node):
                continue
            while route.source.pending():
                pending = route.source.pending()
                payloads = self._recv_route_payloads(route.source, pending)
                if not payloads:
                    break

                for payload in payloads:
                    record = DataPathRecord(
                        route.source.node,
                        route.source.path,
                        payload,
                        self._cluster.elapsed_ns,
                    )
                    fanout.records.append(record)
                    self._latest_records[
                        (route.source.node, datapath_key(route.source.path))
                    ] = record
                for sink in route.sinks:
                    self._send_route_payloads(sink, payloads)

    @staticmethod
    def _recv_route_payloads(
        source: DataPathSource[object],
        pending: int,
    ) -> tuple[object, ...]:
        if source.recv_many is not None:
            return source.recv_many(pending)

        payload = source.recv()
        return () if payload is None else (payload,)

    @staticmethod
    def _send_route_payloads(
        sink: DataPathSink[object],
        payloads: tuple[object, ...],
    ) -> None:
        if sink.send_many is not None:
            accepted = sink.send_many(payloads)
            if accepted != len(payloads):
                raise RuntimeError(
                    f"datapath sink {sink.node!r} accepted {accepted} of {len(payloads)} payloads"
                )
            return

        for payload in payloads:
            sink.send(payload)

    def _routes_for_path(self, path: DataPath) -> tuple[_DataPathRoute, ...]:
        key = datapath_key(path)
        if key in self._route_cache:
            return self._route_cache[key]

        links_by_source: dict[str, list[DataPathLink]] = {}
        for link in self._links_for_path(path):
            links_by_source.setdefault(link.source_node, []).append(link)

        routes: list[_DataPathRoute] = []
        for source_node, links in links_by_source.items():
            source = self._source_for_link(links[0])
            sinks = tuple(sink for link in links for sink in self._sinks_for_link(link))
            routes.append(_DataPathRoute(source_node, source, sinks))
        self._route_cache[key] = tuple(routes)
        return self._route_cache[key]

    def _links_for_path(self, path: DataPath) -> tuple[DataPathLink, ...]:
        key = datapath_key(path)
        return tuple(link for link in self._links if datapath_key(link.path) == key)

    def _source_for_link(self, link: DataPathLink) -> DataPathSource[object]:
        node = self._cluster._rig_nodes[link.source_node]
        for output in node.datapaths.outputs(link.path):
            return DataPathSource(
                node=link.source_node,
                path=output.path,
                pending=output.pending,
                recv=output.recv,
                recv_many=output.recv_many,
            )
        raise KeyError(
            f"node {link.source_node!r} has no output for datapath " f"{link.path!r}"
        )

    def _sinks_for_link(self, link: DataPathLink) -> tuple[DataPathSink[object], ...]:
        if link.sink_node is None:
            return ()

        node = self._cluster._rig_nodes[link.sink_node]
        sinks: list[DataPathSink[object]] = []
        for input_ in node.datapaths.inputs(link.path):
            sinks.append(
                DataPathSink(
                    node=link.sink_node,
                    path=input_.path,
                    send=input_.send,
                    send_many=input_.send_many,
                )
            )
        if not sinks:
            raise KeyError(
                f"node {link.sink_node!r} has no input for datapath " f"{link.path!r}"
            )
        return tuple(sinks)


class ClusterCanComms:
    PATH = "can"

    def __init__(self, cluster: ClusterRig, dataroutes: ClusterDataRoutes) -> None:
        self._cluster = cluster
        self._dataroutes = dataroutes

    def connect_bus(
        self,
        bus: int | str | CanBusDescriptor,
        *,
        nodes: tuple[str, ...] | list[str] | None = None,
    ) -> None:
        node_names = tuple(self._cluster.nodes) if nodes is None else tuple(nodes)
        for source_node in node_names:
            source_bus = self._cluster.nodes[source_node].can.bus(bus)
            path = self.path(source_bus)
            if len(node_names) == 1:
                self._dataroutes.connect(
                    path,
                    source_node=source_node,
                )
                continue
            for sink_node in node_names:
                if sink_node == source_node:
                    continue
                self._dataroutes.connect(
                    path,
                    source_node=source_node,
                    sink_node=sink_node,
                )

    def connect_nodes(self, nodes: tuple[str, ...] | list[str]) -> None:
        node_names = tuple(nodes)
        for node_name in node_names:
            if node_name not in self._cluster.nodes:
                raise KeyError(f"CAN node {node_name!r} is not in this rig")

        for source_node in node_names:
            source = self._cluster.nodes[source_node]
            for source_bus in source.can.buses:
                path = self.path(source_bus)
                if not self._node_has_datapath_output(source_node, path):
                    continue
                sink_buses = tuple(
                    sink_node
                    for sink_node in node_names
                    if sink_node != source_node
                    for sink_bus in self._cluster.nodes[sink_node].can.buses
                    if sink_bus.name == source_bus.name
                    and self._node_has_datapath_input(sink_node, path)
                )
                if not sink_buses:
                    self._dataroutes.connect(
                        path,
                        source_node=source_node,
                    )
                    continue
                for sink_node in sink_buses:
                    self._dataroutes.connect(
                        path,
                        source_node=source_node,
                        sink_node=sink_node,
                    )

    def _node_has_datapath_output(self, node: str, path: DataPath) -> bool:
        return bool(self._cluster.nodes[node].datapaths.outputs(path))

    def _node_has_datapath_input(self, node: str, path: DataPath) -> bool:
        return bool(self._cluster.nodes[node].datapaths.inputs(path))

    def connect_available_nodes(self) -> None:
        self.connect_nodes(
            tuple(
                node_name
                for node_name, node in self._cluster.nodes.items()
                if node.has_can
            )
        )

    @property
    def events(self) -> tuple[RoutedCanEvent, ...]:
        events = []
        for path in self._dataroutes.paths:
            if not self._is_can_path(path):
                continue
            bus_name = str(path.parts[1])
            for record in self._dataroutes.records(path):
                if isinstance(record.payload, CanEvent):
                    events.append(
                        RoutedCanEvent(
                            record.source,
                            CanBusDescriptor(record.payload.bus, bus_name),
                            record.payload,
                        )
                    )
        return tuple(events)

    def route(self) -> None:
        for path in self._dataroutes.paths:
            if self._is_can_path(path):
                self._dataroutes.route(path)

    def reset(self) -> None:
        self.clear()

    def clear(self) -> None:
        for path in self._dataroutes.paths:
            if self._is_can_path(path):
                self._dataroutes.clear(path)

    @classmethod
    def path(cls, bus: CanBusDescriptor | str) -> DataPath:
        bus_name = bus.name if isinstance(bus, CanBusDescriptor) else str(bus)
        return DataPath.can_bus(bus_name)

    @classmethod
    def _is_can_path(cls, path: DataPath) -> bool:
        return len(path.parts) == 2 and path.parts[0] == cls.PATH

    def latest_message(
        self,
        node: str,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor,
    ) -> CanEvent | None:
        node_rig = self._cluster.nodes[node]
        bus_descriptor = node_rig.can.bus(bus)
        message_id = (
            node_rig.can.tx_message(message, bus=bus_descriptor).id
            if isinstance(message, str)
            else message.id
        )

        path = self.path(bus_descriptor)
        for record in self._dataroutes.reversed_records(path):
            if record.source != node or not isinstance(record.payload, CanEvent):
                continue
            if (
                record.payload.bus == bus_descriptor.index
                and record.payload.packet.id == message_id
            ):
                return record.payload
        return None

    def latest_bus_event(
        self,
        node: str,
        bus: int | str | CanBusDescriptor,
    ) -> CanEvent | None:
        bus_descriptor = self._cluster.nodes[node].can.bus(bus)
        record = self._dataroutes.latest_record(
            self.path(bus_descriptor),
            source_node=node,
        )
        if isinstance(record, DataPathRecord) and isinstance(record.payload, CanEvent):
            if record.payload.bus == bus_descriptor.index:
                return record.payload
        return None


class _TypedClusterComms(Generic[PayloadT]):
    def __init__(
        self, dataroutes: ClusterDataRoutes, payload_type: type[PayloadT]
    ) -> None:
        self._dataroutes = dataroutes
        self._payload_type = payload_type

    def records(self, path: DataPath) -> tuple[DataPathRecord[PayloadT], ...]:
        return tuple(
            record
            for record in self._dataroutes.records(path)
            if isinstance(record.payload, self._payload_type)
        )

    def events(self, path: DataPath) -> tuple[PayloadT, ...]:
        return tuple(record.payload for record in self.records(path))

    def latest_event(
        self,
        path: DataPath,
        *,
        node: str | None = None,
    ) -> PayloadT | None:
        for record in reversed(self.records(path)):
            if node is not None and record.source != node:
                continue
            return record.payload
        return None


class ClusterTimerComms(_TypedClusterComms[TimerChannelEvent]):
    def __init__(self, dataroutes: ClusterDataRoutes) -> None:
        super().__init__(dataroutes, TimerChannelEvent)


class ClusterSpiComms(_TypedClusterComms[SpiTransaction]):
    def __init__(self, dataroutes: ClusterDataRoutes) -> None:
        super().__init__(dataroutes, SpiTransaction)


class ClusterComms:
    def __init__(self, cluster: ClusterRig, dataroutes: ClusterDataRoutes) -> None:
        self._dataroutes = dataroutes
        self.can = ClusterCanComms(cluster, dataroutes)
        self.timer = ClusterTimerComms(dataroutes)
        self.spi = ClusterSpiComms(dataroutes)

    def route(self) -> None:
        self._dataroutes.route()

    def reset(self) -> None:
        self._dataroutes.reset()

    def connect_node_interfaces(self) -> None:
        self.can.connect_available_nodes()
        self._dataroutes.connect_available_paths()


class ClusterRig:
    def __init__(
        self,
        *,
        name: str | None = None,
        hardware: str | None = None,
        features: frozenset[str] | set[str] | tuple[str, ...] = frozenset(),
        components: tuple[ComponentRig, ...] = (),
        **nodes: ModelRig,
    ) -> None:
        if not nodes and not components:
            raise ValueError("ClusterRig requires at least one node or component")
        self.name = name or "cluster"
        self.hardware = hardware
        self.features = frozenset(features)
        self.nodes = dict(nodes)
        self._reject_duplicate_shared_libraries()
        self.components = tuple(components)
        self._component_nodes = {
            f"__component_{index}": component
            for index, component in enumerate(self.components)
        }
        self._rig_nodes = {
            **self.nodes,
            **self._component_nodes,
        }
        for node_name, node in self.nodes.items():
            node._cluster_rig = self
            node._cluster_node_name = node_name
        for node_name, node in self._component_nodes.items():
            node._cluster_rig = self
            node._cluster_node_name = node_name
        self._online_nodes = {name: True for name in self._rig_nodes}
        self.elapsed_ns = 0
        self.dataroutes = ClusterDataRoutes(self)
        self.comm = ClusterComms(self, self.dataroutes)
        self.comm.connect_node_interfaces()
        self._rust_runtime = self._create_rust_runtime()

    def _reject_duplicate_shared_libraries(self) -> None:
        nodes_by_library: dict[pathlib.Path, list[str]] = {}
        for node_name, node in self.nodes.items():
            library_path = getattr(node, "library_path", None)
            if library_path is None:
                continue
            nodes_by_library.setdefault(
                pathlib.Path(library_path).resolve(), []
            ).append(node_name)

        duplicates = {
            library_path: node_names
            for library_path, node_names in nodes_by_library.items()
            if len(node_names) > 1
        }
        if duplicates:
            details = "; ".join(
                f"{library_path}: {', '.join(node_names)}"
                for library_path, node_names in sorted(duplicates.items())
            )
            raise ValueError(
                "ClusterRig cannot instantiate the same Rust-backed controller shared object "
                f"more than once because the current model ABI owns singleton runtime state: {details}"
            )

    def __getattr__(self, name: str) -> ModelRig:
        try:
            return self.nodes[name]
        except KeyError as exc:
            raise AttributeError(name) from exc

    def reset(self) -> None:
        for node in self._rig_nodes.values():
            node.reset()
        for name in self._rig_nodes:
            self._online_nodes[name] = True
        self.elapsed_ns = 0
        self.comm.reset()
        self._rust_runtime = self._create_rust_runtime()

    def add_component(self, component: ComponentRig) -> ComponentRig:
        return self.add_components(component)[0]

    def add_components(self, *components: ComponentRig) -> tuple[ComponentRig, ...]:
        if self.elapsed_ns != 0:
            raise RuntimeError(
                "components must be added before a cluster starts running"
            )
        if not components:
            return ()

        start_index = len(self.components)
        self.components = (*self.components, *components)
        for offset, component in enumerate(components):
            name = f"__component_{start_index + offset}"
            self._component_nodes[name] = component
            self._rig_nodes[name] = component
            component._cluster_rig = self
            component._cluster_node_name = name
            self._online_nodes[name] = True
        self.comm.connect_node_interfaces()
        self._rust_runtime = self._create_rust_runtime()
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

        self._rust_runtime.run_for(duration_ns, step_ns, fast_forward=True)
        self._sync_elapsed_from_runtime()

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
            self._rust_runtime.set_node_online(name, online)

    def disable_node(self, name: str) -> None:
        self.set_node_online(name, False)

    def enable_node(self, name: str) -> None:
        self.set_node_online(name, True)

    def node_online(self, name: str) -> bool:
        if name not in self._rig_nodes:
            raise KeyError(f"node {name!r} is not in this rig")
        return self._online_nodes[name]

    def _create_rust_runtime(self) -> _RustClusterRuntime:
        hosts = tuple(
            node for node in self.nodes.values() if hasattr(node, "_bind_symbol")
        )
        unsupported_nodes = tuple(
            name
            for name, node in self._rig_nodes.items()
            if not hasattr(node, "rust_cluster_node_abi")
        )
        if unsupported_nodes:
            raise TypeError(
                "Rust-hosted clusters require every node/component to expose the "
                f"Rust scheduler callback ABI; missing: {', '.join(unsupported_nodes)}"
            )
        host = hosts[0] if hosts else _StandaloneRustRuntimeHost()
        runtime = _RustClusterRuntime(
            host=host,
            route=self._route_from_runtime,
        )
        for name, node in self._rig_nodes.items():
            runtime.add_node(name, node, online=self.node_online(name))
        return runtime

    def _route_from_runtime(self, elapsed_ns: int) -> None:
        self.elapsed_ns = elapsed_ns
        self.comm.route()

    def _sync_elapsed_from_runtime(self) -> None:
        self.elapsed_ns = self._rust_runtime.elapsed_ns()
        elapsed_by_node = self._rust_runtime.node_elapsed_ns_values()
        for name, elapsed_ns in elapsed_by_node.items():
            self._rig_nodes[name].elapsed_ns = elapsed_ns
