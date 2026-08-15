from __future__ import annotations

from sim.infra.rig import ClusterCatalog, ClusterSpec, NodeSpec, PowerControlPath
from sim.models.components.battery_source import BatterySourceModel, BatterySourceSpec
from sim.models.components.dc_load import DcLoadModel
from sim.models.components.drivetrain import DrivetrainModel, DrivetrainSpec
from sim.models.platforms import PLATFORM_VARIANTS

from . import BmsbModel, BmsbModelExtensions


def bmsb_node(
    hardware: str | None = None,
    *,
    power_input: PowerControlPath | None = None,
    include_drivetrain: bool = False,
) -> NodeSpec:
    battery_voltage = BatterySourceModel.terminal_voltage_output_channel("bmsb-pack")
    load_current = DcLoadModel.current_output_channel("bmsb-load")
    drivetrain_current = DrivetrainModel.current_draw_output_channel("vehicle")
    drivetrain_torque = DrivetrainModel.torque_request_input_channel("vehicle")
    drivetrain_bus_voltage = DrivetrainModel.bus_voltage_output_channel("vehicle")
    drivetrain_torque_output = DrivetrainModel.mechanical_torque_output_channel(
        "vehicle"
    )
    current_drains = (
        (load_current, drivetrain_current)
        if include_drivetrain
        else (load_current,)
    )
    components = [
        BatterySourceModel.spec(
            terminal_voltage_output_channel=battery_voltage,
            source_spec=BatterySourceSpec(
                voltage=350.0,
                internal_resistance_ohms=0.05,
            ),
            current_drain_channels=current_drains,
            bindings=(
                *(() if include_drivetrain else (
                    BatterySourceModel.terminal_voltage_output.bind_to(
                        BmsbModelExtensions.pack_voltage_input()
                    ),
                )),
            ),
        ),
    ]
    if include_drivetrain:
        components.append(
            DrivetrainModel.spec(
                terminal_voltage_input_channel=battery_voltage,
                bus_voltage_output_channel=drivetrain_bus_voltage,
                torque_request_input_channel=drivetrain_torque,
                mechanical_torque_output_channel=drivetrain_torque_output,
                current_draw_output_channel=drivetrain_current,
                drivetrain_spec=DrivetrainSpec(
                    max_torque_nm=200.0,
                    torque_constant_nm_per_amp=1.0,
                ),
                bindings=(
                    DrivetrainModel.current_draw_output.bind_to(
                        BmsbModelExtensions.pack_current_input()
                    ),
                    DrivetrainModel.bus_voltage_output.bind_to(
                        BmsbModelExtensions.pack_voltage_input()
                    ),
                ),
            )
        )
    return NodeSpec(
        "bmsb",
        BmsbModel,
        hardware=hardware,
        power_input=power_input,
        components=tuple(components),
    )


def bmsb_cluster_spec(hardware: str | None = None) -> ClusterSpec:
    prefix = f"{hardware}-" if hardware is not None else ""
    return ClusterSpec(
        name=f"{prefix}bmsb-cluster",
        hardware=hardware,
        nodes=(bmsb_node(hardware),),
    )


BMSB_CLUSTERS = ClusterCatalog(
    *(bmsb_cluster_spec(hardware) for hardware in PLATFORM_VARIANTS),
)
