"""Firmware-only composition modules built on the reusable :mod:`rig` API.

This package intentionally exports no symbols. Import a composition object
from its owning module (for example ``sim.bindings.core.firmware_cluster`` or
``sim.models.catalog``). Generic simulation infrastructure belongs to
``rig``; peripheral adapters belong to their package under
``sim.bindings``.
"""
