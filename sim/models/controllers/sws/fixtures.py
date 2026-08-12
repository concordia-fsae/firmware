from __future__ import annotations

from sim.infra.rig import cluster_rig_fixture

from .variants import SWS_CLUSTERS

sws_cluster = cluster_rig_fixture(SWS_CLUSTERS)
