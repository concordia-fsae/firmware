from __future__ import annotations

import os


_PLATFORM_ENV = "SIM_PLATFORM_VARIANTS"


def _load_platform_variants() -> tuple[str, ...]:
    raw = os.environ.get(_PLATFORM_ENV)
    if raw is None:
        raise RuntimeError(
            f"{_PLATFORM_ENV} is not configured; Buck tests must provide the platform catalog"
        )

    platforms = tuple(
        platform.strip() for platform in raw.split(",") if platform.strip()
    )
    if not platforms:
        raise RuntimeError(f"{_PLATFORM_ENV} did not declare any platforms")
    if len(set(platforms)) != len(platforms):
        raise RuntimeError(f"{_PLATFORM_ENV} contains duplicate platforms: {raw}")
    return platforms


PLATFORM_VARIANTS = _load_platform_variants()
