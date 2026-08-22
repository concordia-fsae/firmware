from __future__ import annotations

from sim.models.catalog import (
    ClusterCatalog,
    ClusterSpec,
    NodeSpec,
)
from sim.bindings.firmware.power import PowerControlPath
from sim.models.components.drivetrain import DrivetrainModel
from sim.models.platforms import PLATFORM_VARIANTS

from . import VcrearModel


def vcrear_node(
    hardware: str | None = None,
    *,
    power_input: PowerControlPath | None = None,
    drivetrain_output: object | None = None,
) -> NodeSpec:
    components = (
        (
            DrivetrainModel.can_command_spec(
                output_channel=drivetrain_output,
                message_name="VCREAR_mcCommand",
                signal_name="VCREAR_torqueCommand",
            ),
        )
        if drivetrain_output is not None
        else ()
    )
    return NodeSpec(
        "vcrear",
        VcrearModel,
        hardware=hardware,
        power_input=power_input,
        components=components,
    )


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
