from __future__ import annotations


class VcfrontPytestHelpers:
    AnalogInput = None
    _APPS1_POINTS = (
        (0, 0.720),
        (5, 0.802),
        (10, 0.882),
        (15, 0.945),
        (20, 1.007),
        (25, 1.059),
        (30, 1.112),
        (35, 1.163),
        (40, 1.209),
        (45, 1.261),
        (50, 1.309),
        (55, 1.351),
        (60, 1.390),
        (65, 1.429),
        (70, 1.469),
        (75, 1.509),
        (80, 1.543),
        (85, 1.576),
        (90, 1.600),
        (95, 1.620),
        (100, 1.628),
    )
    _APPS2_POINTS = (
        (0, 1.475),
        (5, 1.523),
        (10, 1.555),
        (15, 1.584),
        (20, 1.613),
        (25, 1.640),
        (30, 1.666),
        (35, 1.690),
        (40, 1.716),
        (45, 1.738),
        (50, 1.762),
        (55, 1.786),
        (60, 1.809),
        (65, 1.831),
        (70, 1.853),
        (75, 1.875),
        (80, 1.895),
        (85, 1.914),
        (90, 1.928),
        (95, 1.942),
        (100, 1.950),
    )
    _BRAKE_POINTS = (
        (0, 0.3),
        (100, 2.7),
    )

    def set_brake_position(self, position_percent: float) -> None:
        if self.AnalogInput is None:
            raise RuntimeError("VcfrontPytestHelpers.AnalogInput was not configured")
        self.set_analog_input(
            self.AnalogInput.BR_PR,
            self._voltage_for_position(position_percent, self._BRAKE_POINTS),
        )

    def set_accelerator_position(self, position_percent: float) -> None:
        self.set_apps1_position(position_percent)
        self.set_apps2_position(position_percent)

    def set_apps1_position(self, position_percent: float) -> None:
        if self.AnalogInput is None:
            raise RuntimeError("VcfrontPytestHelpers.AnalogInput was not configured")
        self.set_analog_input(
            self.AnalogInput.APPS_P1,
            self._voltage_for_position(position_percent, self._APPS1_POINTS),
        )

    def set_apps2_position(self, position_percent: float) -> None:
        if self.AnalogInput is None:
            raise RuntimeError("VcfrontPytestHelpers.AnalogInput was not configured")
        self.set_analog_input(
            self.AnalogInput.APPS_P2,
            self._voltage_for_position(position_percent, self._APPS2_POINTS),
        )

    def _voltage_for_position(
        self, position_percent: float, points: tuple[tuple[float, float], ...]
    ) -> float:
        if position_percent < 0 or position_percent > 100:
            raise ValueError(
                f"pedal position percent must be in [0, 100], got {position_percent}"
            )

        for index, (left_position, left_voltage) in enumerate(points[:-1]):
            right_position, right_voltage = points[index + 1]
            if position_percent <= right_position:
                span = right_position - left_position
                if span == 0:
                    return right_voltage
                ratio = (position_percent - left_position) / span
                return left_voltage + ((right_voltage - left_voltage) * ratio)

        return points[-1][1]
