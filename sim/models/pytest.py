from __future__ import annotations

from collections.abc import Callable

import pytest

from .catalog import ClusterCatalog
from sim.bindings.core.firmware_cluster import FirmwareClusterRig


def cluster_rig_fixture(
    catalog: ClusterCatalog,
    *,
    setup: Callable[[FirmwareClusterRig], None] | None = None,
):
    cases = catalog.pytest_cases()
    rigs: dict[str, FirmwareClusterRig] = {}

    @pytest.fixture(
        params=cases,
        ids=lambda cluster: cluster.name,
    )
    def fixture(request) -> FirmwareClusterRig:
        cluster = request.param
        rig = rigs.get(cluster.name)
        if rig is None:
            rig = cluster.rig()
            rigs[cluster.name] = rig
        else:
            rig.reset_to_initial_topology()
        if setup is not None:
            setup(rig)
        return rig

    return fixture
