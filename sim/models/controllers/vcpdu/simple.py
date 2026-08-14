from __future__ import annotations

from enum import IntEnum

from sim.infra.rig import (
    CanInterface,
    PeriodicCanMessage,
)
from sim.infra.models import SimpleCanComponent, SimpleNodeRig


class VcpduSimpleModel(SimpleNodeRig):
    """Python-only VCPDU CAN source for tests that do not need VCPDU firmware."""

    def __init__(self, can: CanInterface, *, buses: tuple[str, ...] = ("veh",)):
        self.can_component = SimpleCanComponent(can, buses=buses)
        super().__init__(self.can_component)

    def periodic_vehicle_state(
        self,
        state: IntEnum,
        *,
        period: int | float = 20,
        bus: str = "veh",
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "VCPDU_vehicleState",
            bus=bus,
            period=period,
            VCPDU_vehicleState=state,
        )
