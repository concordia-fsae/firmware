from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum

from rig.datapath import DataPath
from sim.bindings.core.peripheral import (
    PeripheralBinding,
    PeripheralInterface,
    peripheral_datapath,
    require_peripheral_binding,
)


@dataclass(frozen=True)
class TimerRouteEndpoint:
    interface: int
    port: int
    channel: int
    count: int
    recv_many: int
    send_many: int

    @property
    def scalar_source_route_id(self) -> None:
        return None

    def compatible_with(self, sink: object) -> bool:
        return isinstance(sink, TimerRouteEndpoint) and (
            self.interface,
            self.port,
            self.channel,
        ) == (sink.interface, sink.port, sink.channel)

    def connect(
        self, runtime: object, *, source_node: str, sink_node: str, sink: object
    ) -> bool:
        if not isinstance(sink, TimerRouteEndpoint) or not self.compatible_with(sink):
            return False
        return runtime.add_timer_route(
            source_node=source_node,
            interface=self.interface,
            port=self.port,
            channel=self.channel,
            source_count=self.count,
            source_recv_many=self.recv_many,
            sink_node=sink_node,
            sink_send_many=sink.send_many,
        )


class TimerChannelEvent(ctypes.Structure):
    _fields_ = [
        ("port", ctypes.c_int32),
        ("channel", ctypes.c_int32),
        ("value", ctypes.c_float),
        ("timestamp_ns", ctypes.c_uint64),
    ]


class TimerCaptureEvent(ctypes.Structure):
    _fields_ = [
        ("channel", ctypes.c_int32),
        ("value", ctypes.c_float),
        ("timestamp_ns", ctypes.c_uint64),
    ]


