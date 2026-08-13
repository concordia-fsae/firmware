from __future__ import annotations

from sim.infra.rig import ClusterCatalog, ClusterSpec
from sim.models.controllers.vcfront.variants import vcfront_node
from sim.models.controllers.vcpdu.variants import vcpdu_node
from sim.models.controllers.vcrear.variants import vcrear_node
from sim.models.platforms import PLATFORM_VARIANTS


def vehicle_cluster_spec(hardware: str) -> ClusterSpec:
    return ClusterSpec(
        name=f"{hardware}-vc-cluster",
        hardware=hardware,
        nodes=(
            vcfront_node(hardware),
            vcpdu_node(hardware),
            vcrear_node(hardware),
        ),
    )


VEHICLE_CLUSTERS = ClusterCatalog(
    *(vehicle_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
