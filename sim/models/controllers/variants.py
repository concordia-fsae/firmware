from __future__ import annotations

from sim.infra.rig import ClusterCatalog
from sim.models.controllers.vcfront.variants import VCFRONT_CLUSTERS
from sim.models.controllers.vcpdu.variants import VCPDU_CLUSTERS
from sim.models.controllers.vcrear.variants import VCREAR_CLUSTERS

CONTROLLER_CLUSTERS = ClusterCatalog(
    *(VCFRONT_CLUSTERS.clusters + VCPDU_CLUSTERS.clusters + VCREAR_CLUSTERS.clusters),
)
