from __future__ import annotations

from sim.bindings.firmware.can import CanInterface, CanNodeRig, SimpleCanComponent


BMSW_WORKER_COUNT_BY_PLATFORM = {
    "cfr25": 6,
    "cfr26": 8,
}


class BmswSimpleModel(CanNodeRig):
    """Healthy BMS worker CAN source for BMSB SIL tests."""

    def __init__(
        self,
        can: CanInterface,
        worker_count: int,
        *,
        period: int | float = 10,
    ) -> None:
        self.can_component = SimpleCanComponent(can)
        super().__init__(self.can_component)
        self.critical_data = tuple(
            self.can_component.periodic_send(
                f"bmsw{node_id}_criticalData",
                period=period + node_id,
                **self.healthy_critical_data(node_id, worker_count),
            )
            for node_id in range(worker_count)
        )
        checksum_seed = self._checksum_seed_from_yamcan(self.critical_data[0])
        for message in self.critical_data:
            message.packet.data[0] = (
                checksum_seed + sum(message.packet.data[1:])
            ) & 0xFF

    @staticmethod
    def _checksum_seed_from_yamcan(message) -> int:
        """Read the generated YamCan checksum seed from a valid encoded frame."""
        return (message.packet.data[0] - sum(message.packet.data[1:])) & 0xFF

    @staticmethod
    def healthy_critical_data(
        node_id: int, worker_count: int
    ) -> dict[str, float | int]:
        return {
            f"BMSW{node_id}_faultTemp": 1,
            f"BMSW{node_id}_faultBMS": 1,
            f"BMSW{node_id}_tempMax": 25.0,
            f"BMSW{node_id}_segmentVoltage": 350.0 / worker_count,
            f"BMSW{node_id}_voltageMax": 4.2,
            f"BMSW{node_id}_voltageMin": 4.1,
        }


class BmswSimpleCluster:
    def __init__(self, model: BmswSimpleModel, worker_count: int) -> None:
        self.model = model
        self.worker_count = worker_count

    @classmethod
    def for_platform(
        cls,
        can: CanInterface,
        platform: str,
        *,
        period: int | float = 10,
    ) -> BmswSimpleCluster:
        try:
            worker_count = BMSW_WORKER_COUNT_BY_PLATFORM[platform.lower()]
        except KeyError as exc:
            supported = ", ".join(sorted(BMSW_WORKER_COUNT_BY_PLATFORM))
            raise ValueError(
                f"unsupported BMSW platform {platform!r}; expected one of {supported}"
            ) from exc
        return cls(BmswSimpleModel(can, worker_count, period=period), worker_count)

    def add_to(self, cluster) -> None:
        cluster.add_component(self.model)
