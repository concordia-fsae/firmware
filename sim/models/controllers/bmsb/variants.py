from __future__ import annotations

from sim.infra.rig import ClusterCatalog, ClusterSpec, NodeSpec, PowerControlPath
from sim.models.platforms import PLATFORM_VARIANTS

from . import BmsbModel


def bmsb_node(
    hardware: str | None = None, *, power_input: PowerControlPath | None = None
) -> NodeSpec:
    return NodeSpec("bmsb", BmsbModel, hardware=hardware, power_input=power_input)


def bmsb_cluster_spec(hardware: str | None = None) -> ClusterSpec:
    prefix = f"{hardware}-" if hardware is not None else ""
    return ClusterSpec(
        name=f"{prefix}bmsb-cluster",
        hardware=hardware,
        nodes=(bmsb_node(hardware),),
    )


BMSB_CLUSTERS = ClusterCatalog(
    *(bmsb_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
