from __future__ import annotations

from sim.infra.rig import ClusterCatalog, ClusterSpec, NodeSpec, PowerControlPath
from sim.models.components.battery_source import BatterySourceModel, BatterySourceSpec
from sim.models.components.dc_load import DcLoadModel
from sim.models.platforms import PLATFORM_VARIANTS

from . import BmsbModel, BmsbModelExtensions


def bmsb_node(
    hardware: str | None = None, *, power_input: PowerControlPath | None = None
) -> NodeSpec:
    battery_voltage = BatterySourceModel.terminal_voltage_output_channel("bmsb-pack")
    load_current = DcLoadModel.current_output_channel("bmsb-load")
    return NodeSpec(
        "bmsb",
        BmsbModel,
        hardware=hardware,
        power_input=power_input,
        components=(
            BatterySourceModel.spec(
                terminal_voltage_output_channel=battery_voltage,
                source_spec=BatterySourceSpec(
                    voltage=350.0,
                    internal_resistance_ohms=0.05,
                ),
                current_drain_channels=(load_current,),
                bindings=(
                    BatterySourceModel.terminal_voltage_output.bind_to(
                        BmsbModelExtensions.pack_voltage_input()
                    ),
                ),
            ),
        ),
    )


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
