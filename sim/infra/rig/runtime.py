from __future__ import annotations

import ctypes
import os
import pathlib
from collections.abc import Callable

from .artifacts import buck_output, load_shared_library, repo_root
from .scheduler import PythonSchedulerCallbacks, RustSchedulerCallbacks


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
        self._add_scalar_transform_algorithm = bind_symbol(
            "rig_cluster_add_scalar_transform_algorithm",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
            ],
            ctypes.c_bool,
        )
        self._compile_dataflow_graph = bind_symbol(
            "rig_cluster_compile_dataflow_graph",
            restype=ctypes.c_bool,
        )
        self._add_node = bind_symbol(
            "rig_cluster_add_node",
            [
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_bool,
            ],
            ctypes.c_uint32,
        )
        self._add_python_node = bind_symbol(
            "rig_cluster_add_python_node",
            [
                ctypes.c_size_t,
                ctypes.c_size_t,
                ctypes.c_uint64,
                ctypes.c_bool,
            ],
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
            "rig_cluster_run_for",
            [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_size_t],
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
        self._latest_can_signal = bind_symbol(
            "rig_cluster_latest_can_signal",
            [
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_uint32,
                ctypes.c_char_p,
                ctypes.POINTER(ctypes.c_double),
            ],
            ctypes.c_bool,
        )
        self._run_until_can_signal_eq = bind_symbol(
            "rig_cluster_run_until_can_signal_eq",
            [
                ctypes.c_uint64,
                ctypes.c_uint64,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_uint32,
                ctypes.c_char_p,
                ctypes.c_double,
                ctypes.c_double,
            ],
            ctypes.c_uint64,
        )
        self._run_until_can_signal_index_eq = bind_symbol(
            "rig_cluster_run_until_can_signal_index_eq",
            [
                ctypes.c_uint64,
                ctypes.c_uint64,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_double,
                ctypes.c_double,
            ],
            ctypes.c_uint64,
        )
        self._run_until_can_signal_index_cmp = bind_symbol(
            "rig_cluster_run_until_can_signal_index_cmp",
            [
                ctypes.c_uint64,
                ctypes.c_uint64,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_double,
                ctypes.c_double,
                ctypes.c_uint8,
            ],
            ctypes.c_uint64,
        )
        self._run_until_can_signal_comparisons = bind_symbol(
            "rig_cluster_run_until_can_signal_comparisons",
            [
                ctypes.c_uint64,
                ctypes.c_uint64,
                ctypes.c_size_t,
                ctypes.c_uint32,
                ctypes.c_void_p,
                ctypes.c_uint32,
            ],
            ctypes.c_uint64,
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
        self._add_timer_source = bind_symbol(
            "rig_cluster_add_timer_source",
            [
                ctypes.c_uint32,
                ctypes.c_uint16,
                ctypes.c_int32,
                ctypes.c_int32,
                ctypes.c_size_t,
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
            ],
            ctypes.c_bool,
        )
        self._add_scalar_state_sink = bind_symbol(
            "rig_cluster_add_scalar_state_sink",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_float,
            ],
            ctypes.c_bool,
        )
        self._add_dc_load_voltage_route = bind_symbol(
            "rig_cluster_add_dc_load_voltage_route",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
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
        self._add_timer_scaled_scalar_source = bind_symbol(
            "rig_cluster_add_timer_scaled_scalar_source",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint16,
                ctypes.c_int32,
                ctypes.c_int32,
                ctypes.c_uint32,
                ctypes.c_float,
                ctypes.c_float,
            ],
            ctypes.c_bool,
        )
        self._add_battery_source = bind_symbol(
            "rig_cluster_add_battery_source",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
            ],
            ctypes.c_bool,
        )
        self._add_periodic_can_source = bind_symbol(
            "rig_cluster_add_periodic_can_source",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_uint64, ctypes.c_void_p],
            ctypes.c_uint32,
        )
        self._update_periodic_can_source = bind_symbol(
            "rig_cluster_update_periodic_can_source",
            [ctypes.c_uint32, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._send_native_can_source_event = bind_symbol(
            "rig_cluster_send_native_can_source_event",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._add_dc_load = bind_symbol(
            "rig_cluster_add_dc_load",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_uint64,
            ],
            ctypes.c_bool,
        )
        self._noop_timer_count = bind_symbol("rig_cluster_noop_timer_count")
        self._noop_timer_recv_many = bind_symbol("rig_cluster_noop_timer_recv_many")
        self._noop_timer_send_many = bind_symbol("rig_cluster_noop_timer_send_many")
        self._noop_can_tx_count = bind_symbol("rig_cluster_noop_can_tx_count")
        self._noop_can_recv_events = bind_symbol("rig_cluster_noop_can_recv_events")
        self._noop_scalar_count = bind_symbol("rig_cluster_noop_scalar_count")
        self._noop_scalar_recv_many = bind_symbol("rig_cluster_noop_scalar_recv_many")
        self._noop_scalar_send_many = bind_symbol("rig_cluster_noop_scalar_send_many")
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
            if index == 0xFFFFFFFF:
                raise RuntimeError(f"failed to register Rust cluster node {name!r}")
            self._node_indices[name] = index
            return

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

    def add_can_route(
        self,
        *,
        source_node: str,
        source_bus: int,
        source_tx_count: int,
        source_recv_events: int,
        sink_node: str | None = None,
        sink_bus: int = 0,
        sink_send_many: int = 0,
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
        except KeyError:
            return False
        if sink_node is None:
            sink_index = 0xFFFFFFFF
        else:
            try:
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

    def add_timer_source(
        self,
        *,
        source_node: str,
        interface: int,
        port: int,
        channel: int,
        source_count: int,
        source_recv_many: int,
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
        except KeyError:
            return False
        return bool(
            self._add_timer_source(
                ctypes.c_uint32(source_index),
                ctypes.c_uint16(interface),
                ctypes.c_int32(port),
                ctypes.c_int32(channel),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
            )
        )

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
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
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
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
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

    def add_dc_load_voltage_route(
        self,
        *,
        source_node: str,
        source_route_id: int,
        sink_node: str,
        sink_route_id: int,
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
        return bool(
            self._add_dc_load_voltage_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint32(source_route_id),
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
    ) -> bool:
        try:
            source_index = self._node_indices[source_node]
            sink_index = self._node_indices[sink_node]
        except KeyError:
            return False
        return bool(
            self._add_scalar_state_route(
                ctypes.c_uint32(source_index),
                ctypes.c_uint32(route_id),
                ctypes.c_size_t(source_count),
                ctypes.c_size_t(source_recv_many),
                ctypes.c_uint32(sink_index),
                ctypes.c_uint32(sink_route_id),
            )
        )

    def add_scalar_state_sink(
        self,
        *,
        node: str,
        route_id: int,
        initial_value: float,
    ) -> bool:
        try:
            node_index = self._node_indices[node]
        except KeyError:
            return False
        return bool(
            self._add_scalar_state_sink(
                ctypes.c_uint32(node_index),
                ctypes.c_uint32(route_id),
                ctypes.c_float(initial_value),
            )
        )

    def add_periodic_can_source(
        self,
        *,
        node: str,
        bus: int,
        period_ns: int,
        packet,
    ) -> int:
        try:
            node_index = self._node_indices[node]
        except KeyError:
            return 0xFFFFFFFF
        return int(
            self._add_periodic_can_source(
                ctypes.c_uint32(node_index),
                ctypes.c_uint8(bus),
                ctypes.c_uint64(period_ns),
                ctypes.byref(packet),
            )
        )

    def add_timer_scaled_scalar_source(
        self,
        *,
        node: str,
        route_id: int,
        timer_interface: int,
        timer_port: int,
        timer_channel: int,
        scale_route_id: int,
        scale: float,
        offset: float,
    ) -> bool:
        try:
            node_index = self._node_indices[node]
        except KeyError:
            return False
        return bool(
            self._add_timer_scaled_scalar_source(
                ctypes.c_uint32(node_index),
                ctypes.c_uint32(route_id),
                ctypes.c_uint16(timer_interface),
                ctypes.c_int32(timer_port),
                ctypes.c_int32(timer_channel),
                ctypes.c_uint32(scale_route_id),
                ctypes.c_float(scale),
                ctypes.c_float(offset),
            )
        )

    def add_battery_source(
        self,
        *,
        node: str,
        voltage_route_id: int,
        voltage: float,
        internal_resistance_ohms: float,
        capacity_amp_hours: float,
    ) -> bool:
        try:
            node_index = self._node_indices[node]
        except KeyError:
            return False
        return bool(
            self._add_battery_source(
                ctypes.c_uint32(node_index),
                ctypes.c_uint32(voltage_route_id),
                ctypes.c_float(voltage),
                ctypes.c_float(internal_resistance_ohms),
                ctypes.c_float(capacity_amp_hours),
            )
        )

    def update_periodic_can_source(self, handle: int, packet) -> bool:
        return bool(
            self._update_periodic_can_source(
                ctypes.c_uint32(handle),
                ctypes.byref(packet),
            )
        )

    def send_native_can_source_event(
        self,
        *,
        node: str,
        bus: int,
        packet,
    ) -> bool:
        try:
            node_index = self._node_indices[node]
        except KeyError:
            return False
        return bool(
            self._send_native_can_source_event(
                ctypes.c_uint32(node_index),
                ctypes.c_uint8(bus),
                ctypes.byref(packet),
            )
        )

    @property
    def noop_can_source_route_abi(self) -> tuple[int, int]:
        return (
            self._function_address(self._noop_can_tx_count),
            self._function_address(self._noop_can_recv_events),
        )

    def add_dc_load(
        self,
        *,
        node: str,
        voltage_route_id: int,
        current_route_id: int,
        resistance_ohms: float,
        inductance_henrys: float,
        capacitance_farads: float,
        scheduler_period_ns: int,
    ) -> bool:
        try:
            node_index = self._node_indices[node]
        except KeyError:
            return False
        return bool(
            self._add_dc_load(
                ctypes.c_uint32(node_index),
                ctypes.c_uint32(voltage_route_id),
                ctypes.c_uint32(current_route_id),
                ctypes.c_float(resistance_ohms),
                ctypes.c_float(inductance_henrys),
                ctypes.c_float(capacitance_farads),
                ctypes.c_uint64(scheduler_period_ns),
            )
        )

    @property
    def noop_timer_route_abi(self) -> tuple[int, int, int]:
        return (
            self._function_address(self._noop_timer_count),
            self._function_address(self._noop_timer_recv_many),
            self._function_address(self._noop_timer_send_many),
        )

    @property
    def noop_scalar_route_abi(self) -> tuple[int, int, int]:
        return (
            self._function_address(self._noop_scalar_count),
            self._function_address(self._noop_scalar_recv_many),
            self._function_address(self._noop_scalar_send_many),
        )

    def run_for(
        self,
        duration_ns: int,
        step_ns: int,
        *,
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

    def latest_can_signal(
        self,
        source_node: str,
        bus: int,
        message_id: int,
        signal_name: str,
    ) -> float | None:
        index = self._node_indices.get(source_node)
        if index is None:
            return None
        value = ctypes.c_double()
        if not self._latest_can_signal(
            ctypes.c_uint32(index),
            ctypes.c_uint8(bus),
            ctypes.c_uint32(message_id),
            signal_name.encode(),
            ctypes.byref(value),
        ):
            return None
        return float(value.value)

    def run_until_can_signal_eq(
        self,
        *,
        source_node: str,
        bus: int,
        message_id: int,
        signal_name: str,
        expected: float,
        tolerance: float,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None:
        index = self._node_indices.get(source_node)
        if index is None:
            return None
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        elapsed_ns = int(
            self._run_until_can_signal_eq(
                ctypes.c_uint64(timeout_ns),
                ctypes.c_uint64(step_ns),
                ctypes.c_size_t(route_callback or 0),
                ctypes.c_uint32(index),
                ctypes.c_uint8(bus),
                ctypes.c_uint32(message_id),
                signal_name.encode(),
                ctypes.c_double(expected),
                ctypes.c_double(tolerance),
            )
        )
        return None if elapsed_ns == 0xFFFFFFFFFFFFFFFF else elapsed_ns

    def run_until_can_signal_index_eq(
        self,
        *,
        source_node: str,
        bus: int,
        message_id: int,
        signal_index: int,
        expected: float,
        tolerance: float,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None:
        index = self._node_indices.get(source_node)
        if index is None:
            return None
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        elapsed_ns = int(
            self._run_until_can_signal_index_eq(
                ctypes.c_uint64(timeout_ns),
                ctypes.c_uint64(step_ns),
                ctypes.c_size_t(route_callback or 0),
                ctypes.c_uint32(index),
                ctypes.c_uint8(bus),
                ctypes.c_uint32(message_id),
                ctypes.c_uint32(signal_index),
                ctypes.c_double(expected),
                ctypes.c_double(tolerance),
            )
        )
        return None if elapsed_ns == 0xFFFFFFFFFFFFFFFF else elapsed_ns

    def run_until_can_signal_index_cmp(
        self,
        *,
        source_node: str,
        bus: int,
        message_id: int,
        signal_index: int,
        expected: float,
        tolerance: float,
        comparison: int,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None:
        index = self._node_indices.get(source_node)
        if index is None:
            return None
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        elapsed_ns = int(
            self._run_until_can_signal_index_cmp(
                ctypes.c_uint64(timeout_ns),
                ctypes.c_uint64(step_ns),
                ctypes.c_size_t(route_callback or 0),
                ctypes.c_uint32(index),
                ctypes.c_uint8(bus),
                ctypes.c_uint32(message_id),
                ctypes.c_uint32(signal_index),
                ctypes.c_double(expected),
                ctypes.c_double(tolerance),
                ctypes.c_uint8(comparison),
            )
        )
        return None if elapsed_ns == 0xFFFFFFFFFFFFFFFF else elapsed_ns

    def run_until_can_signal_comparisons(
        self,
        *,
        source_node: str,
        comparisons,
        comparison_count: int,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None:
        index = self._node_indices.get(source_node)
        if index is None:
            return None
        route_callback = (
            ctypes.cast(self._route_callback, ctypes.c_void_p).value
            if route and self._route is not None
            else 0
        )
        elapsed_ns = int(
            self._run_until_can_signal_comparisons(
                ctypes.c_uint64(timeout_ns),
                ctypes.c_uint64(step_ns),
                ctypes.c_size_t(route_callback or 0),
                ctypes.c_uint32(index),
                ctypes.cast(comparisons, ctypes.c_void_p),
                ctypes.c_uint32(comparison_count),
            )
        )
        return None if elapsed_ns == 0xFFFFFFFFFFFFFFFF else elapsed_ns

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

    def latest_spi_transaction(
        self, source_node: str, device: int, transaction
    ) -> bool:
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

    def _route_callback_fn(self, elapsed_ns: int) -> None:
        if self._route is not None:
            self._route(int(elapsed_ns))

    @staticmethod
    def _function_address(function) -> int:
        value = ctypes.cast(function, ctypes.c_void_p).value
        if value is None:
            raise RuntimeError(f"could not resolve function pointer for {function!r}")
        return int(value)
