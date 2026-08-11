from __future__ import annotations

import ctypes
from collections.abc import Iterator, Mapping
from dataclasses import dataclass
from enum import IntEnum


SIGNAL_KIND_NAMES = {
    0: "Numeric",
    1: "Boolean",
    2: "Enum",
}


class CanPacket(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_uint32),
        ("len", ctypes.c_uint8),
        ("data", ctypes.c_uint8 * 8),
    ]

    @classmethod
    def from_payload(cls, frame_id: int, payload: bytes | bytearray | list[int] | tuple[int, ...]):
        payload_bytes = bytes(payload)
        if len(payload_bytes) > 8:
            raise ValueError(f"CAN payload must be at most 8 bytes, got {len(payload_bytes)}")

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
