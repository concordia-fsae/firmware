from __future__ import annotations

from sim.infra.rig import cluster_rig_fixture

from .variants import VEHICLE_CLUSTERS

vehicle_cluster = cluster_rig_fixture(VEHICLE_CLUSTERS)
