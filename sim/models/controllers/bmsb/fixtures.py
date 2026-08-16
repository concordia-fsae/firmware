from __future__ import annotations

from sim.bindings.core.firmware_cluster import FirmwareClusterRig
from sim.models.pytest import cluster_rig_fixture

from . import DigitalIo
from .variants import BMSB_CLUSTERS


def _configure_bmsb(rig: FirmwareClusterRig) -> None:
    for input_ in (
        DigitalIo.VPACK_DIAG,
        DigitalIo.TSMS_CHG,
        DigitalIo.OK_HS,
        DigitalIo.BMS_IMD_RESET,
    ):
        rig.bmsb.set_digital_io(input_, False)


bmsb_cluster = cluster_rig_fixture(BMSB_CLUSTERS, setup=_configure_bmsb)
