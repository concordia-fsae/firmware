from __future__ import annotations

from sim.models.catalog import ClusterCatalog, ClusterSpec, NodeSpec
from sim.models.platforms import PLATFORM_VARIANTS

from . import BmsSegmentModel, BmswModel


def bmsw_node(hardware: str) -> NodeSpec:
    series_cells = 14 if hardware == "cfr25" else 11
    thermistors = 20 if hardware == "cfr25" else 9
    segment = BmsSegmentModel.spec(
        platform=hardware,
        bindings=(
            *tuple(
                BmsSegmentModel.cell_voltage_output(index).bind_to(
                    BmswModel.cell_voltage_input(index, platform=hardware)
                )
                for index in range(series_cells)
            ),
            *tuple(
                BmsSegmentModel.thermistor_voltage_output(index).bind_to(
                    BmswModel.thermistor_voltage_input(index, platform=hardware)
                )
                for index in range(thermistors)
            ),
            BmsSegmentModel.segment_voltage_output.bind_to(
                BmswModel.segment_voltage_input(platform=hardware)
            ),
        ),
    )
    return NodeSpec(
        "bmsw",
        BmswModel,
        hardware=hardware,
        components=(segment,),
    )


def bmsw_cluster_spec(hardware: str) -> ClusterSpec:
    return ClusterSpec(
        name=f"{hardware}-bmsw-cluster",
        hardware=hardware,
        nodes=(bmsw_node(hardware),),
    )


BMSW_CLUSTERS = ClusterCatalog(
    *(bmsw_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS)
)
