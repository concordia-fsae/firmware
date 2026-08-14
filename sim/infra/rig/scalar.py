from __future__ import annotations

import ctypes
from dataclasses import dataclass



class ScalarEvent(ctypes.Structure):
    """Canonical scalar event ABI shared by model callbacks and the runtime."""

    _fields_ = [
        ("value", ctypes.c_float),
        ("timestamp_ns", ctypes.c_uint64),
    ]


@dataclass(frozen=True)
class ScalarRouteEndpoint:
    route_id: int
    count: int
    recv_many: int
    send_many: int

    @property
    def scalar_source_route_id(self) -> int:
        return self.route_id

    def compatible_with(self, sink: object) -> bool:
        return isinstance(sink, (ScalarRouteEndpoint, ScalarSinkRouteEndpoint, ScalarStateSinkRouteEndpoint, ScalarInputRouteEndpoint))

    def connect(self, runtime: object, *, source_node: str, sink_node: str, sink: object) -> bool:
        if isinstance(sink, ScalarInputRouteEndpoint):
            return runtime.add_scalar_input_route(source_node=source_node, source_route_id=self.route_id, source_count=self.count, source_recv_many=self.recv_many, sink_node=sink_node, sink_route_id=sink.route_id)
        if isinstance(sink, ScalarStateSinkRouteEndpoint):
            return runtime.add_scalar_state_route(source_node=source_node, route_id=self.route_id, source_count=self.count, source_recv_many=self.recv_many, sink_node=sink_node, sink_route_id=sink.route_id, sink_id=sink.sink_id, value_scale=sink.value_scale, set_value=sink.set_value)
        if isinstance(sink, ScalarSinkRouteEndpoint):
            return self.route_id == sink.route_id and runtime.add_scalar_sink_route(source_node=source_node, route_id=self.route_id, source_count=self.count, source_recv_many=self.recv_many, sink_node=sink_node, sink_id=sink.sink_id, value_scale=sink.value_scale, set_value=sink.set_value)
        if isinstance(sink, ScalarRouteEndpoint):
            return self.route_id == sink.route_id and runtime.add_scalar_route(source_node=source_node, route_id=self.route_id, source_count=self.count, source_recv_many=self.recv_many, sink_node=sink_node, sink_send_many=sink.send_many)
        return False


@dataclass(frozen=True)
class ScalarSinkRouteEndpoint:
    route_id: int
    sink_id: int
    value_scale: float
    set_value: int

    @property
    def scalar_source_route_id(self) -> None:
        return None

    def compatible_with(self, sink: object) -> bool:
        return False


@dataclass(frozen=True)
class ScalarStateSinkRouteEndpoint:
    route_id: int
    initial_value: float
    sink_id: int | None = None
    value_scale: float = 1.0
    set_value: int | None = None

    @property
    def scalar_source_route_id(self) -> None:
        return None

    def compatible_with(self, sink: object) -> bool:
        return False


@dataclass(frozen=True)
class ScalarInputRouteEndpoint:
    route_id: int

    @property
    def scalar_source_route_id(self) -> None:
        return None

    def compatible_with(self, sink: object) -> bool:
        return False
