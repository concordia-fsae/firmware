from __future__ import annotations

import ctypes
from collections.abc import Callable
from enum import IntEnum

from .can import (
    CanEvent,
    CanInterface,
    CanMessageDescriptor,
    CanPacket,
    PeriodicCanMessage,
)
from .datapath import DataPath, datapath_key
from .model import ComponentRig, ModelRig
from .time import duration_to_ns


SimpleDataPathHandler = Callable[[object], None]


class SimpleComponent(ComponentRig):
    """Python-only component with generic ingress and egress datapaths."""

    def __init__(
        self,
        *,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
    ) -> None:
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_unit=scheduler_unit,
        )
        self._ingress_events: dict[str, list[object]] = {}
        self._ingress_paths: dict[str, DataPath] = {}
        self._egress_events: dict[str, list[object]] = {}
        self._egress_paths: dict[str, DataPath] = {}

    @property
    def ingress_datapaths(self) -> tuple[DataPath, ...]:
        return tuple(self._ingress_paths.values())

    @property
    def egress_datapaths(self) -> tuple[DataPath, ...]:
        return tuple(self._egress_paths.values())

    def reset(self) -> None:
        super().reset()
        for events in self._ingress_events.values():
            events.clear()
        for events in self._egress_events.values():
            events.clear()

    def add_ingress_datapath(
        self,
        path: DataPath,
        *,
        handler: SimpleDataPathHandler | None = None,
    ) -> DataPath:
        key = datapath_key(path)
        self._ingress_paths.setdefault(key, path)
        self._ingress_events.setdefault(key, [])
        self.datapaths.add_input(
            path,
            send=lambda payload, path=path, handler=handler: self._send_ingress(
                path,
                payload,
                handler=handler,
            ),
            send_many=lambda payloads,
            path=path,
            handler=handler: self._send_ingress_many(
                path,
                payloads,
                handler=handler,
            ),
        )
        return path

    def add_egress_datapath(self, path: DataPath) -> DataPath:
        key = datapath_key(path)
        self._egress_paths.setdefault(key, path)
        self._egress_events.setdefault(key, [])
        self.datapaths.add_output(
            path,
            pending=lambda path=path: self.egress_count(path),
            recv=lambda path=path: self.recv_egress(path),
            recv_many=lambda count, path=path: self.recv_egress_many(path, count),
        )
        return path

    def emit_egress(self, path: DataPath, payload: object) -> bool:
        key = datapath_key(path)
        if key not in self._egress_events:
            raise ValueError(f"egress datapath {path!r} is not configured")
        self._egress_events[key].append(payload)
        return True

    def ingress_events(self, path: DataPath) -> tuple[object, ...]:
        return tuple(self._events_for(self._ingress_events, path, "ingress"))

    def latest_ingress(self, path: DataPath) -> object | None:
        events = self._events_for(self._ingress_events, path, "ingress")
        return events[-1] if events else None

    def egress_count(self, path: DataPath) -> int:
        return len(self._events_for(self._egress_events, path, "egress"))

    def recv_egress(self, path: DataPath) -> object | None:
        events = self._events_for(self._egress_events, path, "egress")
        return events.pop(0) if events else None

    def recv_egress_many(self, path: DataPath, count: int) -> tuple[object, ...]:
        events = self._events_for(self._egress_events, path, "egress")
        payloads = tuple(events[:count])
        del events[:count]
        return payloads

    def _send_ingress(
        self,
        path: DataPath,
        payload: object,
        *,
        handler: SimpleDataPathHandler | None,
    ) -> bool:
        key = datapath_key(path)
        if key not in self._ingress_events:
            raise ValueError(f"ingress datapath {path!r} is not configured")
        self._ingress_events[key].append(payload)
        if handler is not None:
            handler(payload)
        return True

    def _send_ingress_many(
        self,
        path: DataPath,
        payloads: tuple[object, ...],
        *,
        handler: SimpleDataPathHandler | None,
    ) -> int:
        for payload in payloads:
            self._send_ingress(path, payload, handler=handler)
        return len(payloads)

    @staticmethod
    def _events_for(
        events_by_path: dict[str, list[object]],
        path: DataPath,
        direction: str,
    ) -> list[object]:
        key = datapath_key(path)
        try:
            return events_by_path[key]
        except KeyError as exc:
            raise ValueError(
                f"{direction} datapath {path!r} is not configured"
            ) from exc


