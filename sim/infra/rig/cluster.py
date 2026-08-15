from __future__ import annotations

import pathlib
from dataclasses import dataclass
from functools import cache
from typing import Generic, TypeVar

from .can import CanBusDescriptor, CanEvent, CanMessageDescriptor, RoutedCanEvent
from .datapath import (
    DataPathKey,
    DataPath,
    DataPathLink,
    DataPathRecord,
    DataPathSink,
    DataPathSource,
    FanoutDataPath,
    PeripheralInterface,
    can_datapath_bus,
    datapath_key,
    is_can_datapath,
)
from .model import ComponentRig, ModelRig
from .runtime import _RustClusterRuntime, _StandaloneRustRuntimeHost
from .dataflow import NativeRouteEndpoint
from .scalar import ScalarEvent
from .spi import SpiTransaction
from .timer import TimerChannelEvent
from .time import duration_to_ns, run_until


PayloadT = TypeVar("PayloadT")


@dataclass(frozen=True)
class _DataPathRoute:
    source_node: str
    source_key: DataPathKey
    source: DataPathSource[object]
    sinks: tuple[DataPathSink[object], ...]


class ClusterDataRoutes:
    def __init__(self, cluster: ClusterRig) -> None:
        self._cluster = cluster
        self._fanouts: dict[DataPathKey, FanoutDataPath[object]] = {}
        self._paths: dict[DataPathKey, DataPath] = {}
        self._links: list[DataPathLink] = []
        self._route_cache: dict[DataPathKey, tuple[_DataPathRoute, ...]] = {}
        self._ordered_paths_cache: tuple[DataPath, ...] | None = None
        self._latest_records: dict[
            tuple[str, DataPathKey], DataPathRecord[object]
        ] = {}
        self._native_routes: set[tuple[DataPathKey, str, str]] = set()
        self._native_route_abi_cache: dict[
            tuple[str, DataPathKey], NativeRouteEndpoint | None
        ] = {}

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
        if sink_node is not None and self._connect_native_route(
            path,
            source_node=source_node,
            sink_node=sink_node,
        ):
            return
        if sink_node is not None and self._native_route_available(
            path,
            source_node=source_node,
            sink_node=sink_node,
        ):
            raise TypeError(
                f"datapath {path!r} between {source_node!r} and {sink_node!r} "
                "advertises a Rust route ABI but failed to connect natively"
            )
        if self._requires_native_route(path):
            raise TypeError(
                f"datapath {path!r} between {source_node!r} and {sink_node!r} "
                "requires a Rust route ABI"
            )
        link = DataPathLink(
            path=path,
            source_node=source_node,
            sink_node=sink_node,
        )
        if link not in self._links:
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
                if not sink_nodes:
                    if (
                        getattr(node, "rust_datapath_route_abi", lambda _path: None)(
                            output.path
                        )
                        is not None
                    ):
                        self._fanout(output.path)
                        continue
                    self.connect(output.path, source_node=source_node)
                    continue
                for sink_node in sink_nodes:
                    self.connect(
                        output.path,
                        source_node=source_node,
                        sink_node=sink_node,
                    )

    def _route(self, path: DataPath | None = None) -> None:
        paths = (path,) if path is not None else self._ordered_paths()
        for path_name in paths:
            self._route_path(path_name)

    def reset(self) -> None:
        for fanout in self._fanouts.values():
            fanout.clear()
        self._latest_records.clear()
        self._native_routes.clear()
        self._native_route_abi_cache.clear()

    def clear_native_routes(self) -> None:
        self._native_routes.clear()
        self._native_route_abi_cache.clear()

    def has_python_routes(self) -> bool:
        return bool(self._links)

    def clear(self, path: DataPath) -> None:
        self._fanout(path).clear()
        key = datapath_key(path)
        self._latest_records = {
            latest_key: record
            for latest_key, record in self._latest_records.items()
            if latest_key[1] != key
        }

    def records(self, path: DataPath) -> tuple[DataPathRecord[object], ...]:
        records = tuple(self._fanout(path).records)
        if records:
            return records
        return self._native_scalar_records(path)

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
            record = self._latest_records.get((source_node, key))
            if record is not None:
                return record
            for native_record in self._native_scalar_records(path):
                if native_record.source == source_node:
                    return native_record
            return None

        latest = None
        for (node, record_key), record in self._latest_records.items():
            if record_key != key:
                continue
            if latest is None or record.timestamp_ns >= latest.timestamp_ns:
                latest = record
        for record in self._native_scalar_records(path):
            if latest is None or record.timestamp_ns >= latest.timestamp_ns:
                latest = record
        return latest

    @property
    def paths(self) -> tuple[DataPath, ...]:
        return tuple(self._paths.values())

    def _ordered_paths(self) -> tuple[DataPath, ...]:
        if self._ordered_paths_cache is not None:
            return self._ordered_paths_cache

        paths = self.paths
        source_nodes_by_key: dict[DataPathKey, set[str]] = {}
        sink_nodes_by_key: dict[DataPathKey, set[str]] = {}
        for path in paths:
            key = datapath_key(path)
            for node_name, node in self._cluster._rig_nodes.items():
                if node.datapaths.outputs(path):
                    source_nodes_by_key.setdefault(key, set()).add(node_name)
                if node.datapaths.inputs(path):
                    sink_nodes_by_key.setdefault(key, set()).add(node_name)

        path_by_key = {datapath_key(path): path for path in paths}
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

    def _fanout(self, path: DataPath) -> FanoutDataPath[object]:
        key = datapath_key(path)
        if key not in self._paths:
            self._paths[key] = path
            self._ordered_paths_cache = None
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
                    self._latest_records[(route.source.node, route.source_key)] = record
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
            routes.append(
                _DataPathRoute(source_node, datapath_key(source.path), source, sinks)
            )
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

    def _connect_native_route(
        self,
        path: DataPath,
        *,
        source_node: str,
        sink_node: str,
    ) -> bool:
        key = (datapath_key(path), source_node, sink_node)
        if key in self._native_routes:
            return True

        source_abi = self._rust_datapath_route_abi(source_node, path)
        sink_abi = self._rust_datapath_route_abi(sink_node, path)
        if source_abi is None or sink_abi is None:
            return False
        if not source_abi.compatible_with(sink_abi):
            return False

        connected = source_abi.connect(
            self._cluster._rust_runtime,
            source_node=source_node,
            sink_node=sink_node,
            sink=sink_abi,
        )

        if connected:
            self._native_routes.add(key)
        return connected

    def _native_route_available(
        self,
        path: DataPath,
        *,
        source_node: str,
        sink_node: str,
    ) -> bool:
        source_abi = self._rust_datapath_route_abi(source_node, path)
        sink_abi = self._rust_datapath_route_abi(sink_node, path)
        return source_abi is not None or sink_abi is not None

    def _rust_datapath_route_abi(
        self,
        node_name: str,
        path: DataPath,
    ) -> NativeRouteEndpoint | None:
        key = (node_name, datapath_key(path))
        if key not in self._native_route_abi_cache:
            node = self._cluster._rig_nodes[node_name]
            self._native_route_abi_cache[key] = getattr(
                node,
                "rust_datapath_route_abi",
                lambda _path: None,
            )(path)
        return self._native_route_abi_cache[key]

    @staticmethod
    def _requires_native_route(path: DataPath) -> bool:
        binding = path.peripheral_binding
        return binding is not None and binding.interface in {
            PeripheralInterface.TIMER_DUTY,
            PeripheralInterface.TIMER_FREQUENCY,
            PeripheralInterface.SPI_TRANSACTION,
        }

    def _native_scalar_records(
        self, path: DataPath
    ) -> tuple[DataPathRecord[object], ...]:
        native_records: list[DataPathRecord[object]] = []
        for node_name in self._cluster._rig_nodes:
            route_abi = self._rust_datapath_route_abi(node_name, path)
            if route_abi is None:
                continue
            route_id = route_abi.scalar_source_route_id
            if route_id is None:
                continue
            event = ScalarEvent()
            if self._cluster._rust_runtime.latest_scalar_event(
                node_name,
                route_id,
                event,
            ):
                native_records.append(
                    DataPathRecord(
                        source=node_name,
                        path=path,
                        payload=float(event.value),
                        timestamp_ns=int(event.timestamp_ns),
                    )
                )
        return tuple(native_records)


