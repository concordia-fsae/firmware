from __future__ import annotations

from sim.models.pytest import cluster_rig_fixture

from .variants import VCPDU_CLUSTERS

vcpdu_cluster = cluster_rig_fixture(VCPDU_CLUSTERS)
