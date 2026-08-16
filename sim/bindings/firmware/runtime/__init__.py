"""Public Python interface for the firmware runtime binding.

Implementation modules live below :mod:`sim.bindings.firmware.runtime.python`,
but consumers should import the runtime API from this package.
"""

from typing import Any

from .python import __all__


def __getattr__(name: str) -> Any:
    if name not in __all__:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    from . import python

    value = getattr(python, name)
    globals()[name] = value
    return value
