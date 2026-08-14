from __future__ import annotations

import ctypes

from enum import Enum, auto

from sim.infra.rig import (
    DataPath,
    ModelDataPathOutputConnector,
    ModelDataPathInputConnector,
    PowerControlEvent,
    PowerControlPath,
    PowerInterface,
)


class VcpduPowerInput(Enum):
    BUS_VOLTAGE = auto()


class VcpduModelExtensions:
    AnalogInput = None
    DigitalIo = None
    SpiDevice = None
    Tps2hb16abIc = None
    Tps2hb16abOutput = None
    Vn9008Channel = None

    def _configure_abi(self) -> None:
        super()._configure_abi()
        if (
            self.AnalogInput is None
            or self.DigitalIo is None
            or self.SpiDevice is None
            or self.Tps2hb16abIc is None
            or self.Tps2hb16abOutput is None
            or self.Vn9008Channel is None
        ):
            raise RuntimeError(
                "VcpduModelExtensions generated enums were not configured"
            )
        self._get_vn9008_cs_amps_per_volt = self._bind_model_symbol(
            "get_vn9008_cs_amps_per_volt",
            [ctypes.c_int],
            ctypes.c_float,
        )
        self._configure_spi_chip_select = self._bind_symbol(
            "rig_runtime_spi_configure_device_chip_select",
            [ctypes.c_int, ctypes.c_int],
        )
        self._configure_spi_chip_select(
            ctypes.c_int(int(self.SpiDevice.IMU)),
            ctypes.c_int(int(self.DigitalIo.SPI_NCS_IMU)),
        )
        self._configure_spi_chip_select(
            ctypes.c_int(int(self.SpiDevice.SD)),
            ctypes.c_int(int(self.DigitalIo.SPI_NCS_SD)),
        )

    def latest_vehicle_state(self):
        return self.can.latest_signal(
            "VCPDU_vehicleState",
            "VCPDU_vehicleState",
            bus="veh",
        )

    def latest_vehicle_state_message(self):
        return self.can.latest(
            "VCPDU_vehicleState",
            bus="veh",
        )

    def record_latest_vehicle_state(self, observed: list) -> object | None:
        state = self.latest_vehicle_state()
        if state is not None and (not observed or observed[-1] != state):
            observed.append(state)
        return state

    def run_until_vehicle_state(
        self,
        state,
        *,
        timeout: int | float,
        unit: str = "ms",
        step: int | float = 1,
        step_unit: str | None = None,
        message: str | None = None,
    ) -> int:
        return self.can.run_until_signal_eq(
            "VCPDU_vehicleState",
            "VCPDU_vehicleState",
            state,
            bus="veh",
            timeout=timeout,
            unit=unit,
            step=step,
            step_unit=step_unit,
            message_on_timeout=message,
        )

    def latest_hsd_duty_cycle(self, hsd_channel) -> float | None:
        signal_name = self._hsd_duty_signal_name(hsd_channel)
        value = self.can.latest_signal("VCPDU_hsdDuty", signal_name, bus="veh")
        return None if value is None else float(value)

    def latest_hsd_current(self, hsd_channel) -> float | None:
        signal_name = self._hsd_current_signal_name(hsd_channel)
        value = self.can.latest_signal("VCPDU_hsdCurrent1", signal_name, bus="veh")
        return None if value is None else float(value)

    def run_until_hsd_output_eq(
        self,
        hsd_channel,
        *,
        duty_cycle: float,
        current: float,
        **kwargs,
    ) -> int:
        self._normalize_run_until_message(kwargs)
        return self.can.run_until_signals_eq(
            (
                ("VCPDU_hsdDuty", self._hsd_duty_signal_name(hsd_channel), duty_cycle),
                ("VCPDU_hsdCurrent1", self._hsd_current_signal_name(hsd_channel), current),
            ),
            bus="veh",
            **kwargs,
        )

    def run_until_hsd_output_gt(
        self,
        hsd_channel,
        *,
        duty_cycle: float,
        current: float,
        **kwargs,
    ) -> int:
        self._normalize_run_until_message(kwargs)
        return self.can.run_until_signals_gt(
            (
                ("VCPDU_hsdDuty", self._hsd_duty_signal_name(hsd_channel), duty_cycle),
                ("VCPDU_hsdCurrent1", self._hsd_current_signal_name(hsd_channel), current),
            ),
            bus="veh",
            **kwargs,
        )

    @staticmethod
    def _normalize_run_until_message(kwargs: dict) -> None:
        message = kwargs.pop("message", None)
        if message is not None:
            kwargs["message_on_timeout"] = message

    def set_vn9008_current_feedback(
        self, hsd_channel, analog_channel, current_amps: float
    ) -> bool:
        amps_per_volt = float(
            self._get_vn9008_cs_amps_per_volt(ctypes.c_int(int(hsd_channel)))
        )
        if amps_per_volt <= 0.0:
            return False
        self.set_analog_input(analog_channel, float(current_amps) / amps_per_volt)
        return True

    @classmethod
    def bus_voltage_input(cls) -> ModelDataPathInputConnector:
        def connect(node, path) -> None:
            node.add_scalar_state_sink(
                path,
                initial_value=0.0,
                sink_id=int(node.AnalogInput.UVL_BATT),
                value_scale=1.0 / 6.62,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)

    @classmethod
    def bus_voltage_path(cls) -> DataPath:
        return DataPath.component(cls, VcpduPowerInput.BUS_VOLTAGE)

    @classmethod
    def vn9008_load_voltage_output(
        cls,
        *,
        hsd_channel,
        timer_path,
        bus_voltage_path,
        voltage_path,
        duty_full_scale: float = 100.0,
    ) -> ModelDataPathOutputConnector:
        def connect(node) -> None:
            node._hsd_signal_name(hsd_channel, pump="pump", fan="fan")
            node.add_timer_scaled_scalar_output(
                voltage_path,
                timer_path=timer_path,
                scale_path=bus_voltage_path,
                scale=1.0 / float(duty_full_scale),
            )

        return ModelDataPathOutputConnector(connect)

    def _hsd_signal_name(self, hsd_channel, *, pump: str, fan: str) -> str:
        if int(hsd_channel) == int(self.Vn9008Channel.PUMP):
            return pump
        if int(hsd_channel) == int(self.Vn9008Channel.FAN):
            return fan
        raise ValueError(f"unsupported VCPDU HSD channel {hsd_channel!r}")

    def _hsd_duty_signal_name(self, hsd_channel) -> str:
        return self._hsd_signal_name(
            hsd_channel,
            pump="VCPDU_pumpDutyCycle",
            fan="VCPDU_fanDutyCycle",
        )

    def _hsd_current_signal_name(self, hsd_channel) -> str:
        return self._hsd_signal_name(
            hsd_channel,
            pump="VCPDU_pumpCurrent",
            fan="VCPDU_fanCurrent",
        )

    @classmethod
    def tps2hb_power_control(cls, ic, output) -> PowerControlPath:
        path = PowerInterface._control_datapath((ic, output))
        hsd_state_signal = cls._tps2hb_hsd_state_signal(ic, output)

        def connect(node) -> None:
            latest_enabled = None
            pending_events = []

            def pending() -> int:
                nonlocal latest_enabled
                if pending_events:
                    return len(pending_events)

                vehicle_state = node.latest_vehicle_state()
                if (
                    vehicle_state is None
                    or vehicle_state == node.can.enums.VehicleState.INIT
                ):
                    return 0

                state = node.can.latest_signal(
                    "VCPDU_hsdState", hsd_state_signal, bus="veh"
                )
                if state is None:
                    return 0

                enabled = state == node.can.enums.HsdState.ON
                if latest_enabled is None or enabled != latest_enabled:
                    latest_enabled = enabled
                    pending_events.append(PowerControlEvent(enabled=enabled))
                return len(pending_events)

            def recv():
                return pending_events.pop(0) if pending_events else None

            node.datapaths.add_output(path, pending=pending, recv=recv)

        return PowerControlPath(path=path, connect=connect)

    @classmethod
    def _tps2hb_hsd_state_signal(cls, ic, output) -> str:
        output_index = int(output)
        output_names = str(ic.name).split("_")
        if output_index >= len(output_names):
            raise ValueError(f"TPS2HB output {output!r} is not present on IC {ic!r}")
        return f"VCPDU_{output_names[output_index].lower()}HsdState"

    @classmethod
    def vn9008_current_feedback(
        cls, *, hsd_channel, analog_input
    ) -> ModelDataPathInputConnector:
        def connect(node, path) -> None:
            amps_per_volt = float(
                node._get_vn9008_cs_amps_per_volt(ctypes.c_int(int(hsd_channel)))
            )
            if amps_per_volt <= 0.0:
                raise ValueError(
                    f"VCPDU HSD channel {hsd_channel!r} has invalid current-sense scale"
                )
            node.add_scalar_sink(
                path,
                sink_id=int(analog_input),
                value_scale=1.0 / amps_per_volt,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)
