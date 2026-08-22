from __future__ import annotations

from sim.models.catalog import ClusterCatalog
from sim.models.controllers.bmsb.variants import BMSB_CLUSTERS
from sim.models.controllers.sws.variants import SWS_CLUSTERS
from sim.models.controllers.vcfront.variants import VCFRONT_CLUSTERS
from sim.models.controllers.vcpdu.variants import VCPDU_CLUSTERS
from sim.models.controllers.vcrear.variants import VCREAR_CLUSTERS

CONTROLLER_CLUSTERS = ClusterCatalog(
    *(
        BMSB_CLUSTERS.clusters
        + SWS_CLUSTERS.clusters
        + VCFRONT_CLUSTERS.clusters
        + VCPDU_CLUSTERS.clusters
        + VCREAR_CLUSTERS.clusters
    ),
)
