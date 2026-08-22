from __future__ import annotations

from sim.models.catalog import ClusterCatalog, ClusterSpec, NodeSpec
from sim.bindings.firmware.power import PowerControlPath
from sim.models.platforms import PLATFORM_VARIANTS

from . import SwsModel


def sws_node(
    hardware: str | None = None, *, power_input: PowerControlPath | None = None
) -> NodeSpec:
    return NodeSpec("sws", SwsModel, hardware=hardware, power_input=power_input)


def sws_cluster_spec(hardware: str | None = None) -> ClusterSpec:
    prefix = f"{hardware}-" if hardware is not None else ""
    return ClusterSpec(
        name=f"{prefix}sws-cluster",
        hardware=hardware,
        nodes=(sws_node(hardware),),
    )


SWS_CLUSTERS = ClusterCatalog(
    *(sws_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
