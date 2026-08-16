from __future__ import annotations

from sim.bindings.firmware.can import CanInterface, CanNodeRig, SimpleCanComponent


class VcrearSimpleModel(CanNodeRig):
    """Python-only VCREAR CAN source for tests that do not need VCREAR firmware."""

    def __init__(self, can: CanInterface, *, buses: tuple[str, ...] = ("veh",)):
        self.can_component = SimpleCanComponent(can, buses=buses)
        super().__init__(self.can_component)
