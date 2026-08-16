"""Firmware peripheral extension of the generic Rig Rust runtime.

Generic node registration, scalar dataflow, scheduling, waits, and clock
operations live in :mod:`rig.runtime`.  This module owns only firmware ABI
surfaces for CAN, timer, SPI, and firmware-specific composite routes.
"""

from __future__ import annotations

import ctypes
from collections.abc import Callable

from rig.dataflow import DataflowWait
from rig.runtime import RustClusterRuntime, RustRuntimeHost


class FirmwareRuntime(RustClusterRuntime):
    """Firmware binding layer over the generic Rust Rig runtime."""

    def __init__(
        self,
        *,
        host: object | None = None,
        route: Callable[[int], None] | None = None,
    ) -> None:
        super().__init__(host=host or RustRuntimeHost(), route=route)

        self._add_can_route = self.bind_symbol(
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
        self._latest_can_message = self.bind_symbol(
            "rig_cluster_latest_can_message",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_uint32, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._register_can_signal_wake = self.bind_symbol(
            "rig_cluster_register_can_signal_wake",
            [ctypes.c_void_p, ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._latest_can_bus_event = self.bind_symbol(
            "rig_cluster_latest_can_bus_event",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._latest_can_signal = self.bind_symbol(
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
        self._begin_can_signal_wait = self.bind_symbol(
            "rig_cluster_begin_can_signal_wait",
            [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_size_t],
            ctypes.c_uint64,
        )
        self._add_timer_route = self.bind_symbol(
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
        self._add_timer_source = self.bind_symbol(
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
        self._latest_timer_event = self.bind_symbol(
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
        self._add_spi_route = self.bind_symbol(
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
        self._add_periodic_can_source = self.bind_symbol(
            "rig_cluster_add_periodic_can_source",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_uint64, ctypes.c_void_p],
            ctypes.c_uint32,
        )
        self._update_periodic_can_source = self.bind_symbol(
            "rig_cluster_update_periodic_can_source",
            [ctypes.c_uint32, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._send_native_can_source_event = self.bind_symbol(
            "rig_cluster_send_native_can_source_event",
            [ctypes.c_uint32, ctypes.c_uint8, ctypes.c_void_p],
            ctypes.c_bool,
        )
        self._add_timer_scaled_scalar_source = self.bind_symbol(
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
        self._noop_timer_count = self.bind_symbol("rig_cluster_noop_timer_count")
        self._noop_timer_recv_many = self.bind_symbol("rig_cluster_noop_timer_recv_many")
        self._noop_timer_send_many = self.bind_symbol("rig_cluster_noop_timer_send_many")
        self._noop_can_tx_count = self.bind_symbol("rig_cluster_noop_can_tx_count")
        self._noop_can_recv_events = self.bind_symbol("rig_cluster_noop_can_recv_events")

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
        source_index = self.node_index(source_node)
        if source_index is None:
            return False
        sink_index = 0xFFFFFFFF if sink_node is None else self.node_index(sink_node)
        if sink_index is None:
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
        indices = self._node_pair(source_node, sink_node)
        if indices is None:
            return False
        source_index, sink_index = indices
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
        source_index = self.node_index(source_node)
        if source_index is None:
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
        indices = self._node_pair(source_node, sink_node)
        if indices is None:
            return False
        source_index, sink_index = indices
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

    def add_periodic_can_source(
        self,
        *,
        node: str,
        bus: int,
        period_ns: int,
        packet,
    ) -> int:
        node_index = self.node_index(node)
        if node_index is None:
            return 0xFFFFFFFF
        return int(
            self._add_periodic_can_source(
                ctypes.c_uint32(node_index),
                ctypes.c_uint8(bus),
                ctypes.c_uint64(period_ns),
                ctypes.byref(packet),
            )
        )

    def update_periodic_can_source(self, handle: int, packet) -> bool:
        return bool(
            self._update_periodic_can_source(
                ctypes.c_uint32(handle), ctypes.byref(packet)
            )
        )

    def send_native_can_source_event(self, *, node: str, bus: int, packet) -> bool:
        node_index = self.node_index(node)
        if node_index is None:
            return False
        return bool(
            self._send_native_can_source_event(
                ctypes.c_uint32(node_index),
                ctypes.c_uint8(bus),
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
        node_index = self.node_index(node)
        if node_index is None:
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

    @property
    def noop_can_source_route_abi(self) -> tuple[int, int]:
        return (
            self._function_address(self._noop_can_tx_count),
            self._function_address(self._noop_can_recv_events),
        )

    @property
    def noop_timer_route_abi(self) -> tuple[int, int, int]:
        return (
            self._function_address(self._noop_timer_count),
            self._function_address(self._noop_timer_recv_many),
            self._function_address(self._noop_timer_send_many),
        )

    def latest_can_message(
        self, source_node: str, bus: int, message_id: int, event
    ) -> bool:
        node_index = self.node_index(source_node)
        if node_index is None:
            return False
        return bool(
            self._latest_can_message(
                ctypes.c_uint32(node_index),
                ctypes.c_uint8(bus),
                ctypes.c_uint32(message_id),
                ctypes.byref(event),
            )
        )

    def register_can_signal_wake(self, wake, callback: int) -> bool:
        return bool(
            self._register_can_signal_wake(
                ctypes.byref(wake), ctypes.c_size_t(callback)
            )
        )

    def latest_can_bus_event(self, source_node: str, bus: int, event) -> bool:
        node_index = self.node_index(source_node)
        if node_index is None:
            return False
        return bool(
            self._latest_can_bus_event(
                ctypes.c_uint32(node_index),
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
        node_index = self.node_index(source_node)
        if node_index is None:
            return None
        value = ctypes.c_double()
        if not self._latest_can_signal(
            ctypes.c_uint32(node_index),
            ctypes.c_uint8(bus),
            ctypes.c_uint32(message_id),
            signal_name.encode(),
            ctypes.byref(value),
        ):
            return None
        return float(value.value)

    def begin_can_signal_wait(
        self,
        *,
        source_node: str,
        comparisons,
        comparison_count: int,
        decoder: int,
    ) -> DataflowWait | None:
        node_index = self.node_index(source_node)
        if node_index is None:
            return None
        handle = int(
            self._begin_can_signal_wait(
                ctypes.c_uint32(node_index),
                ctypes.cast(comparisons, ctypes.c_void_p),
                ctypes.c_uint32(comparison_count),
                ctypes.c_size_t(decoder),
            )
        )
        if handle == 0xFFFFFFFFFFFFFFFF:
            return None
        return DataflowWait(self, handle)

    def latest_timer_event(
        self, source_node: str, interface: int, port: int, channel: int, event
    ) -> bool:
        node_index = self.node_index(source_node)
        if node_index is None:
            return False
        return bool(
            self._latest_timer_event(
                ctypes.c_uint32(node_index),
                ctypes.c_uint16(interface),
                ctypes.c_int32(port),
                ctypes.c_int32(channel),
                ctypes.byref(event),
            )
        )


__all__ = ["FirmwareRuntime"]
