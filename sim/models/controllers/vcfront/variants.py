from __future__ import annotations

from sim.models.catalog import ClusterCatalog, ClusterSpec, NodeSpec
from sim.bindings.power import PowerControlPath
from sim.models.platforms import PLATFORM_VARIANTS

from . import VcfrontModel


def vcfront_node(
    hardware: str | None = None, *, power_input: PowerControlPath | None = None
) -> NodeSpec:
    return NodeSpec("vcfront", VcfrontModel, hardware=hardware, power_input=power_input)


def vcfront_cluster_spec(hardware: str | None = None) -> ClusterSpec:
    prefix = f"{hardware}-" if hardware is not None else ""
    return ClusterSpec(
        name=f"{prefix}vcfront-cluster",
        hardware=hardware,
        nodes=(vcfront_node(hardware),),
    )


VCFRONT_CLUSTERS = ClusterCatalog(
    *(vcfront_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
