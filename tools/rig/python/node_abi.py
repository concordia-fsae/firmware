"""Generic ctypes contracts for model-provided Rig datapaths."""

from __future__ import annotations

import ctypes


class ModelDataPathDescriptor(ctypes.Structure):
    """C-compatible model datapath descriptor.

    ``interface`` is opaque to Rig. The consuming binding maps it to its own
    typed peripheral or dataflow interface.
    """

    _fields_ = [
        ("interface", ctypes.c_uint16),
        ("port", ctypes.c_int32),
        ("channel", ctypes.c_int32),
        ("device", ctypes.c_int32),
    ]


__all__ = ["ModelDataPathDescriptor"]
