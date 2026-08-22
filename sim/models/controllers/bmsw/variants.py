from __future__ import annotations

from collections.abc import Iterable

from sim.models.catalog import ClusterCatalog, ClusterSpec, NodeSpec
from sim.models.platforms import PLATFORM_VARIANTS

from . import BmsSegmentModel, BmswModel
from .simple import BMSW_WORKER_COUNT_BY_PLATFORM


def bmsw_node(hardware: str, node_id: int = 0) -> NodeSpec:
    hardware = hardware.lower()
    worker_count = BMSW_WORKER_COUNT_BY_PLATFORM.get(hardware)
    if worker_count is None:
        raise ValueError(f"unsupported BMSW platform {hardware!r}")
    if not 0 <= node_id < worker_count:
        raise ValueError(
            f"BMSW node {node_id} is not available on {hardware}; "
            f"expected 0 <= node < {worker_count}"
        )
    series_cells = 14 if hardware == "cfr25" else 11
    thermistors = 20 if hardware == "cfr25" else 9
    segment = BmsSegmentModel.spec(
        platform=hardware,
        node_id=node_id,
        bindings=(
            *tuple(
                BmsSegmentModel.cell_voltage_output(index, node_id=node_id).bind_to(
                    BmswModel.cell_voltage_input(index, platform=hardware)
                )
                for index in range(series_cells)
            ),
            *tuple(
                BmsSegmentModel.thermistor_voltage_output(
                    index, node_id=node_id
                ).bind_to(BmswModel.thermistor_voltage_input(index, platform=hardware))
                for index in range(thermistors)
            ),
            BmsSegmentModel.segment_voltage_output.bind_to(
                BmswModel.segment_voltage_input(platform=hardware)
            ),
        ),
    )
    return NodeSpec(
        f"bmsw{node_id}",
        BmswModel,
        hardware=hardware,
        components=(segment,),
    )


def bmsw_cluster_spec(
    hardware: str,
    *,
    node_ids: Iterable[int] = (0,),
) -> ClusterSpec:
    """Build a BMSW cluster with an explicit node set.

    A single node is the safe default for focused controller tests.  Callers
    that model the complete vehicle topology must pass every node explicitly.
    This prevents a test that only exercises ``bmsw0`` from silently creating
    and scheduling every production BMSW worker.
    """
    return ClusterSpec(
        name=f"{hardware}-bmsw-cluster",
        hardware=hardware,
        nodes=tuple(bmsw_node(hardware, node_id) for node_id in node_ids),
    )


BMSW_CLUSTERS = ClusterCatalog(
    *(bmsw_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS)
)