class SimpleNodeRig(ModelRig):
    """Python-only node composed from arbitrary simple components."""

    def __init__(self, *components: ComponentRig) -> None:
        super().__init__()
        self.components: list[ComponentRig] = []
        for component in components:
            self.add_component(component)

    def add_component(self, component: ComponentRig) -> ComponentRig:
        if self._cluster_rig is not None:
            raise RuntimeError("simple components must be added before clustering")
        if getattr(component, "_scheduler_period_ns", None) is not None:
            raise ValueError(
                "scheduled components must be added to a ClusterRig directly so "
                "Rust can schedule each component independently"
            )
        self.components.append(component)
        self._bind_component_interfaces(component)
        self._bind_component_datapaths(component)
        return component

    def reset(self) -> None:
        super().reset()
        for component in self.components:
            component.reset()

    def _bind_component_datapaths(self, component: ComponentRig) -> None:
        for output in component.datapaths.outputs():
            self.datapaths.add_output(
                output.path,
                pending=output.pending,
                recv=output.recv,
                recv_many=output.recv_many,
            )
        for input_ in component.datapaths.inputs():
            self.datapaths.add_input(
                input_.path,
                send=input_.send,
                send_many=input_.send_many,
            )

    def _bind_component_interfaces(self, component: ComponentRig) -> None:
        if hasattr(component, "can"):
            if hasattr(self, "can"):
                raise ValueError("SimpleNodeRig can only expose one CAN interface")
            self.can = component.can

    def rust_can_route_abi(self, bus) -> tuple[int, int, int, int] | None:
        for component in self.components:
            component._cluster_rig = self._cluster_rig
            component._cluster_node_name = self._cluster_node_name
            route_abi = getattr(component, "rust_can_route_abi", lambda _bus: None)(bus)
            if route_abi is not None:
                return route_abi
        return None


class _SimpleCanInterface:
    def __init__(self, model: SimpleCanComponent) -> None:
        self._model = model

    @property
    def buses(self):
        return self._model._buses

    @property
    def enums(self):
        return self._model._encoder.enums

    def bus(self, bus):
        return self._model._encoder.bus(bus)

    def message(self, name, *, bus=None, tx=False):
        return self._model._encoder.message(name, bus=bus, tx=tx)

    def tx_message(self, name, *, bus=None):
        return self._model._encoder.tx_message(name, bus=bus)