class ClusterCanComms:
    PATH = "can"

    def __init__(self, cluster: ClusterRig, dataroutes: ClusterDataRoutes) -> None:
        self._cluster = cluster
        self._dataroutes = dataroutes
        self._native_routes: set[tuple[DataPathKey, str, str]] = set()

    def connect_bus(
        self,
        bus: int | str | CanBusDescriptor,
        *,
        nodes: tuple[str, ...] | list[str] | None = None,
    ) -> None:
        node_names = tuple(self._cluster._rig_nodes) if nodes is None else tuple(nodes)
        path = self.path(bus)
        for source_node in node_names:
            if not self._node_has_datapath_output(source_node, path):
                continue
            if len(node_names) == 1:
                if not self._connect_native_source(path, source_node=source_node):
                    if self._native_source_available(path, source_node=source_node):
                        raise TypeError(
                            f"CAN source {source_node!r} {path!r} advertises a Rust "
                            "route ABI but failed to connect natively"
                        )
                    self._dataroutes.connect(
                        path,
                        source_node=source_node,
                    )
                continue
            for sink_node in node_names:
                if sink_node == source_node:
                    continue
                if not self._node_has_datapath_input(sink_node, path):
                    continue
                if self._connect_native_route(
                    path,
                    source_node=source_node,
                    sink_node=sink_node,
                ):
                    continue
                if self._native_route_available(
                    path,
                    source_node=source_node,
                    sink_node=sink_node,
                ):
                    raise TypeError(
                        f"CAN route {path!r} between {source_node!r} and "
                        f"{sink_node!r} advertises a Rust route ABI but failed "
                        "to connect natively"
                    )
                self._dataroutes.connect(
                    path,
                    source_node=source_node,
                    sink_node=sink_node,
                )

    def connect_nodes(self, nodes: tuple[str, ...] | list[str]) -> None:
        node_names = tuple(nodes)
        for node_name in node_names:
            if node_name not in self._cluster._rig_nodes:
                raise KeyError(f"CAN node {node_name!r} is not in this rig")

        for source_node in node_names:
            for path in self._node_can_output_paths(source_node):
                sink_nodes = tuple(
                    sink_node
                    for sink_node in node_names
                    if sink_node != source_node
                    if self._node_has_datapath_input(sink_node, path)
                )
                if not sink_nodes:
                    if not self._connect_native_source(path, source_node=source_node):
                        if self._native_source_available(path, source_node=source_node):
                            raise TypeError(
                                f"CAN source {source_node!r} {path!r} advertises a "
                                "Rust route ABI but failed to connect natively"
                            )
                        self._dataroutes.connect(path, source_node=source_node)
                    continue
                for sink_node in sink_nodes:
                    if self._connect_native_route(
                        path,
                        source_node=source_node,
                        sink_node=sink_node,
                    ):
                        continue
                    if self._native_route_available(
                        path,
                        source_node=source_node,
                        sink_node=sink_node,
                    ):
                        raise TypeError(
                            f"CAN route {path!r} between {source_node!r} and "
                            f"{sink_node!r} advertises a Rust route ABI but failed "
                            "to connect natively"
                        )
                    self._dataroutes.connect(
                        path,
                        source_node=source_node,
                        sink_node=sink_node,
                    )

    def _node_has_datapath_output(self, node: str, path: DataPath) -> bool:
        return bool(self._cluster._rig_nodes[node].datapaths.outputs(path))

    def _node_has_datapath_input(self, node: str, path: DataPath) -> bool:
        return bool(self._cluster._rig_nodes[node].datapaths.inputs(path))

    def _node_can_output_paths(self, node: str) -> tuple[DataPath, ...]:
        return tuple(
            output.path
            for output in self._cluster._rig_nodes[node].datapaths.outputs()
            if self._is_can_path(output.path)
        )

    def connect_available_nodes(self) -> None:
        self.connect_nodes(
            tuple(
                node_name
                for node_name, node in self._cluster._rig_nodes.items()
                if any(self._is_can_path(path) for path in node.datapaths.paths)
            )
        )

    def _connect_native_route(
        self,
        path: DataPath,
        *,
        source_node: str,
        sink_node: str,
    ) -> bool:
        if not self._is_can_path(path):
            return False
        key = (datapath_key(path), source_node, sink_node)
        if key in self._native_routes:
            return True

        bus_name = self._bus_name(path)
        source = self._cluster._rig_nodes.get(source_node)
        sink = self._cluster._rig_nodes.get(sink_node)
        if source is None or sink is None:
            return False
        source_abi = getattr(source, "rust_can_route_abi", lambda _bus: None)(bus_name)
        sink_abi = getattr(sink, "rust_can_route_abi", lambda _bus: None)(bus_name)
        if source_abi is None or sink_abi is None:
            return False

        source_bus, source_tx_count, source_recv_events, _source_send_many = source_abi
        sink_bus, _sink_tx_count, _sink_recv_events, sink_send_many = sink_abi
        if not self._cluster._rust_runtime.add_can_route(
            source_node=source_node,
            source_bus=source_bus,
            source_tx_count=source_tx_count,
            source_recv_events=source_recv_events,
            sink_node=sink_node,
            sink_bus=sink_bus,
            sink_send_many=sink_send_many,
        ):
            return False

        self._native_routes.add(key)
        return True

    def _native_route_available(
        self,
        path: DataPath,
        *,
        source_node: str,
        sink_node: str,
    ) -> bool:
        if not self._is_can_path(path):
            return False
        bus_name = self._bus_name(path)
        source = self._cluster._rig_nodes.get(source_node)
        sink = self._cluster._rig_nodes.get(sink_node)
        if source is None or sink is None:
            return False
        source_abi = getattr(source, "rust_can_route_abi", lambda _bus: None)(bus_name)
        sink_abi = getattr(sink, "rust_can_route_abi", lambda _bus: None)(bus_name)
        return source_abi is not None or sink_abi is not None

    def _connect_native_source(
        self,
        path: DataPath,
        *,
        source_node: str,
    ) -> bool:
        if not self._is_can_path(path):
            return False
        key = (datapath_key(path), source_node, "")
        if key in self._native_routes:
            return True
        bus_name = self._bus_name(path)
        source = self._cluster._rig_nodes.get(source_node)
        if source is None:
            return False
        source_abi = getattr(source, "rust_can_route_abi", lambda _bus: None)(bus_name)
        if source_abi is None:
            return False
        source_bus, source_tx_count, source_recv_events, _source_send_many = source_abi
        if not self._cluster._rust_runtime.add_can_route(
            source_node=source_node,
            source_bus=source_bus,
            source_tx_count=source_tx_count,
            source_recv_events=source_recv_events,
        ):
            raise RuntimeError(
                f"failed to register Rust CAN source route for {source_node!r} {path!r}"
            )
        self._native_routes.add(key)
        return True

    def _native_source_available(self, path: DataPath, *, source_node: str) -> bool:
        if not self._is_can_path(path):
            return False
        source = self._cluster._rig_nodes.get(source_node)
        if source is None:
            return False
        return (
            getattr(source, "rust_can_route_abi", lambda _bus: None)(
                self._bus_name(path)
            )
            is not None
        )

    @property
    def events(self) -> tuple[RoutedCanEvent, ...]:
        events = []
        for path in self._dataroutes.paths:
            if not self._is_can_path(path):
                continue
            bus_name = self._bus_name(path)
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

    def reset(self) -> None:
        self.clear()
        self._native_routes.clear()

    def clear_native_routes(self) -> None:
        self._native_routes.clear()

    def clear(self) -> None:
        for path in self._dataroutes.paths:
            if self._is_can_path(path):
                self._dataroutes.clear(path)

    @classmethod
    def path(cls, bus: CanBusDescriptor) -> DataPath:
        if not isinstance(bus, CanBusDescriptor):
            raise TypeError(
                f"CAN datapaths require CanBusDescriptor identity, got {type(bus).__name__}"
            )
        return cls._path_for_bus(bus)

    @staticmethod
    @cache
    def _path_for_bus(bus: CanBusDescriptor) -> DataPath:
        return DataPath.can_bus(bus)

    @classmethod
    def _is_can_path(cls, path: DataPath) -> bool:
        return is_can_datapath(path)

    @staticmethod
    def _bus_name(path: DataPath) -> str:
        bus = can_datapath_bus(path)
        if isinstance(bus, CanBusDescriptor):
            return bus.name
        return str(bus)

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

        event = CanEvent()
        if self._cluster._rust_runtime.latest_can_message(
            node,
            bus_descriptor.index,
            message_id,
            event,
        ):
            return event
        return None

    def latest_bus_event(
        self,
        node: str,
        bus: int | str | CanBusDescriptor,
    ) -> CanEvent | None:
        bus_descriptor = self._cluster.nodes[node].can.bus(bus)
        event = CanEvent()
        if self._cluster._rust_runtime.latest_can_bus_event(
            node,
            bus_descriptor.index,
            event,
        ):
            return event

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
    def __init__(self, cluster: ClusterRig, dataroutes: ClusterDataRoutes) -> None:
        self._cluster = cluster
        super().__init__(dataroutes, TimerChannelEvent)

    def records(self, path: DataPath) -> tuple[DataPathRecord[TimerChannelEvent], ...]:
        records = super().records(path)
        if records or path.peripheral_binding is None:
            return records
        for node in self._cluster.nodes:
            event = self.latest_event(path, node=node)
            if event is not None:
                return (
                    DataPathRecord(
                        source=node,
                        path=path,
                        payload=event,
                        timestamp_ns=int(event.timestamp_ns),
                    ),
                )
        return ()

    def latest_event(
        self,
        path: DataPath,
        *,
        node: str | None = None,
    ) -> TimerChannelEvent | None:
        if node is not None and path.peripheral_binding is not None:
            binding = path.peripheral_binding
            if binding.interface in (
                PeripheralInterface.TIMER_DUTY,
                PeripheralInterface.TIMER_FREQUENCY,
            ):
                event = TimerChannelEvent()
                interface = int(binding.interface)
                if self._cluster._rust_runtime.latest_timer_event(
                    node,
                    interface,
                    int(binding.port if binding.port is not None else 0),
                    int(binding.channel if binding.channel is not None else 0),
                    event,
                ):
                    return event
        return super().latest_event(path, node=node)


