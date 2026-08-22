from __future__ import annotations

import math
import ctypes
from dataclasses import dataclass
from enum import Enum, auto

from sim.infra.rig import (
    ComponentDataPathBinding,
    ComponentDataPathOutput,
    ComponentSpec,
    ComponentRig,
    DataPath,
    SchedulerContext,
)
from sim.infra.rig.datapath import datapath_key
from sim.infra.rig.dataflow import NativeRouteEndpoint
from sim.infra.rig.model import datapath_route_id
from sim.infra.rig.scalar import (
    ScalarInputRouteEndpoint,
    ScalarRouteEndpoint,
)


class DcLoadPort(Enum):
    VOLTAGE_INPUT = auto()
    CURRENT_OUTPUT = auto()


@dataclass(frozen=True)
class DcLoadSpec:
    resistance_ohms: float | None = None
    inductance_henrys: float | None = 0.0
    capacitance_farads: float | None = 0.0

    def __post_init__(self) -> None:
        for field in ("resistance_ohms", "inductance_henrys", "capacitance_farads"):
            value = getattr(self, field)
            if value is None:
                object.__setattr__(self, field, 0.0)
                value = 0.0
            if math.isnan(value):
                raise ValueError(f"{field} must not be NaN")
            if value < 0.0:
                raise ValueError(f"{field} must not be negative, got {value}")
        if not any(
            _component_present(value)
            for value in (
                self.resistance_ohms,
                self.inductance_henrys,
                self.capacitance_farads,
            )
        ):
            raise ValueError("DC load spec must include at least one L/R/C component")


class DcLoadModel(ComponentRig):
    current_output = ComponentDataPathOutput(
        lambda component: component.current_output_channel,
    )

    @classmethod
    def voltage_input_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (DcLoadPort.VOLTAGE_INPUT, channel))

    @classmethod
    def current_output_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (DcLoadPort.CURRENT_OUTPUT, channel))

    @classmethod
    def spec(
        cls,
        *,
        voltage_input_channel: DataPath,
        current_output_channel: DataPath | None = None,
        load_spec: DcLoadSpec,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "voltage_input_channel": voltage_input_channel,
                "current_output_channel": current_output_channel,
                "load_spec": load_spec,
                "scheduler_period": scheduler_period,
                "scheduler_unit": scheduler_unit,
            },
            bindings=bindings,
        )

    def __init__(
        self,
        *,
        voltage_input_channel: DataPath,
        current_output_channel: DataPath | None = None,
        load_spec: DcLoadSpec,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
    ) -> None:
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_unit=scheduler_unit,
            scheduler_callback=self._update_current,
        )
        self.voltage_input_channel = voltage_input_channel
        self.current_output_channel = current_output_channel or DataPath.component(
            self,
            DcLoadPort.CURRENT_OUTPUT,
        )
        self.load_spec = load_spec
        self._input_voltage = 0.0
        self._output_current = 0.0
        self._inductor_current = 0.0
        self._previous_voltage = 0.0
        self._last_update_ns = 0
        self.datapaths.add_input(
            self.voltage_input_channel,
            send=self._set_voltage,
        )
        self.datapaths.add_output(
            self.current_output_channel,
            pending=lambda: 0,
            recv=lambda: None,
        )

    @property
    def input_voltage(self) -> float:
        return self._input_voltage

    @property
    def output_current(self) -> float:
        if self._cluster_rig is not None and self._cluster_node_name is not None:
            record = self._cluster_rig.dataroutes.latest_record(
                self.current_output_channel,
                source_node=self._cluster_node_name,
            )
            if record is not None:
                self._output_current = float(record.payload)
        return self._output_current

    def reset(self) -> None:
        super().reset()
        self._input_voltage = 0.0
        self._output_current = 0.0
        self._inductor_current = 0.0
        self._previous_voltage = 0.0
        self._last_update_ns = 0

    def _update_current(self, context: SchedulerContext) -> None:
        if self._cluster_rig is not None:
            return
        elapsed_since_update_ns = self.elapsed_ns - self._last_update_ns
        dt_seconds = elapsed_since_update_ns / 1_000_000_000.0
        if dt_seconds <= 0.0:
            return

        self._output_current = self._current_for_step(dt_seconds)
        self._previous_voltage = self._input_voltage
        self._last_update_ns = self.elapsed_ns

    def _current_for_step(self, dt_seconds: float) -> float:
        current = 0.0
        if _component_present(self.load_spec.resistance_ohms):
            current += self.input_voltage / self.load_spec.resistance_ohms
        if _component_present(self.load_spec.inductance_henrys):
            self._inductor_current += (
                self.input_voltage / self.load_spec.inductance_henrys
            ) * dt_seconds
            current += self._inductor_current
        if _component_present(self.load_spec.capacitance_farads):
            current += self.load_spec.capacitance_farads * (
                (self.input_voltage - self._previous_voltage) / dt_seconds
            )
        return current

    def rust_runtime_model(self) -> bool:
        return self._cluster_rig is not None

    def _set_voltage(self, voltage: float | int) -> bool:
        self._input_voltage = max(0.0, float(voltage))
        if self._scheduler_period_ns is None and self._is_static_resistive_load:
            self._output_current = self._current_for_step(0.0)
            self._previous_voltage = self._input_voltage
            self._last_update_ns = self.elapsed_ns
        return True

    @property
    def _is_static_resistive_load(self) -> bool:
        return (
            _component_present(self.load_spec.resistance_ohms)
            and not _component_present(self.load_spec.inductance_henrys)
            and not _component_present(self.load_spec.capacitance_farads)
        )

    def rust_datapath_route_abi(self, path: DataPath) -> NativeRouteEndpoint | None:
        self._register_native_dc_load()
        if path == self.voltage_input_channel:
            return ScalarInputRouteEndpoint(*self._voltage_sink_route_abi(path))
        if path == self.current_output_channel:
            return ScalarRouteEndpoint(*self._scalar_source_route_abi(path))
        return None

    def _voltage_sink_route_abi(self, path: DataPath) -> tuple[int]:
        return (datapath_route_id(datapath_key(path)),)

    def _scalar_source_route_abi(self, path: DataPath) -> tuple[int, int, int, int]:
        count_callback, recv_callback, send_callback = (
            self._cluster_rig._rust_runtime.noop_scalar_route_abi
            if self._cluster_rig is not None
            else (0, 0, 0)
        )
        route_id = datapath_route_id(datapath_key(path))
        return (route_id, count_callback, recv_callback, send_callback)

    def _register_native_dc_load(self) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return
        register = self._bind_native_model_symbol(
            "rig_model_register_dc_load",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_uint64,
            ],
        )
        node_index = self._cluster_rig._rust_runtime.node_index(self._cluster_node_name)
        if node_index is None or not register(
            ctypes.c_uint32(node_index),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.voltage_input_channel))
            ),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.current_output_channel))
            ),
            ctypes.c_float(_native_component_value(self.load_spec.resistance_ohms)),
            ctypes.c_float(_native_component_value(self.load_spec.inductance_henrys)),
            ctypes.c_float(_native_component_value(self.load_spec.capacitance_farads)),
            ctypes.c_uint64(
                0 if self._scheduler_period_ns is None else self._scheduler_period_ns
            ),
        ):
            raise RuntimeError("failed to register native DC load")


def _native_component_value(value: float | None) -> float:
    return 0.0 if value is None else float(value)


def _component_present(value: float | None) -> bool:
    return value is not None and math.isfinite(value) and value > 0.0
