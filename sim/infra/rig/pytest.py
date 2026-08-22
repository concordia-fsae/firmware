from __future__ import annotations

import pytest

from .catalog import ClusterCatalog
from .cluster import ClusterRig


def cluster_rig_fixture(catalog: ClusterCatalog):
    cases = catalog.pytest_cases()
    rigs: dict[str, ClusterRig] = {}

    @pytest.fixture(
        params=cases,
        ids=lambda cluster: cluster.name,
    )
    def fixture(request) -> ClusterRig:
        cluster = request.param
        rig = rigs.get(cluster.name)
        if rig is None:
            rig = cluster.rig()
            rigs[cluster.name] = rig
        else:
            rig.reset_to_initial_topology()
        return rig

    return fixture
