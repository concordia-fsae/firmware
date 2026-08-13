from __future__ import annotations

from collections.abc import Callable
from enum import IntEnum

from .can import CanEvent, CanInterface, CanMessageDescriptor, PeriodicCanMessage
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
        self.components.append(component)
        self._bind_component_datapaths(component)
        return component

    def reset(self) -> None:
        super().reset()
        for component in self.components:
            component.reset()

    def next_scheduler_step(self, duration: int | float, *, unit: str = "ms") -> int:
        duration_ns = duration_to_ns(duration, unit=unit)
        if not self.components:
            return duration_ns
        return min(
            component.next_scheduler_step(duration_ns, unit="ns")
            for component in self.components
        )

    def _run_for_from_runtime(self, duration_ns: int) -> None:
        self.elapsed_ns += duration_ns
        for component in self.components:
            component._run_for_from_runtime(duration_ns)

    def _fast_forward_for_from_runtime(self, duration_ns: int) -> None:
        self.elapsed_ns += duration_ns
        for component in self.components:
            component._fast_forward_for_from_runtime(duration_ns)

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


class _SimpleCanInterface:
    def __init__(self, model: SimpleCanComponent) -> None:
        self._model = model

    @property
    def buses(self):
        return self._model._buses

    def bus(self, bus):
        return self._model._encoder.bus(bus)


class SimpleCanComponent(SimpleComponent):
    """Python-only component that emits generated CAN messages."""

    def __init__(
        self,
        encoder: CanInterface,
        *,
        buses: tuple[str, ...] | list[str] = ("veh",),
    ) -> None:
        super().__init__(scheduler_period=1, scheduler_unit="ms")
        self._encoder = encoder
        self._buses = tuple(encoder.bus(bus) for bus in buses)
        self.can = _SimpleCanInterface(self)
        self._periodic_messages: list[PeriodicCanMessage] = []
        for bus in self._buses:
            self.add_egress_datapath(DataPath.can_bus(bus.name))

    def reset(self) -> None:
        super().reset()
        for periodic in self._periodic_messages:
            periodic.last_emit_ns = 0

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
        descriptor = (
            self._encoder.message(message, bus=bus)
            if isinstance(message, str)
            else message
        )
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
        descriptor = (
            self._encoder.message(message, bus=bus)
            if isinstance(message, str)
            else message
        )
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
        descriptor = (
            self._encoder.message(message, bus=bus)
            if isinstance(message, str)
            else message
        )
        defaults = {}
        enum_defaults = enum_defaults or {}
        for signal in self._encoder.rx_signals:
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

    def _run_scheduled(self) -> None:
        for periodic in self._periodic_messages:
            if self.elapsed_ns - periodic.last_emit_ns < periodic.period_ns:
                continue
            periodic.last_emit_ns = self.elapsed_ns
            self._queue_message(periodic.message, periodic.signals)

    def _queue_message(
        self,
        message: CanMessageDescriptor,
        signals: dict[str, float | int | IntEnum],
    ) -> None:
        packet = self._encoder.encode(message, **signals)
        self.emit_egress(
            DataPath.can_bus(message.bus_name),
            CanEvent.from_packet(message.bus, packet, timestamp_ns=self.elapsed_ns),
        )
