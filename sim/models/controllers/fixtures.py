from __future__ import annotations

from sim.models.pytest import cluster_rig_fixture

from .variants import CONTROLLER_CLUSTERS

controller_cluster = cluster_rig_fixture(CONTROLLER_CLUSTERS)
