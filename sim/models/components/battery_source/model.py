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
)
from sim.infra.rig.datapath import datapath_key
from sim.infra.rig.model import datapath_route_id


class BatterySourcePort(Enum):
    VOLTAGE_OUTPUT = auto()


@dataclass(frozen=True)
class BatterySourceSpec:
    voltage: float
    internal_resistance_ohms: float = 0.0
    capacity_amp_hours: float = math.inf

    def __post_init__(self) -> None:
        if not math.isfinite(self.voltage) or self.voltage < 0.0:
            raise ValueError(f"battery voltage must be finite and non-negative, got {self.voltage}")
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
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "voltage_output_channel": voltage_output_channel,
                "source_spec": source_spec,
            },
            bindings=bindings,
        )

    def __init__(
        self,
        *,
        voltage_output_channel: DataPath,
        source_spec: BatterySourceSpec,
    ) -> None:
        super().__init__()
        self.voltage_output_channel = voltage_output_channel
        self.source_spec = source_spec
        self._voltage = float(source_spec.voltage)
        self.datapaths.add_output(
            self.voltage_output_channel,
            pending=lambda: 0,
            recv=lambda: None,
        )

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

    def rust_datapath_route_abi(
        self, path: DataPath
    ) -> tuple[str, tuple[int, ...]] | None:
        self._register_native_battery_source()
        if path == self.voltage_output_channel:
            return ("scalar", self._scalar_source_route_abi(path))
        return None

    def _scalar_source_route_abi(self, path: DataPath) -> tuple[int, int, int, int]:
        count_callback, recv_callback, send_callback = (
            self._cluster_rig._rust_runtime.noop_scalar_route_abi
            if self._cluster_rig is not None
            else (0, 0, 0)
        )
        route_id = datapath_route_id(datapath_key(path))
        return (route_id, count_callback, recv_callback, send_callback)

    def _register_native_battery_source(self) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return
        if not self._cluster_rig._rust_runtime.add_battery_source(
            node=self._cluster_node_name,
            voltage_route_id=datapath_route_id(datapath_key(self.voltage_output_channel)),
            voltage=float(self.source_spec.voltage),
            internal_resistance_ohms=float(self.source_spec.internal_resistance_ohms),
            capacity_amp_hours=float(self.source_spec.capacity_amp_hours),
        ):
            raise RuntimeError("failed to register native battery source")
