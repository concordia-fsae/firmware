from __future__ import annotations

import ctypes
import os
import pathlib
from collections.abc import Callable
from dataclasses import dataclass

from .artifacts import buck_output, load_shared_library, repo_root


@dataclass(frozen=True)
class RustNodeSchedulerAbi:
    run_for: int
    fast_forward_for: int
    next_step: int
    reset: int


class _StandaloneRustRuntimeHost:
    buck_target = "//sim/infra/runtime:runtime-so"
    env_var = "RIG_RUNTIME_SIM_LIB"

    def __init__(self) -> None:
        library_path = os.environ.get(self.env_var)
        self.library_path = (
            pathlib.Path(library_path)
            if library_path is not None
            else buck_output(self.buck_target, repo_root())
        ).resolve()
        self._lib = load_shared_library(self.library_path)

    def bind_symbol(
        self,
        name: str,
        argtypes: list[object] | None = None,
        restype: object | None = None,
    ):
        symbol = getattr(self._lib, name)
        symbol.argtypes = [] if argtypes is None else argtypes
        symbol.restype = restype
        return symbol


class _RustClusterRuntime:
    _RouteCallback = ctypes.CFUNCTYPE(None, ctypes.c_uint64)

    def __init__(
        self,
        *,
        host: object | None = None,
        route: Callable[[int], None] | None = None,
    ) -> None:
        self._node_indices: dict[str, int] = {}
        self._route = route
        self._route_callback = self._RouteCallback(self._route_callback_fn)
        host = host or _StandaloneRustRuntimeHost()
        bind_symbol = getattr(host, "bind_symbol", None)
        if bind_symbol is None:
            bind_symbol = getattr(host, "_bind_symbol")
        self._reset = bind_symbol("rig_cluster_reset")
        self._add_node = bind_symbol(
            "rig_cluster_add_node",
            [
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_bool,
            ],
            ctypes.c_uint32,
        )
        self._set_node_online = bind_symbol(
            "rig_cluster_set_node_online",
            [ctypes.c_uint32, ctypes.c_bool],
            ctypes.c_bool,
        )
        self._run_for = bind_symbol(
            "rig_cluster_run_for",
            [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_bool, ctypes.c_size_t],
        )
        self._add_can_route = bind_symbol(
            "rig_cluster_add_can_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_size_t,
            ],
            ctypes.c_bool,
        )
        self._latest_can_message = bind_symbol(
            "rig_cluster_latest_can_message",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_uint32, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._latest_can_bus_event = bind_symbol(
            "rig_cluster_latest_can_bus_event",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._add_timer_route = bind_symbol(
            "rig_cluster_add_timer_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint16,
                ctypes.c_int32,
                ctypes.c_int32,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_size_t,
            ],
            ctypes.c_bool,
        )
        self._latest_timer_event = bind_symbol(
            "rig_cluster_latest_timer_event",
            [
                ctypes.c_uint32,
                ctypes.c_uint16,
                ctypes.c_int32,
                ctypes.c_int32,
                ctypes.c_void_p,
            ],
            ctypes.c_bool,
        )
        self._add_spi_route = bind_symbol(
            "rig_cluster_add_spi_route",
            [
                ctypes.c_uint32,
                ctypes.c_int32,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_size_t,
            ],
            ctypes.c_bool,
        )
        self._latest_spi_transaction = bind_symbol(
            "rig_cluster_latest_spi_transaction",
            [ctypes.c_uint32, ctypes.c_int32, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._elapsed_ns = bind_symbol(
            "rig_cluster_elapsed_ns",
            restype=ctypes.c_uint64,
        )
        self._node_elapsed_ns = bind_symbol(
            "rig_cluster_node_elapsed_ns",
            [ctypes.c_uint32],
            ctypes.c_uint64,
        )
        self._node_elapsed_ns_many = bind_symbol(
            "rig_cluster_node_elapsed_ns_many",
            [ctypes.POINTER(ctypes.c_uint64), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self.reset()

    def reset(self) -> None:
        self._reset()
        self._node_indices.clear()

    def add_node(self, name: str, node, *, online: bool = True) -> None:
        scheduler = node.rust_cluster_node_abi()
        if not isinstance(scheduler, RustNodeSchedulerAbi):
            raise TypeError(
                f"node {name!r} returned unsupported scheduler ABI "
                f"{type(scheduler).__name__}"
            )
        index = int(
            self._add_node(
                ctypes.c_size_t(scheduler.run_for),
                ctypes.c_size_t(scheduler.fast_forward_for),
                ctypes.c_size_t(scheduler.next_step),
                ctypes.c_size_t(scheduler.reset),
                ctypes.c_bool(online),
            )
        )
        if index == 0xFFFFFFFF:
            raise RuntimeError(f"failed to register Rust cluster node {name!r}")
        self._node_indices[name] = index

    def add_can_route(
        self,
        *,
        source_node: str,
        source_bus: int,
        source_tx_count: int,
        source_recv_events: int,
        sink_node: str,
        sink_bus: int,
        sink_send_many: int,
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
        return bool(
            self._add_can_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint8(source_bus),
                ctypes.c_size_t(source_tx_count),
                ctypes.c_size_t(source_recv_events),
                ctypes.c_uint32(sink_index),
                ctypes.c_uint8(sink_bus),
                ctypes.c_size_t(sink_send_many),
            )
        )

    def add_timer_route(
        self,
        *,
        source_node: str,
        interface: int,
        port: int,
        channel: int,
        source_count: int,
        source_recv_many: int,
        sink_node: str,
        sink_send_many: int,
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
        return bool(
            self._add_timer_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint16(interface),
                ctypes.c_int32(port),
                ctypes.c_int32(channel),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_size_t(sink_send_many),
            )
        )

    def add_spi_route(
        self,
        *,
        source_node: str,
        device: int,
        source_count: int,
        source_recv_many: int,
        sink_node: str,
        sink_send_many: int,
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
        return bool(
            self._add_spi_route(
                ctypes.c_uint32(source_index),
                ctypes.c_int32(device),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_size_t(sink_send_many),
            )
        )

    def run_for(
        self,
        duration_ns: int,
        step_ns: int,
        *,
        fast_forward: bool = False,
        route: bool = True,
    ) -> None:
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        self._run_for(
            ctypes.c_uint64(duration_ns),
            ctypes.c_uint64(step_ns),
            ctypes.c_bool(fast_forward),
            ctypes.c_size_t(route_callback or 0),
        )

    def set_node_online(self, name: str, online: bool) -> None:
        index = self._node_indices.get(name)
        if index is None:
            return
        if not self._set_node_online(ctypes.c_uint32(index), ctypes.c_bool(online)):
            raise RuntimeError(
                f"failed to set Rust cluster node {name!r} online={online}"
            )

    def elapsed_ns(self) -> int:
        return int(self._elapsed_ns())

    def node_elapsed_ns(self, name: str) -> int:
        return int(self._node_elapsed_ns(ctypes.c_uint32(self._node_indices[name])))

    def node_elapsed_ns_values(self) -> dict[str, int]:
        if not self._node_indices:
            return {}
        values = (ctypes.c_uint64 * len(self._node_indices))()
        count = int(self._node_elapsed_ns_many(values, ctypes.c_uint32(len(values))))
        elapsed_by_name = {}
        for name, index in self._node_indices.items():
            if index < count:
                elapsed_by_name[name] = int(values[index])
        return elapsed_by_name

    def latest_can_message(
        self, source_node: str, bus: int, message_id: int, event
    ) -> bool:
        index = self._node_indices.get(source_node)
        if index is None:
            return False
        return bool(
            self._latest_can_message(
                ctypes.c_uint32(index),
                ctypes.c_uint8(bus),
                ctypes.c_uint32(message_id),
                ctypes.byref(event),
            )
        )

    def latest_can_bus_event(self, source_node: str, bus: int, event) -> bool:
        index = self._node_indices.get(source_node)
        if index is None:
            return False
        return bool(
            self._latest_can_bus_event(
                ctypes.c_uint32(index),
                ctypes.c_uint8(bus),
                ctypes.byref(event),
            )
        )

    def latest_timer_event(
        self, source_node: str, interface: int, port: int, channel: int, event
    ) -> bool:
        index = self._node_indices.get(source_node)
        if index is None:
            return False
        return bool(
            self._latest_timer_event(
                ctypes.c_uint32(index),
                ctypes.c_uint16(interface),
                ctypes.c_int32(port),
                ctypes.c_int32(channel),
                ctypes.byref(event),
            )
        )

    def latest_spi_transaction(self, source_node: str, device: int, transaction) -> bool:
        index = self._node_indices.get(source_node)
        if index is None:
            return False
        return bool(
            self._latest_spi_transaction(
                ctypes.c_uint32(index),
                ctypes.c_int32(device),
                ctypes.byref(transaction),
            )
        )

    def _route_callback_fn(self, elapsed_ns: int) -> None:
        if self._route is not None:
            self._route(int(elapsed_ns))
