from __future__ import annotations

import ctypes

from sim.infra.rig import ComponentRig, ComponentSpec, DataPath


class Asm330Model(ComponentRig):
    @classmethod
    def spec(cls, *, spi_transactions: DataPath, chip_select: int) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "spi_transactions": spi_transactions,
                "chip_select": chip_select,
            },
        )

    def __init__(self, *, spi_transactions: DataPath, chip_select: int) -> None:
        super().__init__()
        self.spi_transactions = spi_transactions
        self.chip_select = int(chip_select)

    def configure_owner(self, owner: object) -> None:
        binding = self.spi_transactions.peripheral_binding
        if binding is None or binding.device is None:
            raise ValueError("ASM330 model must bind to a SPI transaction datapath")
        bind_zero_model = owner._bind_symbol(
            "rig_runtime_asm330_bind_zero_model",
            [ctypes.c_int, ctypes.c_int],
        )
        bind_zero_model(ctypes.c_int(binding.device), ctypes.c_int(self.chip_select))
