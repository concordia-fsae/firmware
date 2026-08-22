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
)
from sim.infra.rig.datapath import datapath_key
from sim.infra.rig.dataflow import NativeRouteEndpoint
from sim.infra.rig.model import datapath_route_id
from sim.infra.rig.scalar import (
    ScalarRouteEndpoint,
    ScalarStateSinkRouteEndpoint,
)


class BatterySourcePort(Enum):
    VOLTAGE_OUTPUT = auto()
    CURRENT_DRAIN_INPUT = auto()


@dataclass(frozen=True)
class BatterySourceSpec:
    voltage: float
    internal_resistance_ohms: float = 0.0
    capacity_amp_hours: float = math.inf
    rc1_resistance_ohms: float = 0.0
    rc1_capacitance_farads: float = 0.0
    rc2_resistance_ohms: float = 0.0
    rc2_capacitance_farads: float = 0.0

    def __post_init__(self) -> None:
        if not math.isfinite(self.voltage) or self.voltage < 0.0:
            raise ValueError(
                f"battery voltage must be finite and non-negative, got {self.voltage}"
            )
        if (
            not math.isfinite(self.internal_resistance_ohms)
            or self.internal_resistance_ohms < 0.0
        ):
            raise ValueError(
                "battery internal resistance must be finite and non-negative, "
                f"got {self.internal_resistance_ohms}"
            )
        if self.capacity_amp_hours <= 0.0:
            raise ValueError(
                f"battery capacity must be positive, got {self.capacity_amp_hours}"
            )
        for resistance, capacitance, name in (
            (
                self.rc1_resistance_ohms,
                self.rc1_capacitance_farads,
                "rc1",
            ),
            (
                self.rc2_resistance_ohms,
                self.rc2_capacitance_farads,
                "rc2",
            ),
        ):
            if not math.isfinite(resistance) or resistance < 0.0:
                raise ValueError(
                    f"battery {name} resistance must be finite and non-negative, "
                    f"got {resistance}"
                )
            if not math.isfinite(capacitance) or capacitance < 0.0:
                raise ValueError(
                    f"battery {name} capacitance must be finite and non-negative, "
                    f"got {capacitance}"
                )


class BatterySourceModel(ComponentRig):
    voltage_output = ComponentDataPathOutput(
        lambda component: component.voltage_output_channel,
    )

    @classmethod
    def voltage_output_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (BatterySourcePort.VOLTAGE_OUTPUT, channel))

    @classmethod
    def spec(
        cls,
        *,
        voltage_output_channel: DataPath,
        source_spec: BatterySourceSpec,
        current_drain_channels: tuple[DataPath, ...] = (),
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "voltage_output_channel": voltage_output_channel,
                "source_spec": source_spec,
                "current_drain_channels": current_drain_channels,
            },
            bindings=bindings,
        )

    def __init__(
        self,
        *,
        voltage_output_channel: DataPath,
        source_spec: BatterySourceSpec,
        current_drain_channels: tuple[DataPath, ...] = (),
    ) -> None:
        super().__init__()
        self.voltage_output_channel = voltage_output_channel
        self.source_spec = source_spec
        self.current_drain_channels = tuple(current_drain_channels)
        self._voltage = float(source_spec.voltage)
        self.datapaths.add_output(
            self.voltage_output_channel,
            pending=lambda: 0,
            recv=lambda: None,
        )
        for channel in self.current_drain_channels:
            self.datapaths.add_input(channel, send=lambda _value: True)

    @property
    def voltage(self) -> float:
        if self._cluster_rig is not None and self._cluster_node_name is not None:
            record = self._cluster_rig.dataroutes.latest_record(
                self.voltage_output_channel,
                source_node=self._cluster_node_name,
            )
            if record is not None:
                self._voltage = float(record.payload)
        return self._voltage

    def reset(self) -> None:
        super().reset()
        self._voltage = float(self.source_spec.voltage)

    def rust_runtime_model(self) -> bool:
        return self._cluster_rig is not None

    def rust_datapath_route_abi(self, path: DataPath) -> NativeRouteEndpoint | None:
        self._register_native_battery_source()
        if path == self.voltage_output_channel:
            return ScalarRouteEndpoint(*self._scalar_source_route_abi(path))
        if path in self.current_drain_channels:
            return ScalarStateSinkRouteEndpoint(*self._current_drain_route_abi(path))
        return None

    def _scalar_source_route_abi(self, path: DataPath) -> tuple[int, int, int, int]:
        count_callback, recv_callback, send_callback = (
            self._cluster_rig._rust_runtime.noop_scalar_route_abi
            if self._cluster_rig is not None
            else (0, 0, 0)
        )
        route_id = datapath_route_id(datapath_key(path))
        return (route_id, count_callback, recv_callback, send_callback)

    def _current_drain_route_abi(self, path: DataPath) -> tuple[int, float]:
        return (datapath_route_id(datapath_key(path)), 0.0)

    def _register_native_battery_source(self) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return
        register = self._bind_native_model_symbol(
            "rig_model_register_battery_source",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
            ],
        )
        node_index = self._cluster_rig._rust_runtime.node_index(self._cluster_node_name)
        if node_index is None or not register(
            ctypes.c_uint32(node_index),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.voltage_output_channel))
            ),
            ctypes.c_float(self.source_spec.voltage),
            ctypes.c_float(self.source_spec.internal_resistance_ohms),
            ctypes.c_float(self.source_spec.capacity_amp_hours),
            ctypes.c_float(self.source_spec.rc1_resistance_ohms),
            ctypes.c_float(self.source_spec.rc1_capacitance_farads),
            ctypes.c_float(self.source_spec.rc2_resistance_ohms),
            ctypes.c_float(self.source_spec.rc2_capacitance_farads),
        ):
            raise RuntimeError("failed to register native battery source")
