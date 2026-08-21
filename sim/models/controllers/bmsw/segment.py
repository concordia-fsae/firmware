from __future__ import annotations

import math
from collections import deque
from enum import Enum, auto

from rig import (
    ComponentDataPathBinding,
    ComponentDataPathOutput,
    ComponentRig,
    DataPath,
    SchedulerContext,
)
from sim.models.catalog import ComponentSpec


class BmsSegmentPort(Enum):
    CELL_VOLTAGE = auto()
    THERMISTOR_VOLTAGE = auto()
    SEGMENT_VOLTAGE = auto()


class BmsSegmentModel(ComponentRig):
    """Python-only BMS segment sensor model.

    The model represents passive cell taps and thermistors.  Its outputs are
    ordinary Rig scalar routes, so a firmware-backed BMSW node consumes them
    through its MAX14921/ADC/mux input interfaces without any Python receive
    fallback.
    """

    def __init__(
        self,
        *,
        platform: str,
        cell_voltages: tuple[float, ...] | list[float] | None = None,
        temperatures_c: tuple[float, ...] | list[float] | None = None,
        segment_voltage: float | None = None,
        node_id: int = 0,
        scheduler_period: int | float = 1,
    ) -> None:
        self.platform = platform.lower()
        self.node_id = node_id
        if self.platform not in {"cfr25", "cfr26"}:
            raise ValueError(f"unsupported BMS segment platform {platform!r}")
        self.series_cells = 14 if self.platform == "cfr25" else 11
        self.thermistor_count = 20 if self.platform == "cfr25" else 9
        self._cell_voltages = self._validate_values(
            cell_voltages or (3.7,) * self.series_cells,
            self.series_cells,
            "cell voltages",
        )
        self._temperatures_c = self._validate_values(
            temperatures_c or (25.0,) * self.thermistor_count,
            self.thermistor_count,
            "temperatures",
        )
        self._segment_voltage = (
            float(segment_voltage)
            if segment_voltage is not None
            else 350.0 / (6 if self.platform == "cfr25" else 8)
        )
        self._queues: dict[DataPath, deque[float]] = {}
        self.cell_voltage_outputs = tuple(
            self.cell_voltage_output_channel(index, node_id=node_id)
            for index in range(self.series_cells)
        )
        self.thermistor_voltage_outputs = tuple(
            self.thermistor_voltage_output_channel(index, node_id=node_id)
            for index in range(self.thermistor_count)
        )
        self.segment_voltage_output = self.segment_voltage_output_channel(
            node_id=node_id
        )
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_callback=self._sample,
        )
        for path in (*self.cell_voltage_outputs, *self.thermistor_voltage_outputs):
            self._add_output(path)
        self._add_output(self.segment_voltage_output)

    @classmethod
    def cell_voltage_output(
        cls, index: int, *, node_id: int = 0
    ) -> ComponentDataPathOutput:
        return ComponentDataPathOutput(
            lambda _component: cls.cell_voltage_output_channel(index, node_id=node_id)
        )

    @classmethod
    def thermistor_voltage_output(
        cls, index: int, *, node_id: int = 0
    ) -> ComponentDataPathOutput:
        return ComponentDataPathOutput(
            lambda _component: cls.thermistor_voltage_output_channel(
                index, node_id=node_id
            )
        )

    segment_voltage_output = ComponentDataPathOutput(
        lambda component: component.segment_voltage_output
    )

    @classmethod
    def spec(
        cls,
        *,
        platform: str,
        bindings: tuple[ComponentDataPathBinding, ...] = (),
        cell_voltages: tuple[float, ...] | list[float] | None = None,
        temperatures_c: tuple[float, ...] | list[float] | None = None,
        segment_voltage: float | None = None,
        node_id: int = 0,
        scheduler_period: int | float = 1,
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "platform": platform,
                "cell_voltages": cell_voltages,
                "temperatures_c": temperatures_c,
                "segment_voltage": segment_voltage,
                "node_id": node_id,
                "scheduler_period": scheduler_period,
            },
            bindings=bindings,
        )

    @classmethod
    def cell_voltage_output_channel(cls, index: int, *, node_id: int = 0) -> DataPath:
        return DataPath.component(
            cls, (node_id, BmsSegmentPort.CELL_VOLTAGE, index)
        )

    @classmethod
    def thermistor_voltage_output_channel(
        cls, index: int, *, node_id: int = 0
    ) -> DataPath:
        return DataPath.component(
            cls, (node_id, BmsSegmentPort.THERMISTOR_VOLTAGE, index)
        )

    @classmethod
    def segment_voltage_output_channel(cls, *, node_id: int = 0) -> DataPath:
        return DataPath.component(cls, (node_id, BmsSegmentPort.SEGMENT_VOLTAGE))

    @property
    def cell_voltages(self) -> tuple[float, ...]:
        return self._cell_voltages

    @property
    def temperatures_c(self) -> tuple[float, ...]:
        return self._temperatures_c

    @property
    def segment_voltage(self) -> float:
        return self._segment_voltage

    def set_cell_voltage(self, index: int, voltage: float) -> None:
        values = list(self._cell_voltages)
        values[index] = self._finite(voltage, "cell voltage")
        self._cell_voltages = tuple(values)

    def set_temperature(self, index: int, temperature_c: float) -> None:
        values = list(self._temperatures_c)
        values[index] = self._finite(temperature_c, "temperature")
        self._temperatures_c = tuple(values)

    def set_segment_voltage(self, voltage: float) -> None:
        self._segment_voltage = self._finite(voltage, "segment voltage")

    def reset(self) -> None:
        super().reset()
        for queue in self._queues.values():
            queue.clear()

    def _add_output(self, path: DataPath) -> None:
        self._queues[path] = deque()
        self.add_scalar_output(
            path,
            pending=lambda path=path: len(self._queues[path]),
            recv=lambda path=path: (
                self._queues[path].popleft() if self._queues[path] else None
            ),
        )

    def _sample(self, _context: SchedulerContext) -> None:
        for path, value in zip(self.cell_voltage_outputs, self._cell_voltages):
            self._queues[path].append(value)
        for path, temperature in zip(
            self.thermistor_voltage_outputs, self._temperatures_c
        ):
            self._queues[path].append(self._thermistor_voltage(temperature))
        # BMSW's firmware multiplies this ADC input by sixteen when it
        # reconstructs the segment pack voltage.
        self._queues[self.segment_voltage_output].append(self._segment_voltage / 16.0)

    def _thermistor_voltage(self, temperature_c: float) -> float:
        # Both production variants use a 10 kOhm pull-up and 10 kOhm at 25 C.
        b_parameter = 3380.0 if self.platform == "cfr25" else 3435.0
        resistance = 10_000.0 * math.exp(
            b_parameter
            * (1.0 / (temperature_c + 273.15) - 1.0 / (25.0 + 273.15))
        )
        return 3.0 * resistance / (10_000.0 + resistance)

    @staticmethod
    def _finite(value: float, label: str) -> float:
        value = float(value)
        if not math.isfinite(value):
            raise ValueError(f"{label} must be finite")
        return value

    @classmethod
    def _validate_values(
        cls, values, expected: int, label: str
    ) -> tuple[float, ...]:
        values = tuple(cls._finite(value, label) for value in values)
        if len(values) != expected:
            raise ValueError(f"{label} must contain {expected} values")
        return values
