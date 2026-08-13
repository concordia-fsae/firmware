from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto

from sim.infra.rig import (
    ComponentDataPathBinding,
    ComponentDataPathOutput,
    ComponentSpec,
    ComponentRig,
    DataPath,
    TimerChannelEvent,
)


class DcLoadPort(Enum):
    CURRENT_OUTPUT = auto()


@dataclass(frozen=True)
class DcLoadSpec:
    resistance_ohms: float | None = None
    inductance_henrys: float | None = None
    capacitance_farads: float | None = None

    def __post_init__(self) -> None:
        if self.resistance_ohms is not None and self.resistance_ohms <= 0.0:
            raise ValueError(f"resistance must be positive, got {self.resistance_ohms}")
        if self.inductance_henrys is not None and self.inductance_henrys <= 0.0:
            raise ValueError(
                f"inductance must be positive, got {self.inductance_henrys}"
            )
        if self.capacitance_farads is not None and self.capacitance_farads <= 0.0:
            raise ValueError(
                f"capacitance must be positive, got {self.capacitance_farads}"
            )
        if (
            self.resistance_ohms is None
            and self.inductance_henrys is None
            and self.capacitance_farads is None
        ):
            raise ValueError("DC load spec must include at least one L/R/C component")


class DcLoadModel(ComponentRig):
    current_output = ComponentDataPathOutput(
        lambda component: component.current_output_channel,
    )

    @classmethod
    def spec(
        cls,
        *,
        voltage_input_channel: DataPath,
        load_spec: DcLoadSpec,
        bindings: tuple[ComponentDataPathBinding, ...] = (),
    ) -> ComponentSpec:
        return ComponentSpec(
            cls,
            parameters={
                "voltage_input_channel": voltage_input_channel,
                "load_spec": load_spec,
            },
            bindings=bindings,
        )

    def __init__(
        self,
        *,
        voltage_input_channel: DataPath,
        current_output_channel: DataPath | None = None,
        load_spec: DcLoadSpec,
    ) -> None:
        super().__init__(
            scheduler_period=1,
            scheduler_unit="ms",
        )
        self.voltage_input_channel = voltage_input_channel
        self.current_output_channel = current_output_channel or DataPath.component(
            self,
            DcLoadPort.CURRENT_OUTPUT,
        )
        self.load_spec = load_spec
        self.input_voltage = 0.0
        self.output_current = 0.0
        self._inductor_current = 0.0
        self._previous_voltage = 0.0
        self._last_update_ns = 0
        self._pending_current = False
        self.datapaths.add_input(
            self.voltage_input_channel,
            send=self._set_voltage_from_timer,
        )
        self.datapaths.add_output(
            self.current_output_channel,
            pending=lambda: 1 if self._pending_current else 0,
            recv=self._recv_current,
        )

    def reset(self) -> None:
        super().reset()
        self.input_voltage = 0.0
        self.output_current = 0.0
        self._inductor_current = 0.0
        self._previous_voltage = 0.0
        self._last_update_ns = 0
        self._pending_current = False

    def _run_scheduled(self) -> None:
        elapsed_since_update_ns = self.elapsed_ns - self._last_update_ns
        dt_seconds = elapsed_since_update_ns / 1_000_000_000.0
        if dt_seconds <= 0.0:
            return

        self.output_current = self._current_for_step(dt_seconds)
        self._previous_voltage = self.input_voltage
        self._last_update_ns = self.elapsed_ns
        self._pending_current = True

    def _current_for_step(self, dt_seconds: float) -> float:
        current = 0.0
        if self.load_spec.resistance_ohms is not None:
            current += self.input_voltage / self.load_spec.resistance_ohms
        if self.load_spec.inductance_henrys is not None:
            self._inductor_current += (
                self.input_voltage / self.load_spec.inductance_henrys
            ) * dt_seconds
            current += self._inductor_current
        if self.load_spec.capacitance_farads is not None:
            current += self.load_spec.capacitance_farads * (
                (self.input_voltage - self._previous_voltage) / dt_seconds
            )
        return current

    def _set_voltage_from_timer(self, event: TimerChannelEvent) -> bool:
        self.input_voltage = max(0.0, float(event.value))
        return True

    def _recv_current(self) -> float | None:
        if not self._pending_current:
            return None
        self._pending_current = False
        return self.output_current
