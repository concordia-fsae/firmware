from __future__ import annotations

from enum import IntEnum

from sim.bindings.firmware.can import (
    CanInterface,
    CanNodeRig,
    PeriodicCanMessage,
    SimpleCanComponent,
)


class BmsbSimpleModel(CanNodeRig):
    """Python-only BMSB CAN source for tests that do not need BMSB firmware."""

    def __init__(self, can: CanInterface, *, buses: tuple[str, ...] = ("veh",)):
        self.can_component = SimpleCanComponent(can, buses=buses)
        super().__init__(self.can_component)

    def periodic_critical_data(
        self,
        *,
        period: int | float = 10,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "BMSB_criticalData",
            bus=bus,
            period=period,
            **signals,
        )

    def send_critical_data(
        self,
        *,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> bool:
        return self.can_component.send(
            "BMSB_criticalData",
            bus=bus,
            **signals,
        )

    def periodic_pack_contactor_state(
        self,
        state: IntEnum,
        *,
        period: int | float = 100,
        bus: str = "veh",
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "BMSB_information",
            bus=bus,
            period=period,
            BMSB_packContactorState=state,
        )

    def send_pack_contactor_state(
        self,
        state: IntEnum,
        *,
        bus: str = "veh",
    ) -> bool:
        return self.can_component.send(
            "BMSB_information",
            bus=bus,
            BMSB_packContactorState=state,
        )

    def periodic_io_status(
        self,
        *,
        period: int | float = 50,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "BMSB_ioStatus",
            bus=bus,
            period=period,
            enum_defaults={"digitalStatus": "OFF"},
            **signals,
        )

    def send_io_status(
        self,
        *,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> bool:
        return self.can_component.send(
            "BMSB_ioStatus",
            bus=bus,
            enum_defaults={"digitalStatus": "OFF"},
            **signals,
        )


class BmsbDrivetrainSimpleModel(CanNodeRig):
    """PM100 CAN feedback source driven by the simulated drivetrain voltage."""

    def __init__(
        self,
        can: CanInterface,
        *,
        terminal_voltage: float = 350.0,
        period: int | float = 10,
    ) -> None:
        self.can_component = SimpleCanComponent(can)
        self.pm_critical_data = self.can_component.periodic_send(
            "PM100DX_criticalData",
            period=period,
            PM100DX_tractiveSystemVoltage=terminal_voltage,
        )
        super().__init__(self.can_component)
