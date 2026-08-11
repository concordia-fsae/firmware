from __future__ import annotations


class RunUntilTimeout(AssertionError):
    pass


def duration_to_ns(duration: int | float, *, unit: str = "ms") -> int:
    scale_by_unit = {
        "ns": 1,
        "us": 1_000,
        "ms": 1_000_000,
        "s": 1_000_000_000,
    }
    if duration < 0:
        raise ValueError(f"duration must be non-negative, got {duration}")
    try:
        scale = scale_by_unit[unit]
    except KeyError as exc:
        raise ValueError(
            f"time unit {unit!r} was not found; expected one of {', '.join(scale_by_unit)}"
        ) from exc
    return int(duration * scale)


def run_until(
    run_for,
    predicate,
    *,
    timeout_ns: int,
    step_ns: int = 1_000_000,
    message: str | None = None,
) -> int:
    if timeout_ns < 0:
        raise ValueError(f"timeout must be non-negative, got {timeout_ns} ns")
    if step_ns <= 0:
        raise ValueError(f"step must be positive, got {step_ns} ns")

    elapsed_ns = 0
    if predicate():
        return elapsed_ns

    while elapsed_ns < timeout_ns:
        delta_ns = min(step_ns, timeout_ns - elapsed_ns)
        run_for(delta_ns)
        elapsed_ns += delta_ns
        if predicate():
            return elapsed_ns

    detail = "" if message is None else f": {message}"
    raise RunUntilTimeout(
        f"condition did not become true within {timeout_ns} ns{detail}"
    )
