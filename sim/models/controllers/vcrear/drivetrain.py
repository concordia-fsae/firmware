from __future__ import annotations

import ctypes

from sim.infra.rig import ComponentRig, DataPath
from sim.infra.rig.datapath import datapath_key
from sim.infra.rig.model import datapath_route_id
from sim.infra.rig.scalar import ScalarRouteEndpoint


class VcrearDrivetrainCommand(ComponentRig):
    """Expose VCREAR's validated motor command to the simulated drivetrain."""

    def __init__(self, *, output_channel: DataPath, scheduler_period_ms: float = 10):
        super().__init__()
        self.output_channel = output_channel
        self._native_registered = False
        self.datapaths.add_output(output_channel, pending=lambda: 0, recv=lambda: None)

    def reset(self) -> None:
        super().reset()
        self._native_registered = False

    def rust_datapath_route_abi(self, path: DataPath):
        if path != self.output_channel:
            return None
        if not self._native_registered:
            owner = self._owner
            message = owner.can.tx_message("VCREAR_mcCommand", bus="veh")
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
            output_node = self._cluster_rig._rust_runtime.node_index(
                self._cluster_node_name
            )
            can_node = self._cluster_rig._rust_runtime.node_index(
                owner._cluster_node_name
            )
            if output_node is None or can_node is None or not register(
                output_node,
                can_node,
                datapath_route_id(datapath_key(path)),
                message.bus,
                message.id,
                b"VCREAR_torqueCommand",
                ctypes.c_size_t(owner._function_address(owner._decode_can_signal)),
                ctypes.c_uint64(10_000_000),
            ):
                raise RuntimeError("failed to register VCREAR drivetrain command")
            self._native_registered = True
        count, recv, send = self._cluster_rig._rust_runtime.noop_scalar_route_abi
        return ScalarRouteEndpoint(
            datapath_route_id(datapath_key(path)), count, recv, send
        )
