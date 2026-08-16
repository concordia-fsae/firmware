from __future__ import annotations

from enum import IntEnum

from sim.bindings.firmware.can import CanInterface, CanNodeRig, PeriodicCanMessage, SimpleCanComponent


class VcfrontSimpleModel(CanNodeRig):
    """Python-only VCFRONT CAN source for tests that do not need VCFRONT firmware."""

    def __init__(self, can: CanInterface, *, buses: tuple[str, ...] = ("veh",)):
        self.can_component = SimpleCanComponent(can, buses=buses)
        super().__init__(self.can_component)

    def periodic_pedal_position(
        self,
        *,
        period: int | float = 20,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "VCFRONT_pedalPosition",
            bus=bus,
            period=period,
            **signals,
        )

    def send_pedal_position(
        self,
        *,
        bus: str = "veh",
        **signals: float | int | IntEnum,
    ) -> bool:
        return self.can_component.send(
            "VCFRONT_pedalPosition",
            bus=bus,
            **signals,
        )

    def periodic_sleepable(
        self,
        state: IntEnum,
        *,
        period: int | float = 100,
        bus: str = "veh",
    ) -> PeriodicCanMessage:
        return self.can_component.periodic_send(
            "VCFRONT_sleep",
            bus=bus,
            period=period,
            VCFRONT_sleepable=state,
        )
