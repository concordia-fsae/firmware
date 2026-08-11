from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from enum import Enum
from typing import Generic, TypeVar


PayloadT = TypeVar("PayloadT")


@dataclass(frozen=True)
class PeripheralBinding:
    interface: str
    channel: int | None = None
    port: int | None = None
    device: int | None = None


@dataclass(frozen=True)
class DataPath:
    parts: tuple[object, ...]
    peripheral_binding: PeripheralBinding | None = None

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
    return "datapath:" + ":".join(_datapath_part_key(part) for part in path.parts)


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


@dataclass(frozen=True)
class DataPathSink(Generic[PayloadT]):
    node: str
    path: DataPath
    send: Callable[[PayloadT], bool]


@dataclass(frozen=True)
class ModelDataPathOutput(Generic[PayloadT]):
    path: DataPath
    pending: Callable[[], int]
    recv: Callable[[], PayloadT | None]


@dataclass(frozen=True)
class ModelDataPathInput(Generic[PayloadT]):
    path: DataPath
    send: Callable[[PayloadT], bool]


class FanoutDataPath(Generic[PayloadT]):
    def __init__(self) -> None:
        self.records: list[DataPathRecord[PayloadT]] = []

    def clear(self) -> None:
        self.records.clear()


class ModelDataPaths:
    def __init__(self) -> None:
        self._outputs: list[ModelDataPathOutput[object]] = []
        self._inputs: list[ModelDataPathInput[object]] = []

    def add_output(
        self,
        path: DataPath,
        *,
        pending: Callable[[], int],
        recv: Callable[[], object | None],
    ) -> None:
        self._outputs.append(ModelDataPathOutput(path, pending, recv))

    def add_input(
        self,
        path: DataPath,
        *,
        send: Callable[[object], bool],
    ) -> None:
        self._inputs.append(ModelDataPathInput(path, send))

    def outputs(
        self, path: DataPath | None = None
    ) -> tuple[ModelDataPathOutput[object], ...]:
        key = None if path is None else datapath_key(path)
        return tuple(
            output
            for output in self._outputs
            if key is None or datapath_key(output.path) == key
        )

    def inputs(
        self, path: DataPath | None = None
    ) -> tuple[ModelDataPathInput[object], ...]:
        key = None if path is None else datapath_key(path)
        return tuple(
            input_
            for input_ in self._inputs
            if key is None or datapath_key(input_.path) == key
        )

    @property
    def paths(self) -> tuple[DataPath, ...]:
        paths: dict[str, DataPath] = {}
        for path in [output.path for output in self._outputs] + [
            input_.path for input_ in self._inputs
        ]:
            paths.setdefault(datapath_key(path), path)
        return tuple(paths.values())
