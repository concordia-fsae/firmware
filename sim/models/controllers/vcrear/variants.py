from __future__ import annotations

from sim.infra.rig import ClusterCatalog, ClusterSpec, NodeSpec, PowerControlPath
from sim.models.platforms import PLATFORM_VARIANTS

from . import VcrearModel


def vcrear_node(
    hardware: str | None = None, *, power_input: PowerControlPath | None = None
) -> NodeSpec:
    return NodeSpec("vcrear", VcrearModel, hardware=hardware, power_input=power_input)


def vcrear_cluster_spec(hardware: str | None = None) -> ClusterSpec:
    prefix = f"{hardware}-" if hardware is not None else ""
    return ClusterSpec(
        name=f"{prefix}vcrear-cluster",
        hardware=hardware,
        nodes=(vcrear_node(hardware),),
    )


VCREAR_CLUSTERS = ClusterCatalog(
    *(vcrear_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
