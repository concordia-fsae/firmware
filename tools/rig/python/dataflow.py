from __future__ import annotations

from typing import Protocol


class DataflowWait:
    """A scheduler-owned wait on an ingress edge or event queue."""

    def __init__(self, runtime: object, wait_id: int) -> None:
        self._runtime = runtime
        self._wait_id = wait_id
        self._active = True

    def wait(
        self,
        *,
        timeout_ns: int,
        step_ns: int,
        route: bool = True,
    ) -> int | None:
        if not self._active:
            raise RuntimeError("dataflow wait is no longer active")
        self._active = False
        return self._runtime.run_until_dataflow_wait(
            self._wait_id,
            timeout_ns=timeout_ns,
            step_ns=step_ns,
            route=route,
        )

    def cancel(self) -> None:
        if self._active:
            self._runtime.cancel_dataflow_wait(self._wait_id)
            self._active = False


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
