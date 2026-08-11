from __future__ import annotations

from sim.infra.rig import (
    ClusterCatalog,
    ClusterSpec,
    ModelDataPathOutputConnector,
    NodeSpec,
    PowerControlPath,
)
from sim.models.components.asm330 import Asm330Model
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec
from sim.models.platforms import PLATFORM_VARIANTS

from . import AnalogInput, SpiDevice, TimerChannel, TimerPort, VcpduModel, Vn9008Channel


def vcpdu_node(
    hardware: str | None = None,
    *,
    model_outputs: tuple[ModelDataPathOutputConnector | PowerControlPath, ...] = (),
) -> NodeSpec:
    pump_voltage = VcpduModel.timer.duty_events(TimerPort.PWM, TimerChannel._1)
    fan_voltage = VcpduModel.timer.duty_events(TimerPort.HP, TimerChannel._2)
    components = (
        Asm330Model.spec(
            spi_transactions=VcpduModel.spi.transactions(SpiDevice.IMU),
        ),
        DcLoadModel.spec(
            voltage_input_channel=pump_voltage,
            load_spec=DcLoadSpec(resistance_ohms=2.0),
            bindings=(
                DcLoadModel.current_output.bind_to(
                    VcpduModel.vn9008_current_feedback(
                        hsd_channel=Vn9008Channel.PUMP,
                        analog_input=AnalogInput.DEMUX2_PUMP,
                    ),
                ),
            ),
        ),
        DcLoadModel.spec(
            voltage_input_channel=fan_voltage,
            load_spec=DcLoadSpec(resistance_ohms=1.0),
            bindings=(
                DcLoadModel.current_output.bind_to(
                    VcpduModel.vn9008_current_feedback(
                        hsd_channel=Vn9008Channel.FAN,
                        analog_input=AnalogInput.DEMUX2_FAN,
                    ),
                ),
            ),
        ),
    )
    return NodeSpec(
        "vcpdu",
        VcpduModel,
        hardware=hardware,
        components=components,
        model_outputs=model_outputs,
    )


def vcpdu_cluster_spec(hardware: str | None = None) -> ClusterSpec:
    prefix = f"{hardware}-" if hardware is not None else ""
    return ClusterSpec(
        name=f"{prefix}vcpdu-cluster",
        hardware=hardware,
        nodes=(vcpdu_node(hardware),),
    )


VCPDU_CLUSTERS = ClusterCatalog(
    *(vcpdu_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
