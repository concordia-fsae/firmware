from __future__ import annotations

import ctypes
from collections.abc import Callable, Iterator, Mapping
from dataclasses import dataclass, field
from enum import IntEnum

from .time import RunUntilTimeout, duration_to_ns


SIGNAL_KIND_NAMES = {
    0: "Numeric",
    1: "Boolean",
    2: "Enum",
}

_COMPARE_EQ = 0
_COMPARE_GT = 1
_COMPARE_GE = 2
_COMPARE_LT = 3
_COMPARE_LE = 4


class CanPacket(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_uint32),
        ("len", ctypes.c_uint8),
        ("data", ctypes.c_uint8 * 8),
    ]

    @classmethod
    def from_payload(
        cls, frame_id: int, payload: bytes | bytearray | list[int] | tuple[int, ...]
    ):
        payload_bytes = bytes(payload)
        if len(payload_bytes) > 8:
            raise ValueError(
                f"CAN payload must be at most 8 bytes, got {len(payload_bytes)}"
            )

        packet = cls()
        packet.id = ctypes.c_uint32(frame_id).value
        packet.len = len(payload_bytes)
        for index, value in enumerate(payload_bytes):
            packet.data[index] = value
        return packet

    @property
    def payload(self) -> bytes:
        return bytes(self.data[: self.len])


class CanEvent(ctypes.Structure):
    _fields_ = [
        ("bus", ctypes.c_uint8),
        ("timestamp_ns", ctypes.c_uint64),
        ("packet", CanPacket),
    ]

    @classmethod
    def from_packet(cls, bus: int, packet: CanPacket, *, timestamp_ns: int = 0):
        event = cls()
        event.bus = int(bus)
        event.timestamp_ns = int(timestamp_ns)
        event.packet = packet
        return event


@dataclass(frozen=True)
class CanBusDescriptor:
    index: int
    name: str


@dataclass(frozen=True)
class CanMessageDescriptor:
    bus: int
    bus_name: str
    name: str
    id: int
    len: int


@dataclass(frozen=True)
class CanSignalDescriptor:
    index: int
    bus: int
    bus_name: str
    message_name: str
    message_id: int
    signal_name: str
    unit: str | None
    kind: str
    enum_name: str | None


@dataclass(frozen=True)
class CanEnumValueDescriptor:
    enum_name: str
    label: str
    raw: int


class _CanMessageDescriptorAbi(ctypes.Structure):
    _fields_ = [
        ("bus", ctypes.c_uint8),
        ("id", ctypes.c_uint32),
        ("len", ctypes.c_uint8),
    ]


class _CanSignalDescriptorAbi(ctypes.Structure):
    _fields_ = [
        ("bus", ctypes.c_uint8),
        ("message_id", ctypes.c_uint32),
        ("kind", ctypes.c_uint8),
    ]


class _CanEnumValueDescriptorAbi(ctypes.Structure):
    _fields_ = [
        ("raw", ctypes.c_int),
    ]


class CanSignalValue(ctypes.Structure):
    _fields_ = [
        ("value", ctypes.c_double),
    ]


class CanSignalComparison(ctypes.Structure):
    _fields_ = [
        ("bus", ctypes.c_uint8),
        ("message_id", ctypes.c_uint32),
        ("signal_index", ctypes.c_uint32),
        ("expected", ctypes.c_double),
        ("tolerance", ctypes.c_double),
        ("comparison", ctypes.c_uint8),
    ]


@dataclass(frozen=True)
class DecodedCanMessage:
    message: CanMessageDescriptor
    values: dict[str, object]
    event: CanEvent | None = None

    def __getitem__(self, signal_name: str) -> object:
        return self.values[signal_name]

    def __getattr__(self, signal_name: str) -> object:
        try:
            return self.values[signal_name]
        except KeyError as exc:
            raise AttributeError(signal_name) from exc


@dataclass(frozen=True)
class RoutedCanEvent:
    node: str
    bus: CanBusDescriptor
    event: CanEvent


@dataclass
class PeriodicCanMessage:
    message: CanMessageDescriptor
    period_ns: int
    signals: dict[str, float | int | IntEnum]
    encoder: Callable[
        [CanMessageDescriptor, Mapping[str, float | int | IntEnum]], CanPacket
    ] = field(repr=False)
    last_emit_ns: int = 0
    packet: CanPacket = field(init=False)
    native_update: Callable[[CanPacket], None] | None = field(default=None, repr=False)

    def __post_init__(self) -> None:
        self._refresh_packet()

    def set(self, **signals: float | int | IntEnum) -> PeriodicCanMessage:
        self.signals.update(signals)
        self._refresh_packet()
        return self

    def _refresh_packet(self) -> None:
        self.packet = self.encoder(self.message, self.signals)
        if self.native_update is not None:
            self.native_update(self.packet)


