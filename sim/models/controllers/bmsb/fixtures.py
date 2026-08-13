from __future__ import annotations

from sim.infra.rig import cluster_rig_fixture

from .variants import BMSB_CLUSTERS

bmsb_cluster = cluster_rig_fixture(BMSB_CLUSTERS)
