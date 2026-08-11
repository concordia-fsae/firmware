from __future__ import annotations

import ctypes

from sim.infra.rig import ComponentRig, ComponentSpec, DataPath


class Asm330Model(ComponentRig):
    @classmethod
    def spec(cls, *, spi_transactions: DataPath) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "spi_transactions": spi_transactions,
            },
        )

    def __init__(self, *, spi_transactions: DataPath) -> None:
        super().__init__()
        self.spi_transactions = spi_transactions

    def rust_cluster_node_abi(self):
        if self._cluster_rig is None:
            return super().rust_cluster_node_abi()
        runtime = getattr(self._cluster_rig, "_building_rust_runtime", None)
        if runtime is None:
            runtime = self._cluster_rig._rust_runtime
        return runtime.noop_scheduler_abi

    def configure_owner(self, owner: object) -> None:
        binding = self.spi_transactions.peripheral_binding
        if binding is None or binding.device is None:
            raise ValueError("ASM330 model must bind to a SPI transaction datapath")
        bind_zero_model = owner._bind_symbol(
            "rig_runtime_asm330_bind_zero_model",
            [ctypes.c_int],
        )
        bind_zero_model(ctypes.c_int(binding.device))