class CanInterface:
    def __init__(self, model: NodeRig) -> None:
        self._model = model

    @property
    def buses(self) -> tuple[CanBusDescriptor, ...]:
        return self._model._can_buses

    @property
    def bus_count(self) -> int:
        return self._model._can_bus_count_value()

    @property
    def rx_messages(self) -> tuple[CanMessageDescriptor, ...]:
        return self._model._can_messages

    @property
    def tx_messages(self) -> tuple[CanMessageDescriptor, ...]:
        return self._model._can_tx_messages

    @property
    def rx_signals(self) -> tuple[CanSignalDescriptor, ...]:
        return self._model._can_signals

    @property
    def tx_signals(self) -> tuple[CanSignalDescriptor, ...]:
        return self._model._can_tx_signals

    @property
    def enums(self) -> CanEnumNamespace:
        return self._model._can_enums

    def bus(self, bus: int | str | CanBusDescriptor) -> CanBusDescriptor:
        return self._model._can_bus_descriptor(bus)

    def message(
        self,
        name: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        tx: bool = False,
    ) -> CanMessageDescriptor:
        return self._model._can_message_descriptor(name, bus=bus, tx=tx)

    def tx_message(
        self,
        name: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanMessageDescriptor:
        return self._model._can_tx_message_descriptor(name, bus=bus)

    def send(
        self,
        bus_or_message: int | str | CanBusDescriptor | CanMessageDescriptor,
        frame_id: int | None = None,
        payload: bytes | bytearray | list[int] | tuple[int, ...] | None = None,
        **signals: float | int | IntEnum,
    ) -> bool:
        if isinstance(bus_or_message, CanMessageDescriptor):
            if frame_id is not None or payload is not None:
                raise ValueError(
                    "frame_id and payload must not be provided when sending a CAN message descriptor"
                )
            packet = self.encode(bus_or_message, **signals)
            return self._model._can_send_packet(
                bus_or_message.bus,
                bus_or_message.id,
                packet.payload,
            )

        if frame_id is None or payload is None:
            raise ValueError("raw CAN send requires bus, frame_id, and payload")
        if signals:
            raise ValueError("signal values require sending a CAN message descriptor")
        return self._model._can_send_packet(bus_or_message, frame_id, payload)

    def recv(self, bus: int | str | CanBusDescriptor) -> CanEvent | None:
        return self._model._can_recv_event(bus)

    def recv_latest(self, message: CanMessageDescriptor) -> CanEvent | None:
        return self._model._can_recv_message(message)

    def latest_event(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanEvent | None:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is not None and node_name is not None:
            return cluster.comm.can.latest_message(
                node_name,
                descriptor,
                bus=descriptor.bus,
            )
        return self.recv_latest(descriptor)

    def latest_bus_event(self, bus: int | str | CanBusDescriptor) -> CanEvent | None:
        bus_descriptor = self.bus(bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is not None and node_name is not None:
            return cluster.comm.can.latest_bus_event(node_name, bus_descriptor)

        latest = None
        while self.tx_count(bus_descriptor):
            latest = self.recv(bus_descriptor)
        return latest

    def latest(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        signals: tuple[str, ...] | list[str] | None = None,
    ) -> DecodedCanMessage | None:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        event = self.latest_event(descriptor)
        if event is None:
            return None
        return self.decode(descriptor, event, signals)

    def latest_signal(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> object | None:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is not None and node_name is not None:
            raw_value = cluster._rust_runtime.latest_can_signal(
                node_name,
                descriptor.bus,
                descriptor.id,
                signal,
            )
            if raw_value is not None:
                return self._model._coerce_decoded_can_value(signal, raw_value)

        decoded = self.latest(descriptor, signals=(signal,))
        if decoded is None:
            return None
        return getattr(decoded, signal)

    def run_until_signal_eq(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        expected: float | int | IntEnum,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        timeout: int | float,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
        tolerance: float = 0.0,
        fast_forward: bool = False,
        message_on_timeout: str | None = None,
    ) -> int:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is None or node_name is None:
            raise RuntimeError("native CAN signal predicates require a clustered model")
        timeout_ns = duration_to_ns(timeout, unit=unit)
        step_ns = duration_to_ns(step, unit=step_unit or unit)
        signal_descriptor = self._model._can_tx_signal_descriptor(
            descriptor,
            signal,
        )
        elapsed_ns = cluster._rust_runtime.run_until_can_signal_index_eq(
            source_node=node_name,
            bus=descriptor.bus,
            message_id=descriptor.id,
            signal_index=signal_descriptor.index,
            expected=float(
                int(expected) if isinstance(expected, IntEnum) else expected
            ),
            tolerance=float(tolerance),
            timeout_ns=timeout_ns,
            step_ns=step_ns,
            fast_forward=fast_forward,
            route=cluster.comm.has_python_routes(),
        )
        cluster._sync_elapsed_from_runtime()
        if elapsed_ns is None:
            detail = "" if message_on_timeout is None else f": {message_on_timeout}"
            raise RunUntilTimeout(
                f"condition did not become true within {timeout_ns} ns{detail}"
            )
        return elapsed_ns

    def run_until_signal_gt(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        expected: float | int | IntEnum,
        **kwargs,
    ) -> int:
        return self._run_until_signal_cmp(
            message, signal, expected, comparison=_COMPARE_GT, **kwargs
        )

    def run_until_signal_ge(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        expected: float | int | IntEnum,
        **kwargs,
    ) -> int:
        return self._run_until_signal_cmp(
            message, signal, expected, comparison=_COMPARE_GE, **kwargs
        )

    def run_until_signal_lt(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        expected: float | int | IntEnum,
        **kwargs,
    ) -> int:
        return self._run_until_signal_cmp(
            message, signal, expected, comparison=_COMPARE_LT, **kwargs
        )

    def run_until_signal_le(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        expected: float | int | IntEnum,
        **kwargs,
    ) -> int:
        return self._run_until_signal_cmp(
            message, signal, expected, comparison=_COMPARE_LE, **kwargs
        )

    def run_until_signals_eq(self, comparisons, **kwargs) -> int:
        return self._run_until_signals_with_comparison(
            comparisons, comparison=_COMPARE_EQ, **kwargs
        )

    def run_until_signals_gt(self, comparisons, **kwargs) -> int:
        return self._run_until_signals_with_comparison(
            comparisons, comparison=_COMPARE_GT, **kwargs
        )

    def run_until_signals_ge(self, comparisons, **kwargs) -> int:
        return self._run_until_signals_with_comparison(
            comparisons, comparison=_COMPARE_GE, **kwargs
        )

    def run_until_signals_lt(self, comparisons, **kwargs) -> int:
        return self._run_until_signals_with_comparison(
            comparisons, comparison=_COMPARE_LT, **kwargs
        )

    def run_until_signals_le(self, comparisons, **kwargs) -> int:
        return self._run_until_signals_with_comparison(
            comparisons, comparison=_COMPARE_LE, **kwargs
        )

    def _run_until_signals_with_comparison(
        self,
        comparisons,
        *,
        comparison: int,
        **kwargs,
    ) -> int:
        return self.run_until_signals_cmp(
            tuple((*signal_comparison, comparison) for signal_comparison in comparisons),
            **kwargs,
        )

    def _run_until_signal_cmp(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        expected: float | int | IntEnum,
        *,
        comparison: int,
        bus: int | str | CanBusDescriptor | None = None,
        timeout: int | float,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
        tolerance: float = 0.0,
        fast_forward: bool = False,
        message_on_timeout: str | None = None,
    ) -> int:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is None or node_name is None:
            raise RuntimeError("native CAN signal predicates require a clustered model")
        timeout_ns = duration_to_ns(timeout, unit=unit)
        step_ns = duration_to_ns(step, unit=step_unit or unit)
        signal_descriptor = self._model._can_tx_signal_descriptor(
            descriptor,
            signal,
        )
        elapsed_ns = cluster._rust_runtime.run_until_can_signal_index_cmp(
            source_node=node_name,
            bus=descriptor.bus,
            message_id=descriptor.id,
            signal_index=signal_descriptor.index,
            expected=float(
                int(expected) if isinstance(expected, IntEnum) else expected
            ),
            tolerance=float(tolerance),
            comparison=int(comparison),
            timeout_ns=timeout_ns,
            step_ns=step_ns,
            fast_forward=fast_forward,
            route=cluster.comm.has_python_routes(),
        )
        cluster._sync_elapsed_from_runtime()
        if elapsed_ns is None:
            detail = "" if message_on_timeout is None else f": {message_on_timeout}"
            raise RunUntilTimeout(
                f"condition did not become true within {timeout_ns} ns{detail}"
            )
        return elapsed_ns

    def run_until_signals_cmp(
        self,
        comparisons: tuple[
            tuple[str | CanMessageDescriptor, str, float | int | IntEnum, int],
            ...
        ]
        | list[tuple[str | CanMessageDescriptor, str, float | int | IntEnum, int]],
        *,
        bus: int | str | CanBusDescriptor | None = None,
        timeout: int | float,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
        tolerance: float = 0.0,
        fast_forward: bool = False,
        message_on_timeout: str | None = None,
    ) -> int:
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is None or node_name is None:
            raise RuntimeError("native CAN signal predicates require a clustered model")
        if not comparisons:
            raise ValueError("at least one CAN signal comparison is required")

        comparison_array = (CanSignalComparison * len(comparisons))()
        for index, (message, signal, expected, comparison) in enumerate(comparisons):
            descriptor = self._tx_message_descriptor(message, bus=bus)
            signal_descriptor = self._model._can_tx_signal_descriptor(
                descriptor,
                signal,
            )
            comparison_array[index].bus = descriptor.bus
            comparison_array[index].message_id = descriptor.id
            comparison_array[index].signal_index = signal_descriptor.index
            comparison_array[index].expected = float(
                int(expected) if isinstance(expected, IntEnum) else expected
            )
            comparison_array[index].tolerance = float(tolerance)
            comparison_array[index].comparison = int(comparison)

        timeout_ns = duration_to_ns(timeout, unit=unit)
        step_ns = duration_to_ns(step, unit=step_unit or unit)
        elapsed_ns = cluster._rust_runtime.run_until_can_signal_comparisons(
            source_node=node_name,
            comparisons=comparison_array,
            comparison_count=len(comparison_array),
            timeout_ns=timeout_ns,
            step_ns=step_ns,
            fast_forward=fast_forward,
            route=cluster.comm.has_python_routes(),
        )
        cluster._sync_elapsed_from_runtime()
        if elapsed_ns is None:
            detail = "" if message_on_timeout is None else f": {message_on_timeout}"
            raise RunUntilTimeout(
                f"condition did not become true within {timeout_ns} ns{detail}"
            )
        return elapsed_ns

    def rx_count(self, bus: int | str | CanBusDescriptor) -> int:
        return self._model._can_rx_count_value(bus)

    def tx_count(self, bus: int | str | CanBusDescriptor) -> int:
        return self._model._can_tx_count_value(bus)

    def decode(
        self,
        message: CanMessageDescriptor,
        packet_or_event: CanPacket | CanEvent,
        signal_names: tuple[str, ...] | list[str] | None = None,
    ) -> DecodedCanMessage:
        event = packet_or_event if isinstance(packet_or_event, CanEvent) else None
        packet = packet_or_event.packet if event is not None else packet_or_event
        signals = (
            self._model._signals_for_message(message)
            if signal_names is None
            else signal_names
        )
        raw_values = self._model._can_decode_message_raw(
            message.bus, packet, tuple(signals)
        )
        typed_values = {
            signal_name: self._model._coerce_decoded_can_value(signal_name, value)
            for signal_name, value in raw_values.items()
        }
        return DecodedCanMessage(message=message, values=typed_values, event=event)

    def encode(
        self, message: CanMessageDescriptor, **signals: float | int | IntEnum
    ) -> CanPacket:
        raw_signals = {
            signal_name: int(value) if isinstance(value, IntEnum) else value
            for signal_name, value in signals.items()
        }
        return self._model._can_encode_message_raw(
            message.bus, message.name, **raw_signals
        )

    def _tx_message_descriptor(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanMessageDescriptor:
        if isinstance(message, CanMessageDescriptor):
            if bus is not None and self._model._coerce_can_bus(bus) != message.bus:
                raise ValueError(f"message {message.name!r} is not on bus {bus!r}")
            return message
        return self.tx_message(message, bus=bus)


class CanEnumNamespace(Mapping[str, type[IntEnum]]):
    def __init__(self, enums: Mapping[str, type[IntEnum]]) -> None:
        self._enums = dict(enums)
        self._attrs = {
            python_enum_class_name(enum_name): enum_type
            for enum_name, enum_type in self._enums.items()
        }

    def __getitem__(self, enum_name: str) -> type[IntEnum]:
        try:
            return self._enums[enum_name]
        except KeyError:
            return self._attrs[enum_name]

    def __iter__(self) -> Iterator[str]:
        return iter(self._enums)

    def __len__(self) -> int:
        return len(self._enums)

    def __getattr__(self, enum_name: str) -> type[IntEnum]:
        try:
            return self._attrs[enum_name]
        except KeyError as exc:
            raise AttributeError(enum_name) from exc


def python_enum_member(label: str) -> str:
    member = "".join(ch if ch.isalnum() else "_" for ch in label).upper().strip("_")
    if not member:
        member = "VALUE"
    if member[0].isdigit():
        member = "_" + member
    return member


def python_enum_attr_member(label: str) -> str:
    member = "".join(ch.lower() if ch.isalnum() else "_" for ch in label).strip("_")
    if not member:
        member = "value"
    if member[0].isdigit():
        member = "_" + member
    return member


def python_enum_class_name(name: str) -> str:
    parts = []
    current = ""
    for ch in name:
        if ch.isalnum():
            current += ch
            continue
        if current:
            parts.append(current)
            current = ""
    if current:
        parts.append(current)
    return "".join(part[:1].upper() + part[1:] for part in parts) or "GeneratedEnum"
