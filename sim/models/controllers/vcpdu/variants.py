from __future__ import annotations

from sim.infra.rig import (
    ClusterCatalog,
    ClusterSpec,
    ModelDataPathOutputConnector,
    NodeSpec,
    PowerControlPath,
)
from sim.models.components.asm330 import Asm330Model
from sim.models.components.battery_source import BatterySourceModel, BatterySourceSpec
from sim.models.components.dc_load import DcLoadModel, DcLoadSpec
from sim.models.platforms import PLATFORM_VARIANTS

from . import (
    AnalogInput,
    DigitalIo,
    SpiDevice,
    TimerChannel,
    TimerPort,
    VcpduModel,
    Vn9008Channel,
)


def vcpdu_node(
    hardware: str | None = None,
    *,
    model_outputs: tuple[ModelDataPathOutputConnector | PowerControlPath, ...] = (),
) -> NodeSpec:
    pump_duty = VcpduModel.timer.duty_events(TimerPort.PWM, TimerChannel._1)
    fan_duty = VcpduModel.timer.duty_events(TimerPort.HP, TimerChannel._2)
    bus_voltage = VcpduModel.bus_voltage_path()
    pump_voltage = DcLoadModel.voltage_input_channel(Vn9008Channel.PUMP)
    fan_voltage = DcLoadModel.voltage_input_channel(Vn9008Channel.FAN)
    pump_current = DcLoadModel.current_output_channel(Vn9008Channel.PUMP)
    fan_current = DcLoadModel.current_output_channel(Vn9008Channel.FAN)
    configured_outputs = (
        *model_outputs,
        VcpduModel.vn9008_load_voltage_output(
            hsd_channel=Vn9008Channel.PUMP,
            timer_path=pump_duty,
            bus_voltage_path=bus_voltage,
            voltage_path=pump_voltage,
        ),
        VcpduModel.vn9008_load_voltage_output(
            hsd_channel=Vn9008Channel.FAN,
            timer_path=fan_duty,
            bus_voltage_path=bus_voltage,
            voltage_path=fan_voltage,
        ),
    )
    components = (
        BatterySourceModel.spec(
            voltage_output_channel=bus_voltage,
            source_spec=BatterySourceSpec(
                voltage=12.0,
                internal_resistance_ohms=0.01,
                rc1_resistance_ohms=0.02,
                rc1_capacitance_farads=1.0,
                rc2_resistance_ohms=0.03,
                rc2_capacitance_farads=10.0,
            ),
            current_drain_channels=(pump_current, fan_current),
            bindings=(
                BatterySourceModel.voltage_output.bind_to(
                    VcpduModel.bus_voltage_input(),
                ),
            ),
        ),
        Asm330Model.spec(
            spi_transactions=VcpduModel.spi.transactions(SpiDevice.IMU),
            chip_select=DigitalIo.SPI_NCS_IMU,
        ),
        DcLoadModel.spec(
            voltage_input_channel=pump_voltage,
            current_output_channel=pump_current,
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
            current_output_channel=fan_current,
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
        model_outputs=configured_outputs,
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
