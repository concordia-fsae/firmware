from __future__ import annotations

import ctypes
import os
import pathlib
from collections.abc import Callable

from .artifacts import buck_output, load_shared_library, repo_root


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
            [ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_bool],
            ctypes.c_uint32,
        )
        self._set_node_online = bind_symbol(
            "rig_cluster_set_node_online",
            [ctypes.c_uint32, ctypes.c_bool],
            ctypes.c_bool,
        )
        self._run_for = bind_symbol(
            "rig_cluster_run_for",
            [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_size_t],
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
        run_for, next_step, reset = node.rust_cluster_node_abi()
        index = int(
            self._add_node(
                ctypes.c_size_t(run_for),
                ctypes.c_size_t(next_step),
                ctypes.c_size_t(reset),
                ctypes.c_bool(online),
            )
        )
        if index == 0xFFFFFFFF:
            raise RuntimeError(f"failed to register Rust cluster node {name!r}")
        self._node_indices[name] = index

    def run_for(self, duration_ns: int, step_ns: int) -> None:
        self._run_for(
            ctypes.c_uint64(duration_ns),
            ctypes.c_uint64(step_ns),
            ctypes.c_size_t(
                ctypes.cast(self._route_callback, ctypes.c_void_p).value or 0
            ),
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

    def _route_callback_fn(self, elapsed_ns: int) -> None:
        if self._route is not None:
            self._route(int(elapsed_ns))
