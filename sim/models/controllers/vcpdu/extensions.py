from __future__ import annotations

import ctypes

from sim.infra.rig import (
    ModelDataPathInputConnector,
    PowerControlEvent,
    PowerControlPath,
    PowerInterface,
)


class VcpduModelExtensions:
    AnalogInput = None
    Tps2hb16abIc = None
    Tps2hb16abOutput = None
    Vn9008Channel = None

    def _configure_abi(self) -> None:
        super()._configure_abi()
        if (
            self.AnalogInput is None
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
        self._allow_sleep = self._bind_model_symbol("allow_sleep")

    def latest_vehicle_state(self):
        return self.can.latest_signal(
            "VCPDU_vehicleState",
            "VCPDU_vehicleState",
            bus="veh",
        )

    def record_latest_vehicle_state(self, observed: list) -> object | None:
        state = self.latest_vehicle_state()
        if state is not None and (not observed or observed[-1] != state):
            observed.append(state)
        return state

    def allow_sleep(self) -> None:
        self._allow_sleep()

    def waking_sleepable_controllers(self) -> tuple[str, ...]:
        controllers = []
        for signal in self.can.rx_signals:
            if not signal.signal_name.endswith("_sleepable"):
                continue
            controller = signal.signal_name.removesuffix("_sleepable").lower()
            if controller not in controllers:
                controllers.append(controller)
        return tuple(controllers)

    def send_waking_controller_sleepable(self, controller: str, state) -> bool:
        prefix = controller.upper()
        message = self.can.message(f"{prefix}_sleep", bus="veh")
        return self.can.send(message, **{f"{prefix}_sleepable": state})

    def send_all_waking_controllers_sleepable(self, state) -> bool:
        return all(
            self.send_waking_controller_sleepable(controller, state)
            for controller in self.waking_sleepable_controllers()
        )

    def request_test_hsd(self, hsd_channel, requested: bool) -> bool:
        signal_name = self._hsd_signal_name(
            hsd_channel,
            pump="SWS_requestTestPump",
            fan="SWS_requestTestFan",
        )
        return self._send_driver_request(signal_name, requested)

    def latest_hsd_duty_cycle(self, hsd_channel) -> float | None:
        signal_name = self._hsd_signal_name(
            hsd_channel,
            pump="VCPDU_pumpDutyCycle",
            fan="VCPDU_fanDutyCycle",
        )
        value = self.can.latest_signal("VCPDU_hsdDuty", signal_name, bus="veh")
        return None if value is None else float(value)

    def latest_hsd_current(self, hsd_channel) -> float | None:
        signal_name = self._hsd_signal_name(
            hsd_channel,
            pump="VCPDU_pumpCurrent",
            fan="VCPDU_fanCurrent",
        )
        value = self.can.latest_signal("VCPDU_hsdCurrent1", signal_name, bus="veh")
        return None if value is None else float(value)

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

    def _send_driver_request(self, signal_name: str, requested: bool) -> bool:
        DigitalStatus = self.can.enums.DigitalStatus
        message = self.can.message("SWS_driverRequest", bus="veh")
        signals = {
            signal.signal_name: DigitalStatus.OFF
            for signal in self.can.rx_signals
            if signal.message_name == message.name
            and signal.enum_name == "digitalStatus"
        }
        signals[signal_name] = DigitalStatus.ON if requested else DigitalStatus.OFF
        return self.can.send(message, **signals)

    def _hsd_signal_name(self, hsd_channel, *, pump: str, fan: str) -> str:
        if int(hsd_channel) == int(self.Vn9008Channel.PUMP):
            return pump
        if int(hsd_channel) == int(self.Vn9008Channel.FAN):
            return fan
        raise ValueError(f"unsupported VCPDU HSD channel {hsd_channel!r}")

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
            node.datapaths.add_input(
                path,
                send=lambda current: node.set_vn9008_current_feedback(
                    hsd_channel,
                    analog_input,
                    current,
                ),
            )

        return ModelDataPathInputConnector(connect)
