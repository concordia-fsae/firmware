from __future__ import annotations

import ctypes
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
    inductance_henrys: float | None = None
    capacitance_farads: float | None = None

    def __post_init__(self) -> None:
        if self.resistance_ohms is not None and self.resistance_ohms <= 0.0:
            raise ValueError(f"resistance must be positive, got {self.resistance_ohms}")
        if self.inductance_henrys is not None and self.inductance_henrys <= 0.0:
            raise ValueError(
                f"inductance must be positive, got {self.inductance_henrys}"
            )
        if self.capacitance_farads is not None and self.capacitance_farads <= 0.0:
            raise ValueError(
                f"capacitance must be positive, got {self.capacitance_farads}"
            )
        if (
            self.resistance_ohms is None
            and self.inductance_henrys is None
            and self.capacitance_farads is None
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
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "voltage_input_channel": voltage_input_channel,
                "load_spec": load_spec,
            },
            bindings=bindings,
        )

    def __init__(
        self,
        *,
        voltage_input_channel: DataPath,
        current_output_channel: DataPath | None = None,
        load_spec: DcLoadSpec,
    ) -> None:
        super().__init__()
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
        self._timer_route_callbacks = []
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
        if self.load_spec.resistance_ohms is not None:
            current += self.input_voltage / self.load_spec.resistance_ohms
        if self.load_spec.inductance_henrys is not None:
            self._inductor_current += (
                self.input_voltage / self.load_spec.inductance_henrys
            ) * dt_seconds
            current += self._inductor_current
        if self.load_spec.capacitance_farads is not None:
            current += self.load_spec.capacitance_farads * (
                (self.input_voltage - self._previous_voltage) / dt_seconds
            )
        return current

    def _run_for_from_runtime(self, duration_ns: int) -> None:
        if self._cluster_rig is not None:
            self.elapsed_ns += duration_ns
            return
        self.elapsed_ns += duration_ns
        self._run_scheduled()

    def rust_cluster_node_abi(self):
        if self._cluster_rig is None:
            return super().rust_cluster_node_abi()
        runtime = getattr(self._cluster_rig, "_building_rust_runtime", None)
        if runtime is None:
            runtime = self._cluster_rig._rust_runtime
        return runtime.noop_scheduler_abi

    def _set_voltage_from_timer(self, event: TimerChannelEvent) -> bool:
        self._input_voltage = max(0.0, float(event.value))
        return True

    def _recv_current(self) -> float | None:
        return None

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
            capacitance_farads=_native_component_value(self.load_spec.capacitance_farads),
        ):
            raise RuntimeError("failed to register native DC load")

    def _send_timer_events(self, events, count: int) -> int:
        accepted = 0
        for index in range(int(count)):
            if not self._set_voltage_from_timer(events[index]):
                break
            accepted += 1
        return accepted

    _TimerRecvManyCallback = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.c_int32,
        ctypes.c_int32,
        ctypes.POINTER(TimerChannelEvent),
        ctypes.c_uint32,
    )
    _TimerSendManyCallback = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.POINTER(TimerChannelEvent),
        ctypes.c_uint32,
    )


def _native_component_value(value: float | None) -> float:
    return math.inf if value is None else float(value)
