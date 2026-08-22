from __future__ import annotations

from sim.models.catalog import ClusterCatalog, ClusterSpec
from sim.models.controllers.bmsb.variants import bmsb_node
from sim.models.controllers.bmsw.simple import BMSW_WORKER_COUNT_BY_PLATFORM
from sim.models.controllers.bmsw.variants import bmsw_node
from sim.models.controllers.vcfront.variants import vcfront_node
from sim.models.controllers.vcpdu import Tps2hb16abIc, Tps2hb16abOutput, VcpduModel
from sim.models.controllers.vcpdu.variants import vcpdu_node
from sim.models.controllers.sws.variants import sws_node
from sim.models.controllers.vcrear.variants import vcrear_node
from sim.models.components.drivetrain import DrivetrainModel
from sim.models.platforms import PLATFORM_VARIANTS


def vehicle_cluster_spec(hardware: str) -> ClusterSpec:
    drivetrain_current = DrivetrainModel.current_draw_output_channel("vehicle")
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
            bmsb_node(hardware, include_drivetrain=True),
            # Vehicle tests explicitly model the complete production BMSW
            # topology. Focused BMSW tests use bmsw_cluster_spec's one-node
            # default instead.
            *(
                bmsw_node(
                    hardware,
                    node_id,
                    current_input_channel=drivetrain_current,
                )
                for node_id in range(BMSW_WORKER_COUNT_BY_PLATFORM[hardware])
            ),
            sws_node(hardware),
            vcfront_node(hardware, power_input=vcfront_power),
            vcpdu_node(
                hardware,
                model_outputs=(
                    vcfront_power,
                    vcrear_power,
                ),
            ),
            vcrear_node(
                hardware,
                power_input=vcrear_power,
                drivetrain_output=DrivetrainModel.torque_request_input_channel(
                    "vehicle"
                ),
            ),
        ),
    )


VEHICLE_CLUSTERS = ClusterCatalog(
    *(vehicle_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
