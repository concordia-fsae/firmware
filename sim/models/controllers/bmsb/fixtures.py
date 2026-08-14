from __future__ import annotations

from sim.infra.rig import ClusterRig, cluster_rig_fixture

from .variants import BMSB_CLUSTERS


def _configure_bmsb(rig: ClusterRig) -> None:
    rig.bmsb.set_digital_io(rig.bmsb.DigitalInput.VPACK_DIAG, False)


bmsb_cluster = cluster_rig_fixture(BMSB_CLUSTERS, setup=_configure_bmsb)
