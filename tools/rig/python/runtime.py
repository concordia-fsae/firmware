"""Rust-backed generic Rig runtime and dataflow operations.

This module owns the runtime surface shared by every Rig backend.  A consuming
binding may extend :class:`RustClusterRuntime` with peripheral-specific route
and event operations, but node registration, scalar edges, scheduling, waits,
and runtime time ownership are Rig concepts.
"""

from __future__ import annotations

import ctypes
import os
import pathlib
from collections.abc import Callable

from .artifacts import load_shared_library
from .contracts import RigRuntime
from .scheduler import PythonSchedulerCallbacks, RustSchedulerCallbacks


class RustRuntimeHost:
    """Shared-library host used by the generic Rust Rig runtime."""

    env_var = "RIG_RUNTIME_LIB"

    def __init__(self, library_path: str | pathlib.Path | None = None) -> None:
        configured_path = library_path or os.environ.get(self.env_var)
        if configured_path is None:
            raise RuntimeError(
                f"{self.env_var} is not configured; a Rig runtime host must provide "
                "the compiled runtime shared library"
            )
        self.library_path = pathlib.Path(configured_path).resolve()
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


class RustClusterRuntime(RigRuntime):
    """Generic Rust implementation of a Rig cluster backend.

    The runtime owns the Rust scheduler, node registry, scalar dataflow edges,
    dataflow waits, and clock.  Peripheral bindings subclass this type and add
    their own route/event ABI without reimplementing the generic operations.
    """

    _RouteCallback = ctypes.CFUNCTYPE(None, ctypes.c_uint64)

    def __init__(
        self,
        *,
        host: object | None = None,
        route: Callable[[int], None] | None = None,
    ) -> None:
        self._node_indices: dict[str, int] = {}
        # Rust stores callback addresses, so the Python node objects must stay
        # alive for as long as their Rust cluster nodes can invoke them.
        self._node_owners: dict[str, object] = {}
        self._route = route
        self._route_callback = self._RouteCallback(self._route_callback_fn)
        host = host or RustRuntimeHost()
        if not hasattr(host, "bind_symbol"):
            raise TypeError("Rig runtime host must implement bind_symbol()")
        self._host_bind_symbol = host.bind_symbol
        bind_symbol = self._host_bind_symbol

        self._reset = bind_symbol("rig_cluster_reset")
        self._add_scalar_transform_algorithm = bind_symbol(
            "rig_cluster_add_scalar_transform_algorithm",
            [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32],
            ctypes.c_bool,
        )
        self._compile_dataflow_graph = bind_symbol(
            "rig_cluster_compile_dataflow_graph", restype=ctypes.c_bool
        )
        self._add_node = bind_symbol(
            "rig_cluster_add_node",
            [ctypes.c_size_t, ctypes.c_size_t, ctypes.c_bool],
            ctypes.c_uint32,
        )
        self._add_python_node = bind_symbol(
            "rig_cluster_add_python_node",
            [ctypes.c_size_t, ctypes.c_size_t, ctypes.c_uint64, ctypes.c_bool],
            ctypes.c_uint32,
        )
        self._add_rust_runtime_model_node = bind_symbol(
            "rig_cluster_add_rust_runtime_model_node",
            [ctypes.c_bool],
            ctypes.c_uint32,
        )
        self._set_node_online = bind_symbol(
            "rig_cluster_set_node_online",
            [ctypes.c_uint32, ctypes.c_bool],
            ctypes.c_bool,
        )
        self._run_for = bind_symbol(
            "rig_cluster_run_for", [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_size_t]
        )
        self._add_scalar_route = bind_symbol(
            "rig_cluster_add_scalar_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_size_t,
            ],
            ctypes.c_bool,
        )
        self._add_scalar_sink_route = bind_symbol(
            "rig_cluster_add_scalar_sink_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_int32,
                ctypes.c_float,
                ctypes.c_size_t,
            ],
            ctypes.c_bool,
        )
        self._add_scalar_state_route = bind_symbol(
            "rig_cluster_add_scalar_state_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_int32,
                ctypes.c_float,
                ctypes.c_size_t,
            ],
            ctypes.c_bool,
        )
        self._add_scalar_state_sink = bind_symbol(
            "rig_cluster_add_scalar_state_sink",
            [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_float],
            ctypes.c_bool,
        )
        self._add_scalar_input_route = bind_symbol(
            "rig_cluster_add_scalar_input_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_uint32,
            ],
            ctypes.c_bool,
        )
        self._latest_scalar_event = bind_symbol(
            "rig_cluster_latest_scalar_event",
            [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._run_until_dataflow_wait = bind_symbol(
            "rig_cluster_run_until_dataflow_wait",
            [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_size_t, ctypes.c_uint64],
            ctypes.c_uint64,
        )
        self._cancel_dataflow_wait = bind_symbol(
            "rig_cluster_cancel_dataflow_wait", [ctypes.c_uint64]
        )
        self._noop_scalar_count = bind_symbol("rig_cluster_noop_scalar_count")
        self._noop_scalar_recv_many = bind_symbol("rig_cluster_noop_scalar_recv_many")
        self._noop_scalar_send_many = bind_symbol("rig_cluster_noop_scalar_send_many")
        self._elapsed_ns = bind_symbol(
            "rig_cluster_elapsed_ns", restype=ctypes.c_uint64
        )
        self._node_elapsed_ns = bind_symbol(
            "rig_cluster_node_elapsed_ns", [ctypes.c_uint32], ctypes.c_uint64
        )
        self._node_elapsed_ns_many = bind_symbol(
            "rig_cluster_node_elapsed_ns_many",
            [ctypes.POINTER(ctypes.c_uint64), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self.reset()

    def bind_symbol(
        self,
        name: str,
        argtypes: list[object] | None = None,
        restype: object | None = None,
    ):
        return self._host_bind_symbol(name, argtypes, restype)

    def reset(self) -> None:
        self._reset()
        self._node_indices.clear()
        self._node_owners.clear()

    def add_scalar_transform_algorithm(
        self,
        *,
        owner_node: str,
        sort_index: int,
        input_route_id: int,
        output_route_id: int,
    ) -> bool:
        try:
            node_index = self._node_indices[owner_node]
        except KeyError:
            return False
        return bool(
            self._add_scalar_transform_algorithm(
                ctypes.c_uint32(node_index),
                ctypes.c_uint32(sort_index),
                ctypes.c_uint32(input_route_id),
                ctypes.c_uint32(output_route_id),
            )
        )

    def compile_dataflow_graph(self) -> bool:
        return bool(self._compile_dataflow_graph())

    def add_node(self, name: str, node, *, online: bool = True) -> None:
        if getattr(node, "rust_runtime_model", lambda: False)():
            index = int(self._add_rust_runtime_model_node(ctypes.c_bool(online)))
        else:
            scheduler = node.scheduler_callbacks()
            if isinstance(scheduler, PythonSchedulerCallbacks):
                index = int(
                    self._add_python_node(
                        ctypes.c_size_t(scheduler.scheduled),
                        ctypes.c_size_t(scheduler.reset),
                        ctypes.c_uint64(scheduler.period_ns),
                        ctypes.c_bool(online),
                    )
                )
            elif isinstance(scheduler, RustSchedulerCallbacks):
                index = int(
                    self._add_node(
                        ctypes.c_size_t(scheduler.run_for),
                        ctypes.c_size_t(scheduler.reset),
                        ctypes.c_bool(online),
                    )
                )
            else:
                raise TypeError(
                    f"node {name!r} returned unsupported scheduler callbacks "
                    f"{type(scheduler).__name__}"
                )
        if index == 0xFFFFFFFF:
            raise RuntimeError(f"failed to register Rust cluster node {name!r}")
        self._node_indices[name] = index
        self._node_owners[name] = node

    def _node_pair(self, source_node: str, sink_node: str) -> tuple[int, int] | None:
        source_index = self._node_indices.get(source_node)
        sink_index = self._node_indices.get(sink_node)
        if source_index is None or sink_index is None:
            return None
        return source_index, sink_index

    def add_scalar_route(
        self,
        *,
        source_node: str,
        route_id: int,
        source_count: int,
        source_recv_many: int,
        sink_node: str,
        sink_send_many: int,
    ) -> bool:
        indices = self._node_pair(source_node, sink_node)
        if indices is None:
            return False
        source_index, sink_index = indices
        return bool(
            self._add_scalar_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint32(route_id),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_size_t(sink_send_many),
            )
        )

    def add_scalar_sink_route(
        self,
        *,
        source_node: str,
        route_id: int,
        source_count: int,
        source_recv_many: int,
        sink_node: str,
        sink_id: int,
        value_scale: float,
        set_value: int,
    ) -> bool:
        indices = self._node_pair(source_node, sink_node)
        if indices is None:
            return False
        source_index, sink_index = indices
        return bool(
            self._add_scalar_sink_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint32(route_id),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_int32(sink_id),
                ctypes.c_float(value_scale),
                ctypes.c_size_t(set_value),
            )
        )

    def add_scalar_input_route(
        self,
        *,
        source_node: str,
        source_route_id: int,
        source_count: int,
        source_recv_many: int,
        sink_node: str,
        sink_route_id: int,
    ) -> bool:
        indices = self._node_pair(source_node, sink_node)
        if indices is None:
            return False
        source_index, sink_index = indices
        return bool(
            self._add_scalar_input_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint32(source_route_id),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_uint32(sink_route_id),
            )
        )

    def add_scalar_state_route(
        self,
        *,
        source_node: str,
        route_id: int,
        source_count: int,
        source_recv_many: int,
        sink_node: str,
        sink_route_id: int,
        sink_id: int | None = None,
        value_scale: float = 1.0,
        set_value: int | None = None,
    ) -> bool:
        indices = self._node_pair(source_node, sink_node)
        if indices is None:
            return False
        source_index, sink_index = indices
        return bool(
            self._add_scalar_state_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint32(route_id),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_uint32(sink_route_id),
                ctypes.c_int32(-1 if sink_id is None else sink_id),
                ctypes.c_float(value_scale),
                ctypes.c_size_t(0 if set_value is None else set_value),
            )
        )

    def add_scalar_state_sink(
        self,
        *,
        node: str,
        route_id: int,
        initial_value: float,
    ) -> bool:
        node_index = self._node_indices.get(node)
        if node_index is None:
            return False
        return bool(
            self._add_scalar_state_sink(
                ctypes.c_uint32(node_index),
                ctypes.c_uint32(route_id),
                ctypes.c_float(initial_value),
            )
        )

    @property
    def noop_scalar_route_abi(self) -> tuple[int, int, int]:
        return (
            self._function_address(self._noop_scalar_count),
            self._function_address(self._noop_scalar_recv_many),
            self._function_address(self._noop_scalar_send_many),
        )

    def node_index(self, node: str) -> int | None:
        return self._node_indices.get(node)

    def run_for(self, duration_ns: int, step_ns: int, *, route: bool = True) -> None:
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        self._run_for(
            ctypes.c_uint64(duration_ns),
            ctypes.c_uint64(step_ns),
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
        return {
            name: int(values[index])
            for name, index in self._node_indices.items()
            if index < count
        }

    def latest_scalar_event(self, source_node: str, route_id: int, event) -> bool:
        index = self._node_indices.get(source_node)
        if index is None:
            return False
        return bool(
            self._latest_scalar_event(
                ctypes.c_uint32(index),
                ctypes.c_uint32(route_id),
                ctypes.byref(event),
            )
        )

    def run_until_dataflow_wait(
        self,
        handle: int,
        *,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None:
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        elapsed_ns = int(
            self._run_until_dataflow_wait(
                ctypes.c_uint64(timeout_ns),
                ctypes.c_uint64(step_ns),
                ctypes.c_size_t(route_callback or 0),
                ctypes.c_uint64(handle),
            )
        )
        return None if elapsed_ns == 0xFFFFFFFFFFFFFFFF else elapsed_ns

    def cancel_dataflow_wait(self, handle: int) -> None:
        self._cancel_dataflow_wait(ctypes.c_uint64(handle))

    def _route_callback_fn(self, elapsed_ns: int) -> None:
        if self._route is not None:
            self._route(int(elapsed_ns))

    @staticmethod
    def _function_address(function) -> int:
        value = ctypes.cast(function, ctypes.c_void_p).value
        if value is None:
            raise RuntimeError(f"could not resolve function pointer for {function!r}")
        return int(value)


__all__ = ["RustClusterRuntime", "RustRuntimeHost"]
