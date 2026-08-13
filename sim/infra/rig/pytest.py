from __future__ import annotations

import pytest

from .catalog import ClusterCatalog
from .cluster import ClusterRig


def cluster_rig_fixture(catalog: ClusterCatalog):
    cases = catalog.pytest_cases()

    @pytest.fixture(
        params=cases,
        ids=lambda cluster: cluster.name,
    )
    def fixture(request) -> ClusterRig:
        rig = request.param.rig()
        yield rig
        rig.reset()

    return fixture