class SimpleCanComponent(SimpleComponent):
    """Python-only component that emits generated CAN messages."""

    _CanTxCountCallback = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_uint8)
    _CanRecvEventsCallback = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.c_uint8,
        ctypes.POINTER(CanEvent),
        ctypes.c_uint32,
    )
    _CanSendManyCallback = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.c_uint8,
        ctypes.POINTER(CanPacket),
        ctypes.c_uint32,
    )

    def __init__(
        self,
        encoder: CanInterface,
        *,
        buses: tuple[str, ...] | list[str] = ("veh",),
    ) -> None:
        super().__init__()
        self._encoder = encoder
        self._buses = tuple(encoder.bus(bus) for bus in buses)
        self._bus_paths = {bus.name: DataPath.can_bus(bus.name) for bus in self._buses}
        self.can = _SimpleCanInterface(self)
        self._periodic_messages: list[PeriodicCanMessage] = []
        self._native_periodic_handles: dict[int, int] = {}
        for path in self._bus_paths.values():
            self.add_egress_datapath(path)
        self._can_tx_count_callback = self._CanTxCountCallback(self._ffi_can_tx_count)
        self._can_recv_events_callback = self._CanRecvEventsCallback(
            self._ffi_can_recv_events
        )
        self._can_send_many_callback = self._CanSendManyCallback(
            self._ffi_can_send_many
        )

    def reset(self) -> None:
        super().reset()
        for periodic in self._periodic_messages:
            periodic.last_emit_ns = 0
        self._native_periodic_handles.clear()
        for periodic in self._periodic_messages:
            periodic.native_update = None

    def periodic_send(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: str = "veh",
        period: int | float = 100,
        unit: str = "ms",
        enum_defaults: dict[str, str] | None = None,
        **signals: float | int | IntEnum,
    ) -> PeriodicCanMessage:
        descriptor = self._message_descriptor(message, bus=bus)
        if DataPath.can_bus(descriptor.bus_name) not in self.egress_datapaths:
            raise ValueError(
                f"simple CAN model is not configured for bus {descriptor.bus_name!r}"
            )
        signals = self.signals_for_message(
            descriptor,
            enum_defaults=enum_defaults,
            **signals,
        )
        periodic = PeriodicCanMessage(
            message=descriptor,
            period_ns=duration_to_ns(period, unit=unit),
            signals=dict(signals),
            encoder=lambda message, signals: self._encoder.encode(message, **signals),
        )
        self._periodic_messages.append(periodic)
        return periodic

    def send(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: str = "veh",
        enum_defaults: dict[str, str] | None = None,
        **signals: float | int | IntEnum,
    ) -> bool:
        descriptor = self._message_descriptor(message, bus=bus)
        self._queue_message(
            descriptor,
            self.signals_for_message(
                descriptor,
                enum_defaults=enum_defaults,
                **signals,
            ),
        )
        return True

    def signals_for_message(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: str = "veh",
        enum_defaults: dict[str, str] | None = None,
        **signals: float | int | IntEnum,
    ) -> dict[str, float | int | IntEnum]:
        descriptor = self._message_descriptor(message, bus=bus)
        defaults = {}
        enum_defaults = enum_defaults or {}
        for signal in (*self._encoder.tx_signals, *self._encoder.rx_signals):
            if signal.bus != descriptor.bus or signal.message_name != descriptor.name:
                continue
            if signal.kind == "Enum" and signal.enum_name in enum_defaults:
                enum_type = self._encoder.enums[signal.enum_name]
                defaults[signal.signal_name] = getattr(
                    enum_type,
                    enum_defaults[signal.enum_name],
                )
        defaults.update(signals)
        return defaults

    def _message_descriptor(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: str = "veh",
    ) -> CanMessageDescriptor:
        if not isinstance(message, str):
            return message
        try:
            return self._encoder.tx_message(message, bus=bus)
        except (KeyError, ValueError):
            return self._encoder.message(message, bus=bus)

    def _queue_message(
        self,
        message: CanMessageDescriptor,
        signals: dict[str, float | int | IntEnum],
    ) -> None:
        self._emit_packet(message, self._encoder.encode(message, **signals))

    def _emit_packet(
        self,
        message: CanMessageDescriptor,
        packet,
    ) -> None:
        if (
            self._cluster_rig is not None
            and self._cluster_node_name is not None
            and self._cluster_rig._rust_runtime.send_native_can_source_event(
                node=self._cluster_node_name,
                bus=message.bus,
                packet=packet,
            )
        ):
            return
        self.emit_egress(
            self._bus_paths[message.bus_name],
            CanEvent.from_packet(message.bus, packet, timestamp_ns=self.elapsed_ns),
        )

    def rust_can_route_abi(self, bus) -> tuple[int, int, int, int] | None:
        try:
            bus_descriptor = self._encoder.bus(bus)
        except (KeyError, ValueError):
            return None
        if bus_descriptor.name not in self._bus_paths:
            return None
        self._register_native_periodic_messages(bus_descriptor)
        if self._cluster_rig is not None:
            source_tx_count, source_recv_events = (
                self._cluster_rig._rust_runtime.noop_can_source_route_abi
            )
        else:
            source_tx_count = self._callback_address(self._can_tx_count_callback)
            source_recv_events = self._callback_address(self._can_recv_events_callback)
        return (
            bus_descriptor.index,
            source_tx_count,
            source_recv_events,
            self._callback_address(self._can_send_many_callback),
        )

    def _register_native_periodic_messages(self, bus_descriptor) -> None:
        cluster = self._cluster_rig
        node_name = self._cluster_node_name
        if cluster is None or node_name is None:
            return
        for periodic in self._periodic_messages:
            if periodic.message.bus != bus_descriptor.index:
                continue
            key = id(periodic)
            if key in self._native_periodic_handles:
                continue
            handle = cluster._rust_runtime.add_periodic_can_source(
                node=node_name,
                bus=bus_descriptor.index,
                period_ns=periodic.period_ns,
                packet=periodic.packet,
            )
            if handle == 0xFFFFFFFF:
                raise RuntimeError(
                    f"failed to register periodic CAN source {periodic.message.name!r}"
                )
            self._native_periodic_handles[key] = handle
            periodic.native_update = (
                lambda packet,
                handle=handle,
                cluster=cluster: cluster._rust_runtime.update_periodic_can_source(
                    handle,
                    packet,
                )
            )

    def _ffi_can_tx_count(self, bus_index: int) -> int:
        bus = self._bus_descriptor_for_index(bus_index)
        if bus is None:
            return 0
        return self.egress_count(self._bus_paths[bus.name])

    def _ffi_can_recv_events(
        self,
        bus_index: int,
        events,
        capacity: int,
    ) -> int:
        bus = self._bus_descriptor_for_index(bus_index)
        if bus is None or capacity == 0:
            return 0
        payloads = self.recv_egress_many(self._bus_paths[bus.name], int(capacity))
        count = 0
        for payload in payloads:
            if not isinstance(payload, CanEvent):
                continue
            events[count] = payload
            count += 1
        return count

    def _ffi_can_send_many(self, bus_index: int, packets, count: int) -> int:
        bus = self._bus_descriptor_for_index(bus_index)
        if bus is None:
            return 0
        path = self._bus_paths[bus.name]
        for index in range(int(count)):
            self._send_ingress(
                path,
                CanEvent.from_packet(
                    bus.index,
                    packets[index],
                    timestamp_ns=self.elapsed_ns,
                ),
                handler=None,
            )
        return int(count)

    def _bus_descriptor_for_index(self, bus_index: int):
        for bus in self._buses:
            if bus.index == int(bus_index):
                return bus
        return None

    @staticmethod
    def _callback_address(callback) -> int:
        value = ctypes.cast(callback, ctypes.c_void_p).value
        if value is None:
            raise RuntimeError(f"could not resolve callback pointer for {callback!r}")
        return int(value)