class ClusterSpiComms(_TypedClusterComms[SpiTransaction]):
    def __init__(self, dataroutes: ClusterDataRoutes) -> None:
        super().__init__(dataroutes, SpiTransaction)


class ClusterComms:
    def __init__(self, cluster: ClusterRig, dataroutes: ClusterDataRoutes) -> None:
        self._dataroutes = dataroutes
        self.can = ClusterCanComms(cluster, dataroutes)
        self.timer = ClusterTimerComms(cluster, dataroutes)
        self.spi = ClusterSpiComms(dataroutes)

    def _route(self) -> None:
        self._dataroutes._route()

    def reset(self) -> None:
        self.can.reset()
        self._dataroutes.reset()

    def clear_native_routes(self) -> None:
        self.can.clear_native_routes()
        self._dataroutes.clear_native_routes()

    def has_python_routes(self) -> bool:
        return self._dataroutes.has_python_routes()

    def connect_node_interfaces(self) -> None:
        self.can.connect_available_nodes()
        self._dataroutes.connect_available_paths(exclude=ClusterCanComms._is_can_path)


class ClusterRig:
    def __init__(
        self,
        *,
        name: str | None = None,
        hardware: str | None = None,
        features: frozenset[str] | set[str] | tuple[str, ...] = frozenset(),
        components: tuple[ComponentRig, ...] = (),
        connect: bool = True,
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
        self._base_component_count = len(self.components)
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
        self._rust_runtime = None
        if connect:
            self._rust_runtime = self._create_rust_runtime()
            self.comm.connect_node_interfaces()

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
        if self._rust_runtime is None:
            self._rust_runtime = self._create_rust_runtime()
        else:
            self._rust_runtime.reset()
            self._populate_rust_runtime(self._rust_runtime)
        self.comm.connect_node_interfaces()

    def reset_to_initial_topology(self) -> None:
        if len(self.components) != self._base_component_count:
            for name, component in tuple(self._component_nodes.items()):
                index = int(name.removeprefix("__component_"))
                if index < self._base_component_count:
                    continue
                component._cluster_rig = None
                component._cluster_node_name = None
                self._component_nodes.pop(name, None)
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
            self._rust_runtime.add_node(name, component, online=True)
        self.comm.connect_node_interfaces()
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

        self._rust_runtime.run_for(
            duration_ns,
            step_ns,
            route=self.comm.has_python_routes(),
        )
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
            if not (
                getattr(node, "rust_runtime_model", lambda: False)()
                or hasattr(node, "scheduler_callbacks")
            )
        )
        if unsupported_nodes:
            raise TypeError(
                "Rust-hosted clusters require every node/component to expose the "
                "scheduler callback interface or opt into Rust runtime model "
                f"stepping; missing: {', '.join(unsupported_nodes)}"
            )
        host = hosts[0] if hosts else _StandaloneRustRuntimeHost()
        runtime = _RustClusterRuntime(
            host=host,
            route=self._route_from_runtime,
        )
        self._populate_rust_runtime(runtime)
        return runtime

    def _populate_rust_runtime(self, runtime: _RustClusterRuntime) -> None:
        self._building_rust_runtime = runtime
        try:
            for name, node in self._rig_nodes.items():
                runtime.add_node(name, node, online=self.node_online(name))
        finally:
            self._building_rust_runtime = None

    def _route_from_runtime(self, elapsed_ns: int) -> None:
        self.elapsed_ns = elapsed_ns
        self.comm._route()

    def _sync_elapsed_from_runtime(self, *, nodes: bool = True) -> None:
        self.elapsed_ns = self._rust_runtime.elapsed_ns()
        if not nodes:
            return
        elapsed_by_node = self._rust_runtime.node_elapsed_ns_values()
        for name, elapsed_ns in elapsed_by_node.items():
            self._rig_nodes[name].elapsed_ns = elapsed_ns
