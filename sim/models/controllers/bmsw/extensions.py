from __future__ import annotations

from rig import DataPath, ModelDataPathInputConnector
from rig.artifacts import load_generated_module


class BmswModelExtensions:
    """Typed native inputs exposed by the BMS worker firmware model."""

    _platform_enum_modules = {}

    @classmethod
    def cell_voltage_input(
        cls, cell: int, *, platform: str
    ) -> ModelDataPathInputConnector:
        if cell < 0:
            raise ValueError(f"cell index must be non-negative, got {cell}")

        def connect(node, path: DataPath) -> None:
            node.add_scalar_sink(
                path,
                sink_id=int(cls._analog_input_enum(node, platform).CELL1) + cell,
                value_scale=1.0,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)

    @classmethod
    def thermistor_voltage_input(
        cls, sensor: int, *, platform: str
    ) -> ModelDataPathInputConnector:
        if sensor < 0:
            raise ValueError(f"sensor index must be non-negative, got {sensor}")

        def connect(node, path: DataPath) -> None:
            node.add_scalar_sink(
                path,
                sink_id=cls._thermistor_channel(node, sensor, platform),
                value_scale=1.0,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)

    @classmethod
    def segment_voltage_input(cls, *, platform: str) -> ModelDataPathInputConnector:
        def connect(node, path: DataPath) -> None:
            # The firmware multiplies the sampled MAX pack output by two in
            # the ADC path before BatteryMonitoring applies its 16x scale.
            analog_input = cls._analog_input_enum(node, platform)
            node.add_scalar_sink(
                path,
                sink_id=int(analog_input.SEGMENT),
                value_scale=0.5,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)

    @classmethod
    def _thermistor_channel(cls, node, sensor: int, platform: str) -> int:
        analog_input = cls._analog_input_enum(node, platform)
        if sensor >= 20:
            raise ValueError("BMSW supports at most 20 thermistors")
        if sensor < 8:
            return int(analog_input.MUX1_CH1) + sensor
        if platform == "cfr25":
            if sensor < 16:
                return int(analog_input.MUX2_CH1) + sensor - 8
            if sensor < 20:
                return int(analog_input.MUX3_CH1) + sensor - 16
        if sensor == 8:
            return int(analog_input.TEMP_THERM9)
        raise ValueError(f"thermistor {sensor} is not available on this BMSW variant")

    @classmethod
    def _analog_input_enum(cls, node, platform: str):
        platform = platform.lower()
        if platform == "cfr25":
            return node.AnalogInput
        if platform != "cfr26":
            raise ValueError(f"unsupported BMSW platform {platform!r}")
        module = cls._platform_enum_modules.get(platform)
        if module is None:
            module = load_generated_module(
                "BMSW_CFR26_ENUMS_PY",
                "//sim/models/controllers/bmsw:enums-py-cfr26",
                "bmsw_cfr26_generated_enums",
            )
            cls._platform_enum_modules[platform] = module
        return module.AnalogInput
