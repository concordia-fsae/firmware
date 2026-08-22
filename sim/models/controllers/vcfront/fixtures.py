from __future__ import annotations

from sim.models.pytest import cluster_rig_fixture

from .variants import VCFRONT_CLUSTERS

vcfront_cluster = cluster_rig_fixture(VCFRONT_CLUSTERS)
