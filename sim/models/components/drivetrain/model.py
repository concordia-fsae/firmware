from __future__ import annotations

import ctypes
import math
from dataclasses import dataclass
from enum import Enum, auto

from sim.models.catalog import ComponentSpec
from rig import ComponentDataPathOutput, ComponentRig, DataPath
from rig.datapath import datapath_key
from rig.dataflow import NativeRouteEndpoint
from rig.model import datapath_route_id
from rig.scalar import ScalarInputRouteEndpoint, ScalarRouteEndpoint


class DrivetrainPort(Enum):
    TERMINAL_VOLTAGE_INPUT = auto()
    BUS_VOLTAGE_OUTPUT = auto()
    TORQUE_REQUEST_INPUT = auto()
    MECHANICAL_TORQUE_OUTPUT = auto()
    CURRENT_DRAW_OUTPUT = auto()


class DrivetrainCanCommand(ComponentRig):
    """Decode a CAN torque command into a drivetrain scalar input.

    The CAN message and signal are supplied by the owning controller model.  The
    drivetrain contract owns the bridge, so controller variants only provide
    generated CAN metadata and a destination datapath.
    """

    def __init__(
        self,
        *,
        output_channel: DataPath,
        message_name: str,
        signal_name: str,
        bus: str = "veh",
        scheduler_period_ms: float = 10,
    ) -> None:
        super().__init__()
        self.output_channel = output_channel
        self.message_name = message_name
        self.signal_name = signal_name
        self.bus = bus
        self.scheduler_period_ms = scheduler_period_ms
        self._native_registered = False
        self.datapaths.add_output(output_channel, pending=lambda: 0, recv=lambda: None)

    def reset(self) -> None:
        super().reset()
        self._native_registered = False

    def rust_datapath_route_abi(self, path: DataPath) -> NativeRouteEndpoint | None:
        if path != self.output_channel:
            return None
        if not self._native_registered:
            owner = self._owner
            message = owner.can.tx_message(self.message_name, bus=self.bus)
            owner._cluster_rig.comm.can.connect_bus(
                owner.can.bus(message.bus),
                nodes=(owner._cluster_node_name,),
            )
            register = self._bind_native_model_symbol(
                "rig_model_register_can_scalar_source",
                [
                    ctypes.c_uint32,
                    ctypes.c_uint32,
                    ctypes.c_uint32,
                    ctypes.c_uint8,
                    ctypes.c_uint32,
                    ctypes.c_char_p,
                    ctypes.c_size_t,
                    ctypes.c_uint64,
                ],
            )
            output_node = self._cluster_rig.runtime.node_index(self._cluster_node_name)
            can_node = self._cluster_rig.runtime.node_index(owner._cluster_node_name)
            period_ns = round(self.scheduler_period_ms * 1_000_000)
            if (
                output_node is None
                or can_node is None
                or not register(
                    output_node,
                    can_node,
                    datapath_route_id(datapath_key(path)),
                    message.bus,
                    message.id,
                    self.signal_name.encode(),
                    ctypes.c_size_t(owner._function_address(owner._decode_can_signal)),
                    ctypes.c_uint64(period_ns),
                )
            ):
                raise RuntimeError("failed to register drivetrain CAN command")
            self._native_registered = True
        count, recv, send = self._cluster_rig.runtime.noop_scalar_route_abi
        return ScalarRouteEndpoint(
            datapath_route_id(datapath_key(path)), count, recv, send
        )


@dataclass(frozen=True)
class DrivetrainSpec:
    max_torque_nm: float
    torque_constant_nm_per_amp: float
    efficiency: float = 1.0
    max_power_w: float = math.inf
    scheduler_period_ms: float = 1.0

    def __post_init__(self) -> None:
        if self.max_torque_nm <= 0.0 or not math.isfinite(self.max_torque_nm):
            raise ValueError("max_torque_nm must be finite and positive")
        if self.torque_constant_nm_per_amp <= 0.0 or not math.isfinite(
            self.torque_constant_nm_per_amp
        ):
            raise ValueError("torque_constant_nm_per_amp must be finite and positive")
        if not 0.0 < self.efficiency <= 1.0 or not math.isfinite(self.efficiency):
            raise ValueError("efficiency must be finite and in (0, 1]")
        if self.max_power_w <= 0.0 or math.isnan(self.max_power_w):
            raise ValueError("max_power_w must be positive")
        if self.scheduler_period_ms <= 0.0 or not math.isfinite(
            self.scheduler_period_ms
        ):
            raise ValueError("scheduler_period_ms must be finite and positive")


