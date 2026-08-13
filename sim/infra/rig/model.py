from __future__ import annotations

from typing import TypeVar

from .datapath import DataPath, ModelDataPaths
from .time import duration_to_ns


ModelClass = TypeVar("ModelClass", bound=type)


class ModelRig:
    """Schedulable model with datapaths that can participate in a cluster."""

    has_can = False

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

    def reset(self) -> None:
        self.elapsed_ns = 0

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        duration_ns = duration_to_ns(duration, unit=unit)
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


class ComponentRig(ModelRig):
    """Pure Python model that can run standalone or inside a cluster."""


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
