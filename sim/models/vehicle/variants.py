from __future__ import annotations

from sim.infra.rig import ClusterCatalog, ClusterSpec
from sim.models.controllers.vcfront.variants import vcfront_node
from sim.models.controllers.vcpdu import Tps2hb16abIc, Tps2hb16abOutput, VcpduModel
from sim.models.controllers.vcpdu.variants import vcpdu_node
from sim.models.controllers.vcrear.variants import vcrear_node
from sim.models.platforms import PLATFORM_VARIANTS


def vehicle_cluster_spec(hardware: str) -> ClusterSpec:
    vcfront_power = VcpduModel.tps2hb_power_control(
        Tps2hb16abIc.VCU1_VCU2, Tps2hb16abOutput._1
    )
    vcrear_power = VcpduModel.tps2hb_power_control(
        Tps2hb16abIc.VC1_VC2, Tps2hb16abOutput._1
    )
    return ClusterSpec(
        name=f"{hardware}-vc-cluster",
        hardware=hardware,
        nodes=(
            vcfront_node(hardware, power_input=vcfront_power),
            vcpdu_node(
                hardware,
                model_outputs=(
                    vcfront_power,
                    vcrear_power,
                ),
            ),
            vcrear_node(hardware, power_input=vcrear_power),
        ),
    )


VEHICLE_CLUSTERS = ClusterCatalog(
    *(vehicle_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
