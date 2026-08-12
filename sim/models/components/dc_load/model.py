from __future__ import annotations

import math
from dataclasses import dataclass
from enum import Enum, auto

from sim.infra.rig import (
    ComponentDataPathBinding,
    ComponentDataPathOutput,
    ComponentSpec,
    ComponentRig,
    DataPath,
    TimerChannelEvent,
)
from sim.infra.rig.datapath import datapath_key
from sim.infra.rig.model import datapath_route_id


class DcLoadPort(Enum):
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
    def spec(
        cls,
        *,
        voltage_input_channel: DataPath,
        load_spec: DcLoadSpec,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "voltage_input_channel": voltage_input_channel,
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
            send=self._set_voltage_from_timer,
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

    def _run_scheduled(self) -> None:
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

    def rust_cluster_node_abi(self):
        if self._cluster_rig is None:
            return super().rust_cluster_node_abi()
        return self._rust_python_scheduler_abi(period_ns=0)

    def _set_voltage_from_timer(self, event: TimerChannelEvent) -> bool:
        self._input_voltage = max(0.0, float(event.value))
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

    def rust_datapath_route_abi(self, path: DataPath) -> tuple[str, tuple[int, ...]] | None:
        self._register_native_dc_load()
        if path == self.voltage_input_channel:
            return ("timer", self._timer_sink_route_abi(path))
        if path == self.current_output_channel:
            return ("scalar", self._scalar_source_route_abi(path))
        return None

    def _timer_sink_route_abi(self, path: DataPath) -> tuple[int, int, int, int, int, int]:
        binding = path.peripheral_binding
        if binding is None or binding.interface not in ("timer.duty", "timer.frequency"):
            raise ValueError(f"datapath {path!r} is not a timer channel")

        count_callback, recv_callback, send_callback = (
            self._cluster_rig._rust_runtime.noop_timer_route_abi
            if self._cluster_rig is not None
            else (0, 0, 0)
        )
        return (
            1 if binding.interface == "timer.duty" else 2,
            int(binding.port if binding.port is not None else 0),
            int(binding.channel if binding.channel is not None else 0),
            count_callback,
            recv_callback,
            send_callback,
        )

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
        binding = self.voltage_input_channel.peripheral_binding
        if binding is None or binding.interface not in ("timer.duty", "timer.frequency"):
            raise ValueError(f"datapath {self.voltage_input_channel!r} is not a timer channel")
        if not self._cluster_rig._rust_runtime.add_dc_load(
            node=self._cluster_node_name,
            current_route_id=datapath_route_id(datapath_key(self.current_output_channel)),
            timer_interface=1 if binding.interface == "timer.duty" else 2,
            timer_port=int(binding.port if binding.port is not None else 0),
            timer_channel=int(binding.channel if binding.channel is not None else 0),
            resistance_ohms=_native_component_value(self.load_spec.resistance_ohms),
            inductance_henrys=_native_component_value(self.load_spec.inductance_henrys),
            capacitance_farads=_native_component_value(
                self.load_spec.capacitance_farads
            ),
            scheduler_period_ns=0
            if self._scheduler_period_ns is None
            else self._scheduler_period_ns,
        ):
            raise RuntimeError("failed to register native DC load")


def _native_component_value(value: float | None) -> float:
    return 0.0 if value is None else float(value)


def _component_present(value: float | None) -> bool:
    return value is not None and math.isfinite(value) and value > 0.0
