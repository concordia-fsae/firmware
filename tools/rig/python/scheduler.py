from __future__ import annotations

import ctypes
from dataclasses import dataclass


class _SchedulerCallbackContextAbi(ctypes.Structure):
    _fields_ = [
        ("elapsed_ns", ctypes.c_uint64),
        ("delta_ns", ctypes.c_uint64),
    ]


@dataclass(frozen=True)
class SchedulerContext:
    elapsed_ns: int
    delta_ns: int

    @classmethod
    def from_abi(cls, context: _SchedulerCallbackContextAbi) -> SchedulerContext:
        return cls(
            elapsed_ns=int(context.elapsed_ns),
            delta_ns=int(context.delta_ns),
        )


@dataclass(frozen=True)
class PythonSchedulerCallbacks:
    scheduled: int
    reset: int
    period_ns: int


@dataclass(frozen=True)
class RustSchedulerCallbacks:
    run_for: int
    reset: int
