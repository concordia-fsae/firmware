from __future__ import annotations

from enum import IntEnum

from sim.infra.rig import (
    CanInterface,
    PeriodicCanMessage,
)
from sim.infra.models import SimpleCanComponent, SimpleNodeRig


class SwsSimpleModel(SimpleNodeRig):
    """Python-only SWS CAN source for tests that do not need steering wheel firmware."""

    def __init__(self, can: CanInterface, *, buses: tuple[str, ...] = ("veh",)):
        self.can_component = SimpleCanComponent(can, buses=buses)
        super().__init__(self.can_component)

    def periodic_sleepable(
        self,
        state: IntEnum,
        *,
        period: int | float = 100,
        bus: str = "veh",
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "SWS_sleep",
            bus=bus,
            period=period,
            SWS_sleepable=state,
        )

    def periodic_driver_request(
        self,
        *,
        period: int | float = 20,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "SWS_driverRequest",
            bus=bus,
            period=period,
            enum_defaults={"digitalStatus": "OFF"},
            **signals,
        )
