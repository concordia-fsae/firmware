from __future__ import annotations

from sim.infra.rig import cluster_rig_fixture

from .variants import VCPDU_CLUSTERS

vcpdu_cluster = cluster_rig_fixture(VCPDU_CLUSTERS)
