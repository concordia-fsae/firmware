from __future__ import annotations

import ctypes
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum
from typing import Generic, TypeVar


PayloadT = TypeVar("PayloadT")


@dataclass(frozen=True)
class PeripheralBinding:
    interface: str
    channel: int | None = None
    port: int | None = None
    device: int | None = None


class ScalarEvent(ctypes.Structure):
    _fields_ = [
        ("value", ctypes.c_float),
        ("timestamp_ns", ctypes.c_uint64),
    ]


@dataclass(frozen=True)
class DataPath:
    parts: tuple[object, ...]
    peripheral_binding: PeripheralBinding | None = None
    key: str = field(init=False, repr=False, compare=False)

    def __post_init__(self) -> None:
        object.__setattr__(
            self,
            "key",
            "datapath:" + ":".join(_datapath_part_key(part) for part in self.parts),
        )

    @classmethod
    def component(cls, component: object, port: object) -> DataPath:
        return cls((component, port))

    @classmethod
    def can_bus(cls, bus: object) -> DataPath:
        return cls(("can", bus))

    @classmethod
    def peripheral(
        cls,
        *parts: object,
        binding: PeripheralBinding,
    ) -> DataPath:
        return cls(parts, binding)


def datapath_key(path: DataPath) -> str:
    return path.key


def _datapath_part_key(part: object) -> str:
    if isinstance(part, Enum):
        return f"{type(part).__module__}.{type(part).__qualname__}.{part.name}"
    if isinstance(part, type):
        return f"{part.__module__}.{part.__qualname__}"
    return str(part)


@dataclass(frozen=True)
class DataPathRecord(Generic[PayloadT]):
    source: str
    path: DataPath
    payload: PayloadT
    timestamp_ns: int


@dataclass(frozen=True)
class DataPathLink:
    path: DataPath
    source_node: str
    sink_node: str | None = None


@dataclass(frozen=True)
class ModelDataPathInputConnector:
    connect: Callable[[object, DataPath], None]


@dataclass(frozen=True)
class ModelDataPathOutputConnector:
    connect: Callable[[object], None]


@dataclass(frozen=True)
class ComponentDataPathOutput:
    path: Callable[[object], DataPath]

    def bind_to(
        self,
        sink: ModelDataPathInputConnector,
    ) -> ComponentDataPathBinding:
        return ComponentDataPathBinding(self, sink)


@dataclass(frozen=True)
class ComponentDataPathBinding:
    output: ComponentDataPathOutput
    sink: ModelDataPathInputConnector

    def bind(self, owner: object, component: object) -> None:
        self.sink.connect(owner, self.output.path(component))


@dataclass(frozen=True)
class DataPathSource(Generic[PayloadT]):
    node: str
    path: DataPath
    pending: Callable[[], int]
    recv: Callable[[], PayloadT | None]
    recv_many: Callable[[int], tuple[PayloadT, ...]] | None = None


@dataclass(frozen=True)
class DataPathSink(Generic[PayloadT]):
    node: str
    path: DataPath
    send: Callable[[PayloadT], bool]
    send_many: Callable[[tuple[PayloadT, ...]], int] | None = None


@dataclass(frozen=True)
class ModelDataPathOutput(Generic[PayloadT]):
    path: DataPath
    pending: Callable[[], int]
    recv: Callable[[], PayloadT | None]
    recv_many: Callable[[int], tuple[PayloadT, ...]] | None = None


@dataclass(frozen=True)
class ModelDataPathInput(Generic[PayloadT]):
    path: DataPath
    send: Callable[[PayloadT], bool]
    send_many: Callable[[tuple[PayloadT, ...]], int] | None = None


class FanoutDataPath(Generic[PayloadT]):
    def __init__(self) -> None:
        self.records: list[DataPathRecord[PayloadT]] = []

    def clear(self) -> None:
        self.records.clear()


class ModelDataPaths:
    def __init__(self) -> None:
        self._outputs: list[ModelDataPathOutput[object]] = []
        self._inputs: list[ModelDataPathInput[object]] = []
        self._outputs_by_key: dict[str, list[ModelDataPathOutput[object]]] = {}
        self._inputs_by_key: dict[str, list[ModelDataPathInput[object]]] = {}
        self._paths_by_key: dict[str, DataPath] = {}

    def add_output(
        self,
        path: DataPath,
        *,
        pending: Callable[[], int],
        recv: Callable[[], object | None],
        recv_many: Callable[[int], tuple[object, ...]] | None = None,
    ) -> None:
        output = ModelDataPathOutput(path, pending, recv, recv_many)
        key = datapath_key(path)
        self._outputs.append(output)
        self._outputs_by_key.setdefault(key, []).append(output)
        self._paths_by_key.setdefault(key, path)

    def add_input(
        self,
        path: DataPath,
        *,
        send: Callable[[object], bool],
        send_many: Callable[[tuple[object, ...]], int] | None = None,
    ) -> None:
        input_ = ModelDataPathInput(path, send, send_many)
        key = datapath_key(path)
        self._inputs.append(input_)
        self._inputs_by_key.setdefault(key, []).append(input_)
        self._paths_by_key.setdefault(key, path)

    def outputs(
        self, path: DataPath | None = None
    ) -> tuple[ModelDataPathOutput[object], ...]:
        if path is None:
            return tuple(self._outputs)
        return tuple(self._outputs_by_key.get(datapath_key(path), ()))

    def inputs(
        self, path: DataPath | None = None
    ) -> tuple[ModelDataPathInput[object], ...]:
        if path is None:
            return tuple(self._inputs)
        return tuple(self._inputs_by_key.get(datapath_key(path), ()))

    @property
    def paths(self) -> tuple[DataPath, ...]:
        return tuple(self._paths_by_key.values())