class TimerPeripheralInterface:
    _TIMER_DUTY = PeripheralInterface.TIMER_DUTY
    _TIMER_FREQUENCY = PeripheralInterface.TIMER_FREQUENCY
    _TIMER_CAPTURE = PeripheralInterface.TIMER_CAPTURE

    def __init__(self, model: FirmwareNodeRig) -> None:
        self._model = model

    @classmethod
    def timer_duty_events(cls, port: object, channel: object) -> DataPath:
        return cls._timer_events(
            cls._TIMER_DUTY,
            PeripheralInterface.TIMER_DUTY,
            port,
            channel,
        )

    @classmethod
    def timer_frequency_events(cls, port: object, channel: object) -> DataPath:
        return cls._timer_events(
            cls._TIMER_FREQUENCY,
            PeripheralInterface.TIMER_FREQUENCY,
            port,
            channel,
        )

    @classmethod
    def timer_capture_events(cls, channel: object) -> DataPath:
        return peripheral_datapath(
            DataPath.named(cls._TIMER_CAPTURE, channel),
            PeripheralBinding(
                cls._TIMER_CAPTURE,
                channel=int(channel),
            ),
        )

    @classmethod
    def _timer_events(
        cls,
        interface: PeripheralInterface,
        event: PeripheralInterface,
        port: object,
        channel: object,
    ) -> DataPath:
        return peripheral_datapath(
            DataPath.named(event, port, channel),
            PeripheralBinding(
                interface,
                channel=int(channel),
                port=int(port),
            ),
        )

    @classmethod
    def supports(cls, path: DataPath) -> bool:
        binding = require_peripheral_binding(path)
        return binding.interface in (
            cls._TIMER_DUTY,
            cls._TIMER_FREQUENCY,
            cls._TIMER_CAPTURE,
        )

    def send(
        self,
        path: DataPath,
        *,
        channel: int = 0,
        value: float = 0.0,
        timestamp_ns: int = 0,
    ) -> bool:
        binding = require_peripheral_binding(path)
        if binding.interface == self._TIMER_CAPTURE:
            event = TimerCaptureEvent()
            event.channel = int(
                binding.channel if binding.channel is not None else channel
            )
            event.value = float(value)
            event.timestamp_ns = int(timestamp_ns)
            return bool(self._model._timer_send_capture(event))

        event = TimerChannelEvent()
        event.port = int(binding.port if binding.port is not None else 0)
        event.channel = int(binding.channel if binding.channel is not None else channel)
        event.value = float(value)
        event.timestamp_ns = int(timestamp_ns)
        return self._send_event(binding, event)

    def send_payload(self, path: DataPath, payload: object) -> bool:
        if require_peripheral_binding(path).interface == self._TIMER_CAPTURE:
            if not isinstance(payload, TimerCaptureEvent):
                raise TypeError(
                    "timer capture datapaths require TimerCaptureEvent payloads, "
                    f"got {type(payload).__name__}"
                )
            return bool(self._model._timer_send_capture(payload))

        if not isinstance(payload, TimerChannelEvent):
            raise TypeError(
                f"timer datapaths require TimerChannelEvent payloads, got {type(payload).__name__}"
            )
        return self._send_event(require_peripheral_binding(path), payload)

    def send_payloads(self, path: DataPath, payloads: tuple[object, ...]) -> int:
        if not payloads:
            return 0
        binding = require_peripheral_binding(path)
        if binding.interface == self._TIMER_CAPTURE:
            raise ValueError("timer capture datapaths do not support batch sends")
        events = (TimerChannelEvent * len(payloads))()
        for index, payload in enumerate(payloads):
            if not isinstance(payload, TimerChannelEvent):
                raise TypeError(
                    f"timer datapaths require TimerChannelEvent payloads, got {type(payload).__name__}"
                )
            events[index] = payload
        send_many = self._send_many_symbol(binding)
        return int(send_many(events, ctypes.c_uint32(len(payloads))))

    def recv(self, path: DataPath) -> TimerChannelEvent | None:
        binding = require_peripheral_binding(path)
        event = TimerChannelEvent()
        recv = self._recv_symbol(binding)
        if recv(
            ctypes.c_int(binding.port if binding.port is not None else 0),
            ctypes.c_int(binding.channel if binding.channel is not None else 0),
            ctypes.byref(event),
        ):
            return event
        return None

    def recv_many(
        self,
        path: DataPath,
        capacity: int,
    ) -> tuple[TimerChannelEvent, ...]:
        if capacity <= 0:
            return ()
        binding = require_peripheral_binding(path)
        events = (TimerChannelEvent * capacity)()
        recv_many = self._recv_many_symbol(binding)
        count = int(
            recv_many(
                ctypes.c_int(binding.port if binding.port is not None else 0),
                ctypes.c_int(binding.channel if binding.channel is not None else 0),
                events,
                ctypes.c_uint32(capacity),
            )
        )
        return tuple(events[index] for index in range(count))

    def output_count(self, path: DataPath) -> int:
        binding = require_peripheral_binding(path)
        count = self._count_symbol(binding)
        return int(
            count(
                ctypes.c_int(binding.port if binding.port is not None else 0),
                ctypes.c_int(binding.channel if binding.channel is not None else 0),
            )
        )

    def _send_event(self, binding: PeripheralBinding, event: TimerChannelEvent) -> bool:
        if binding.interface == self._TIMER_DUTY:
            return bool(self._model._timer_send_duty(ctypes.byref(event)))
        if binding.interface == self._TIMER_FREQUENCY:
            return bool(self._model._timer_send_frequency(ctypes.byref(event)))
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _send_many_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_send_duties
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_send_frequencies
        raise ValueError("timer capture datapaths do not support batch sends")

    def _recv_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_recv_duty
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_recv_frequency
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _recv_many_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_recv_duties
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_recv_frequencies
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _count_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_duty_output_count
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_frequency_output_count
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def rust_route_abi(self, path: DataPath) -> TimerRouteEndpoint:
        binding = require_peripheral_binding(path)
        if binding.interface == self._TIMER_CAPTURE:
            raise ValueError("timer capture datapaths cannot be routed between nodes")
        return TimerRouteEndpoint(
            interface=int(binding.interface),
            port=int(binding.port if binding.port is not None else 0),
            channel=int(binding.channel if binding.channel is not None else 0),
            count=self._model._function_address(self._count_symbol(binding)),
            recv_many=self._model._function_address(self._recv_many_symbol(binding)),
            send_many=self._model._function_address(self._send_many_symbol(binding)),
        )


class TimerInterface:
    def __init__(
        self,
        port_enum: type[IntEnum] | None = None,
        channel_enum: type[IntEnum] | None = None,
    ) -> None:
        self._port_enum = port_enum
        self._channel_enum = channel_enum

    def duty_events(self, port: object, channel: object) -> DataPath:
        port, channel = self._coerce(port, channel)
        return TimerPeripheralInterface.timer_duty_events(port, channel)

    def frequency_events(self, port: object, channel: object) -> DataPath:
        port, channel = self._coerce(port, channel)
        return TimerPeripheralInterface.timer_frequency_events(port, channel)

    def capture_events(self, channel: object) -> DataPath:
        channel = self._coerce_enum(self._channel_enum, channel, "timer channel")
        return TimerPeripheralInterface.timer_capture_events(channel)

    def _coerce(
        self, port: object, channel: object
    ) -> tuple[IntEnum | int, IntEnum | int]:
        return (
            self._coerce_enum(self._port_enum, port, "timer port"),
            self._coerce_enum(self._channel_enum, channel, "timer channel"),
        )

    @staticmethod
    def _coerce_enum(
        enum_type: type[IntEnum] | None, value: object, label: str
    ) -> IntEnum | int:
        if enum_type is None:
            return int(value)  # type: ignore[arg-type]
        if isinstance(value, enum_type):
            return value
        try:
            return enum_type(value)
        except ValueError as exc:
            raise ValueError(
                f"{value!r} is not a valid {label} for {enum_type.__name__}"
            ) from exc
