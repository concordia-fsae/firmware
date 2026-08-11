from __future__ import annotations

import ctypes
from collections.abc import Callable
from typing import TypeVar

from .datapath import DataPath, ModelDataPaths
from .runtime import _RustClusterRuntime
from .time import duration_to_ns


ModelClass = TypeVar("ModelClass", bound=type)


class ModelRig:
    """Schedulable model with datapaths that can participate in a cluster."""

    has_can = False
    _ClusterRunForCallback = ctypes.CFUNCTYPE(None, ctypes.c_uint64)
    _ClusterNextStepCallback = ctypes.CFUNCTYPE(ctypes.c_uint64, ctypes.c_uint64)
    _ClusterResetCallback = ctypes.CFUNCTYPE(None)

    def __init__(
        self,
        *,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
    ) -> None:
        self.datapaths = ModelDataPaths()
        self._cluster_rig: ClusterRig | None = None
        self._cluster_node_name: str | None = None
        self.elapsed_ns = 0
        self._scheduler_period_ns = (
            None
            if scheduler_period is None
            else duration_to_ns(scheduler_period, unit=scheduler_unit)
        )
        if self._scheduler_period_ns is not None and self._scheduler_period_ns <= 0:
            raise ValueError(
                f"scheduler period must be positive, got {scheduler_period}"
            )
        self._cluster_run_for_callback = self._ClusterRunForCallback(
            self._cluster_run_for
        )
        self._cluster_next_step_callback = self._ClusterNextStepCallback(
            self._cluster_next_scheduler_step
        )
        self._cluster_reset_callback = self._ClusterResetCallback(self.reset)
        self._standalone_runtime: _RustClusterRuntime | None = None

    def reset(self) -> None:
        self.elapsed_ns = 0
        self._standalone_runtime = None

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        duration_ns = duration_to_ns(duration, unit=unit)
        self._runtime().run_for(duration_ns, duration_ns)

    def _run_for_from_runtime(self, duration_ns: int) -> None:
        if self._scheduler_period_ns is None:
            self.elapsed_ns += duration_ns
            return

        remaining_ns = duration_ns
        while remaining_ns > 0:
            step_ns = self.next_scheduler_step(remaining_ns, unit="ns")
            self.elapsed_ns += step_ns
            remaining_ns -= step_ns
            if self.elapsed_ns % self._scheduler_period_ns == 0:
                self._run_scheduled()

    def next_scheduler_step(self, duration: int | float, *, unit: str = "ms") -> int:
        duration_ns = duration_to_ns(duration, unit=unit)
        if self._scheduler_period_ns is None:
            return duration_ns
        elapsed_in_period = self.elapsed_ns % self._scheduler_period_ns
        remaining_period_ns = self._scheduler_period_ns - elapsed_in_period
        return min(duration_ns, remaining_period_ns)

    def _run_scheduled(self) -> None:
        pass

    def configure_datapath(self, path: DataPath) -> None:
        raise ValueError(f"datapath {path!r} is not supported by {type(self).__name__}")

    def configure_model_outputs_for(self, model: object) -> None:
        datapaths = getattr(model, "datapaths", None)
        if datapaths is None:
            return
        for input_ in datapaths.inputs():
            if self.supports_datapath(input_.path):
                self.configure_datapath(input_.path)

    def supports_datapath(self, path: DataPath) -> bool:
        return bool(self.datapaths.outputs(path))

    def set_online(self, online: bool) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            raise RuntimeError(
                f"{type(self).__name__} is not attached to a cluster rig"
            )
        self._cluster_rig.set_node_online(self._cluster_node_name, online)

    def is_online(self) -> bool:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return True
        return self._cluster_rig.node_online(self._cluster_node_name)

    def rust_cluster_node_abi(self) -> tuple[int, int, int]:
        return (
            self._callback_address(self._cluster_run_for_callback),
            self._callback_address(self._cluster_next_step_callback),
            self._callback_address(self._cluster_reset_callback),
        )

    def _cluster_run_for(self, duration_ns: int) -> None:
        self._run_for_from_runtime(duration_ns)

    def _cluster_next_scheduler_step(self, duration_ns: int) -> int:
        return self.next_scheduler_step(duration_ns, unit="ns")

    def _runtime(self) -> _RustClusterRuntime:
        if self._standalone_runtime is None:
            self._standalone_runtime = _RustClusterRuntime()
            self._standalone_runtime.add_node("__standalone__", self)
        return self._standalone_runtime

    @staticmethod
    def _callback_address(callback) -> int:
        value = ctypes.cast(callback, ctypes.c_void_p).value
        if value is None:
            raise RuntimeError(f"could not resolve callback pointer for {callback!r}")
        return int(value)


class ComponentRig(ModelRig):
    """Pure Python model that can run standalone or inside a cluster."""

    def configure_owner(self, owner: object) -> None:
        pass


class PeriodicDataPathProducer(ComponentRig):
    """Scheduled component that emits model-input payloads on a datapath."""

    def __init__(
        self,
        path: DataPath,
        payload: object
        | Callable[[PeriodicDataPathProducer], object | tuple[object, ...] | None],
        *,
        scheduler_period: int | float = 1,
        scheduler_unit: str = "ms",
    ) -> None:
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_unit=scheduler_unit,
        )
        self.path = path
        self._payload = payload
        self._last_emit_ns = 0
        self._pending_payloads: list[object] = []
        self.datapaths.add_output(
            path,
            pending=lambda: len(self._pending_payloads),
            recv=self._recv,
            recv_many=self._recv_many,
        )

    def reset(self) -> None:
        super().reset()
        self._last_emit_ns = 0
        self._pending_payloads.clear()

    def next_scheduler_step(self, duration: int | float, *, unit: str = "ms") -> int:
        return duration_to_ns(duration, unit=unit)

    def _run_for_from_runtime(self, duration_ns: int) -> None:
        self.elapsed_ns += duration_ns
        if self._scheduler_period_ns is None:
            return
        if self.elapsed_ns - self._last_emit_ns >= self._scheduler_period_ns:
            self._last_emit_ns = self.elapsed_ns
            self._run_scheduled()

    def _run_scheduled(self) -> None:
        produced = self._payload(self) if callable(self._payload) else self._payload
        if produced is None:
            return
        if isinstance(produced, tuple):
            self._pending_payloads.extend(produced)
            return
        self._pending_payloads.append(produced)

    def _recv(self) -> object | None:
        return self._pending_payloads.pop(0) if self._pending_payloads else None

    def _recv_many(self, count: int) -> tuple[object, ...]:
        payloads = tuple(self._pending_payloads[:count])
        del self._pending_payloads[:count]
        return payloads


def extend_model_class(
    model_class: ModelClass,
    *mixins: type,
    name: str | None = None,
) -> ModelClass:
    if not mixins:
        return model_class

    extended = type(
        name or model_class.__name__,
        (*mixins, model_class),
        {
            "__module__": model_class.__module__,
            "__doc__": model_class.__doc__,
        },
    )
    return extended  # type: ignore[return-value]
