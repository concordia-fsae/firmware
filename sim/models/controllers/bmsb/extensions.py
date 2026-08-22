from __future__ import annotations

from rig import ModelDataPathInputConnector


class BmsbModelExtensions:
    @classmethod
    def pack_voltage_input(cls) -> ModelDataPathInputConnector:
        def connect(node, path) -> None:
            node.add_scalar_sink(
                path,
                sink_id=int(node.AnalogInput.VPACK),
                value_scale=1.0,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)

    @classmethod
    def pack_current_input(cls) -> ModelDataPathInputConnector:
        def connect(node, path) -> None:
            node.add_scalar_sink(
                path,
                sink_id=int(node.AnalogInput.CS),
                value_scale=-0.0025,
                set_value=node._set_analog_input,
            )

        return ModelDataPathInputConnector(connect)
