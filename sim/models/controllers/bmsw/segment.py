from __future__ import annotations

import math
from enum import Enum, auto

from rig import (
    ComponentDataPathBinding,
    ComponentDataPathOutput,
    ComponentRig,
    DataPath,
    SchedulerContext,
)
from rig.datapath import datapath_key
from rig.model import datapath_route_id
from rig.scalar import ScalarRouteEndpoint
from sim.models.catalog import ComponentSpec


class BmsSegmentPort(Enum):
    CURRENT_INPUT = auto()
    CELL_VOLTAGE = auto()
    THERMISTOR_VOLTAGE = auto()
    SEGMENT_VOLTAGE = auto()


class BmsSegmentModel(ComponentRig):
    """Python-configured BMS segment with a native scalar source datapath.

    Python owns the user-facing sensor values, configuration, and one sample
    computation per scheduler timestep. Rig owns the native source bank,
    including event queues, timestamps, and routing to the firmware-backed
    BMSW node.
    """

    def __init__(
        self,
        *,
        platform: str,
        cell_voltages: tuple[float, ...] | list[float] | None = None,
        temperatures_c: tuple[float, ...] | list[float] | None = None,
        segment_voltage: float | None = None,
        current_input_channel: DataPath | None = None,
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
        self.current_input_channel = current_input_channel
        self._current_amps = 0.0
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
        self._source_period_ns = int(float(scheduler_period) * 1_000_000)
        if self._source_period_ns <= 0:
            raise ValueError("scheduler_period must be positive")
        self._output_values: dict[DataPath, float] = {}
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_callback=self._sample,
        )
        if self.current_input_channel is not None:
            self.add_scalar_input(
                self.current_input_channel,
                send=self._receive_current,
            )
        for path in (*self.cell_voltage_outputs, *self.thermistor_voltage_outputs):
            self._add_output(path)
        self._add_output(self.segment_voltage_output)
        self._output_paths = (
            *self.cell_voltage_outputs,
            *self.thermistor_voltage_outputs,
            self.segment_voltage_output,
        )
        self._source_route_ids = tuple(
            datapath_route_id(datapath_key(path)) for path in self._output_paths
        )

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
        current_input_channel: DataPath | None = None,
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
                "current_input_channel": current_input_channel,
                "node_id": node_id,
                "scheduler_period": scheduler_period,
            },
            bindings=bindings,
        )

    @classmethod
    def cell_voltage_output_channel(cls, index: int, *, node_id: int = 0) -> DataPath:
        return DataPath.component(cls, (node_id, BmsSegmentPort.CELL_VOLTAGE, index))

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

    @classmethod
    def current_input_channel(cls, channel: object) -> DataPath:
        """Return a generic scalar current-feedback input channel.

        The owning cluster may provide any compatible current source. Firmware
        composition uses the drivetrain current output, while standalone
        models can provide their own source path.
        """
        return DataPath.component(cls, (BmsSegmentPort.CURRENT_INPUT, channel))

    @property
    def cell_voltages(self) -> tuple[float, ...]:
        return self._cell_voltages

    @property
    def temperatures_c(self) -> tuple[float, ...]:
        return self._temperatures_c

    @property
    def segment_voltage(self) -> float:
        return self._segment_voltage

    @property
    def current_amps(self) -> float:
        """Most recently received drivetrain/load current in amperes."""
        return self._current_amps

    def reset(self) -> None:
        super().reset()
        self._current_amps = 0.0

    def set_cell_voltage(self, index: int, voltage: float) -> None:
        values = list(self._cell_voltages)
        values[index] = self._finite(voltage, "cell voltage")
        self._cell_voltages = tuple(values)
        self._update_output(self.cell_voltage_outputs[index], values[index])

    def set_temperature(self, index: int, temperature_c: float) -> None:
        values = list(self._temperatures_c)
        values[index] = self._finite(temperature_c, "temperature")
        self._temperatures_c = tuple(values)
        self._update_output(
            self.thermistor_voltage_outputs[index],
            self._thermistor_voltage(values[index]),
        )

    def set_segment_voltage(self, voltage: float) -> None:
        self._segment_voltage = self._finite(voltage, "segment voltage")
        self._update_output(
            self.segment_voltage_output,
            self._segment_voltage / 16.0,
        )

    def rust_runtime_model(self) -> bool:
        return False

    def _receive_current(self, current_amps: float) -> bool:
        self._current_amps = self._finite(current_amps, "current")
        return True

    def _sample(self, context: SchedulerContext) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return
        runtime = self._cluster_rig.runtime
        if runtime is None:
            raise RuntimeError("BMS segment requires a Rust cluster runtime")
        values = tuple(self._output_values[path] for path in self._output_paths)
        if not runtime.publish_scalar_source_bank_events(
            node=self._cluster_node_name,
            period_ns=self._source_period_ns,
            timestamp_ns=context.elapsed_ns,
            route_ids=self._source_route_ids,
            values=values,
        ):
            raise RuntimeError("failed to publish native BMS segment sample")

    def _add_output(self, path: DataPath) -> None:
        self._output_values[path] = self._initial_output_value(path)
        # The output is advertised through the normal Rig datapath registry,
        # but its native endpoint is authoritative. These callbacks are only
        # metadata required by the portable datapath contract and are never
        # used by the firmware-backed route.
        self.datapaths.add_output(
            path,
            pending=lambda: 0,
            recv=lambda: None,
        )

    def rust_datapath_route_abi(self, path: DataPath):
        if path == self.current_input_channel:
            return super().rust_datapath_route_abi(path)
        if path not in self._output_values:
            return None
        if self._cluster_rig is None or self._cluster_node_name is None:
            raise RuntimeError("BMS segment native routes require a cluster rig")
        runtime = self._cluster_rig.runtime
        if runtime is None:
            raise RuntimeError("BMS segment native routes require a Rust runtime")
        route_id = datapath_route_id(datapath_key(path))
        if not runtime.add_scalar_source_bank_route(
            node=self._cluster_node_name,
            route_id=route_id,
            period_ns=self._source_period_ns,
            initial_value=self._output_values[path],
        ):
            raise RuntimeError(
                f"failed to register native BMS segment scalar route {route_id}"
            )
        count, recv_many, send_many = runtime.noop_scalar_route_abi
        return ScalarRouteEndpoint(route_id, count, recv_many, send_many)

    def _update_output(self, path: DataPath, value: float) -> None:
        self._output_values[path] = value
        if self._cluster_rig is None or self._cluster_node_name is None:
            return
        runtime = self._cluster_rig.runtime
        if runtime is None:
            return
        route_id = datapath_route_id(datapath_key(path))
        if not runtime.set_scalar_source_bank_value(
            node=self._cluster_node_name,
            route_id=route_id,
            value=value,
        ):
            raise RuntimeError(
                f"failed to update native BMS segment scalar route {route_id}"
            )

    def _initial_output_value(self, path: DataPath) -> float:
        if path in self.cell_voltage_outputs:
            return self._cell_voltages[self.cell_voltage_outputs.index(path)]
        if path in self.thermistor_voltage_outputs:
            index = self.thermistor_voltage_outputs.index(path)
            return self._thermistor_voltage(self._temperatures_c[index])
        if path == self.segment_voltage_output:
            # BMSW's firmware multiplies this ADC input by sixteen when it
            # reconstructs the segment pack voltage.
            return self._segment_voltage / 16.0
        raise KeyError(f"unknown BMS segment output path {path!r}")

    def _thermistor_voltage(self, temperature_c: float) -> float:
        # Both production variants use a 10 kOhm pull-up and 10 kOhm at 25 C.
        b_parameter = 3380.0 if self.platform == "cfr25" else 3435.0
        resistance = 10_000.0 * math.exp(
            b_parameter * (1.0 / (temperature_c + 273.15) - 1.0 / (25.0 + 273.15))
        )
        return 3.0 * resistance / (10_000.0 + resistance)

    @staticmethod
    def _finite(value: float, label: str) -> float:
        value = float(value)
        if not math.isfinite(value):
            raise ValueError(f"{label} must be finite")
        return value

    @classmethod
    def _validate_values(cls, values, expected: int, label: str) -> tuple[float, ...]:
        values = tuple(cls._finite(value, label) for value in values)
        if len(values) != expected:
            raise ValueError(f"{label} must contain {expected} values")
        return values