class DrivetrainModel(ComponentRig):
    bus_voltage_output = ComponentDataPathOutput(
        lambda component: component.bus_voltage_output_channel,
    )
    mechanical_torque_output = ComponentDataPathOutput(
        lambda component: component.mechanical_torque_output_channel,
    )
    current_draw_output = ComponentDataPathOutput(
        lambda component: component.current_draw_output_channel,
    )

    @classmethod
    def terminal_voltage_input_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (DrivetrainPort.TERMINAL_VOLTAGE_INPUT, channel))

    @classmethod
    def torque_request_input_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (DrivetrainPort.TORQUE_REQUEST_INPUT, channel))

    @classmethod
    def bus_voltage_output_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (DrivetrainPort.BUS_VOLTAGE_OUTPUT, channel))

    @classmethod
    def mechanical_torque_output_channel(cls, channel: object) -> DataPath:
        return DataPath.component(
            cls, (DrivetrainPort.MECHANICAL_TORQUE_OUTPUT, channel)
        )

    @classmethod
    def current_draw_output_channel(cls, channel: object) -> DataPath:
        return DataPath.component(cls, (DrivetrainPort.CURRENT_DRAW_OUTPUT, channel))

    @classmethod
    def can_command_spec(
        cls,
        *,
        output_channel: DataPath,
        message_name: str,
        signal_name: str,
        bus: str = "veh",
        scheduler_period_ms: float = 10,
    ) -> ComponentSpec:
        return ComponentSpec(
            DrivetrainCanCommand,
            parameters={
                "output_channel": output_channel,
                "message_name": message_name,
                "signal_name": signal_name,
                "bus": bus,
                "scheduler_period_ms": scheduler_period_ms,
            },
        )

    @classmethod
    def spec(
        cls,
        *,
        terminal_voltage_input_channel: DataPath,
        bus_voltage_output_channel: DataPath | None = None,
        torque_request_input_channel: DataPath,
        mechanical_torque_output_channel: DataPath,
        current_draw_output_channel: DataPath,
        drivetrain_spec: DrivetrainSpec,
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "terminal_voltage_input_channel": terminal_voltage_input_channel,
                "bus_voltage_output_channel": bus_voltage_output_channel,
                "torque_request_input_channel": torque_request_input_channel,
                "mechanical_torque_output_channel": mechanical_torque_output_channel,
                "current_draw_output_channel": current_draw_output_channel,
                "drivetrain_spec": drivetrain_spec,
            },
            bindings=bindings,
        )

    def __init__(
        self,
        *,
        terminal_voltage_input_channel: DataPath,
        bus_voltage_output_channel: DataPath | None = None,
        torque_request_input_channel: DataPath,
        mechanical_torque_output_channel: DataPath,
        current_draw_output_channel: DataPath,
        drivetrain_spec: DrivetrainSpec,
    ) -> None:
        super().__init__()
        self.terminal_voltage_input_channel = terminal_voltage_input_channel
        self.bus_voltage_output_channel = bus_voltage_output_channel
        self.torque_request_input_channel = torque_request_input_channel
        self.mechanical_torque_output_channel = mechanical_torque_output_channel
        self.current_draw_output_channel = current_draw_output_channel
        self.drivetrain_spec = drivetrain_spec
        self._native_registered = False
        self.datapaths.add_input(
            terminal_voltage_input_channel, send=lambda _value: True
        )
        self.datapaths.add_input(torque_request_input_channel, send=lambda _value: True)
        if bus_voltage_output_channel is not None:
            self.datapaths.add_output(
                bus_voltage_output_channel, pending=lambda: 0, recv=lambda: None
            )
        for path in (mechanical_torque_output_channel, current_draw_output_channel):
            self.datapaths.add_output(path, pending=lambda: 0, recv=lambda: None)

    def reset(self) -> None:
        super().reset()
        self._native_registered = False

    def rust_runtime_model(self) -> bool:
        return self._cluster_rig is not None

    def rust_datapath_route_abi(self, path: DataPath) -> NativeRouteEndpoint | None:
        self._register_native_drivetrain()
        if path in (
            self.terminal_voltage_input_channel,
            self.torque_request_input_channel,
        ):
            return ScalarInputRouteEndpoint(datapath_route_id(datapath_key(path)))
        if path in (
            self.mechanical_torque_output_channel,
            self.current_draw_output_channel,
            self.bus_voltage_output_channel,
        ):
            count, recv, send = self._cluster_rig.runtime.noop_scalar_route_abi
            return ScalarRouteEndpoint(
                datapath_route_id(datapath_key(path)), count, recv, send
            )
        return None

    def _register_native_drivetrain(self) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return
        if self._native_registered:
            return
        register = self._bind_native_model_symbol(
            "rig_model_register_drivetrain",
            [
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint32,
                ctypes.c_uint8,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_float,
                ctypes.c_uint64,
            ],
        )
        node = self._cluster_rig.runtime.node_index(self._cluster_node_name)
        if node is None or not register(
            ctypes.c_uint32(node),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.terminal_voltage_input_channel))
            ),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.torque_request_input_channel))
            ),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.mechanical_torque_output_channel))
            ),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.current_draw_output_channel))
            ),
            ctypes.c_uint32(
                datapath_route_id(datapath_key(self.bus_voltage_output_channel))
                if self.bus_voltage_output_channel is not None
                else 0
            ),
            ctypes.c_uint8(self.bus_voltage_output_channel is not None),
            ctypes.c_float(self.drivetrain_spec.max_torque_nm),
            ctypes.c_float(self.drivetrain_spec.torque_constant_nm_per_amp),
            ctypes.c_float(self.drivetrain_spec.efficiency),
            ctypes.c_float(self.drivetrain_spec.max_power_w),
            ctypes.c_uint64(
                round(self.drivetrain_spec.scheduler_period_ms * 1_000_000)
            ),
        ):
            raise RuntimeError("failed to register native drivetrain")
        self._native_registered = True
