from __future__ import annotations

from typing import Protocol


class DataflowEvent(Protocol):
    """Payload contract shared by every event-bearing datapath."""

    timestamp_ns: int


class NativeRouteEndpoint(Protocol):
    """Model-owned native endpoint that validates and registers its own route."""

    def compatible_with(self, sink: object) -> bool: ...

    @property
    def scalar_source_route_id(self) -> int | None: ...

    def connect(
        self,
        runtime: object,
        *,
        source_node: str,
        sink_node: str,
        sink: object,
    ) -> bool: ...
