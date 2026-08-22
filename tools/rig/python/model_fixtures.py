from __future__ import annotations

from rig import ComponentRig, DataPath, ModelRig, SchedulerContext


class FakeNode(ModelRig):
    def __init__(self) -> None:
        super().__init__()
        self.run_count = 0
        self.reset_count = 0

    def reset(self) -> None:
        self.reset_count += 1

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        self.run_count += duration


class ScheduledComponent(ComponentRig):
    def __init__(self) -> None:
        self.scheduled_times_ns = []
        super().__init__(
            scheduler_period=250,
            scheduler_unit="us",
            scheduler_callback=self._record_scheduled_time,
        )

    def reset(self) -> None:
        super().reset()
        self.scheduled_times_ns.clear()

    def _record_scheduled_time(self, context: SchedulerContext) -> None:
        self.scheduled_times_ns.append(context.elapsed_ns)


class TickModel(ModelRig):
    def __init__(self) -> None:
        self.ticks = 0
        super().__init__(
            scheduler_period=1,
            scheduler_callback=self._tick,
        )

    def reset(self) -> None:
        super().reset()
        self.ticks = 0

    def _tick(self, context: SchedulerContext) -> None:
        self.ticks += 1


class BatchObservedModel(TickModel):
    def __init__(self) -> None:
        super().__init__()
        self.run_durations_ns = []

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        super().run_for(duration, unit=unit)
        self.run_durations_ns.append(int(duration))


class PythonOwner(ModelRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.path = path
        self.pending_payloads = []

    def supports_datapath(self, path: DataPath) -> bool:
        return path == self.path

    def configure_datapath(self, path: DataPath) -> None:
        if self.datapaths.outputs(path):
            return
        self.datapaths.add_output(
            path,
            pending=lambda: len(self.pending_payloads),
            recv=lambda: (
                self.pending_payloads.pop(0) if self.pending_payloads else None
            ),
        )


class PythonConsumer(ComponentRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.received_payloads = []
        self.datapaths.add_input(
            path,
            send=lambda payload: not self.received_payloads.append(payload),
        )


class ScalarSourceModel(ModelRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.values = []
        self.add_scalar_output(
            path,
            pending=lambda: len(self.values),
            recv=lambda: self.values.pop(0) if self.values else None,
        )


class InputTriggeredScalarSink(ModelRig):
    def __init__(self, path: DataPath) -> None:
        self.values = []
        self.scheduled_times_ns = []
        super().__init__(scheduler_callback=self._record_scheduled_input)
        self.add_scalar_input(path, send=self._record_value)

    def reset(self) -> None:
        super().reset()
        self.values.clear()
        self.scheduled_times_ns.clear()

    def _record_value(self, value: float) -> bool:
        self.values.append(value)
        return True

    def _record_scheduled_input(self, context: SchedulerContext) -> None:
        self.scheduled_times_ns.append(context.elapsed_ns)


class SharedObjectBackedFakeNode(ModelRig):
    def __init__(self, library_path) -> None:
        super().__init__()
        self.library_path = library_path


__all__ = [
    "BatchObservedModel",
    "FakeNode",
    "InputTriggeredScalarSink",
    "PythonConsumer",
    "PythonOwner",
    "ScalarSourceModel",
    "ScheduledComponent",
    "SharedObjectBackedFakeNode",
    "TickModel",
]
