from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum, IntEnum, auto
from functools import cache
from itertools import count
from typing import Generic, TypeVar


PayloadT = TypeVar("PayloadT")


class PeripheralInterface(IntEnum):
    TIMER_DUTY = 1
    TIMER_FREQUENCY = 2
    SPI_TRANSACTION = 3
    TIMER_CAPTURE = 4


class _DataPathNamespace(Enum):
    CAN = auto()
    NAMED = auto()


@dataclass(frozen=True)
class DataPathKey:
    value: int


@dataclass(frozen=True)
class PeripheralBinding:
    interface: PeripheralInterface
    channel: int | None = None
    port: int | None = None
    device: int | None = None


_DATAPATH_IDS = count(1)


def _next_datapath_key() -> DataPathKey:
    return DataPathKey(next(_DATAPATH_IDS))


@dataclass(frozen=True, eq=False)
class DataPath:
    parts: tuple[object, ...]
    peripheral_binding: PeripheralBinding | None = None
    key: DataPathKey = field(default_factory=_next_datapath_key, repr=False)

    @classmethod
    def component(cls, component: object, port: object) -> DataPath:
        return _component_datapath(component, port)

    @classmethod
    def named(cls, *parts: object) -> DataPath:
        return cls((_DataPathNamespace.NAMED, *parts))

    @classmethod
    def can_bus(cls, bus: object) -> DataPath:
        return _can_datapath(bus)

    @classmethod
    def peripheral(
        cls,
        *parts: object,
        binding: PeripheralBinding,
    ) -> DataPath:
        return cls(parts, binding)


def datapath_key(path: DataPath) -> DataPathKey:
    return path.key


def is_can_datapath(path: DataPath) -> bool:
    return len(path.parts) == 2 and path.parts[0] == _DataPathNamespace.CAN


def can_datapath_bus(path: DataPath) -> object:
    if not is_can_datapath(path):
        raise ValueError(f"datapath {path!r} is not a CAN bus datapath")
    return path.parts[1]


def require_peripheral_binding(path: DataPath) -> PeripheralBinding:
    if path.peripheral_binding is None:
        raise ValueError(f"datapath {path!r} is not backed by a controller peripheral")
    return path.peripheral_binding


@cache
def _component_datapath(component: object, port: object) -> DataPath:
    return DataPath((component, port))


@cache
def _can_datapath(bus: object) -> DataPath:
    return DataPath((_DataPathNamespace.CAN, bus))


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
        self._outputs_by_key: dict[DataPathKey, list[ModelDataPathOutput[object]]] = {}
        self._inputs_by_key: dict[DataPathKey, list[ModelDataPathInput[object]]] = {}
        self._paths_by_key: dict[DataPathKey, DataPath] = {}

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
