from rig import DataPath
from sim.models.controllers.bmsb import BmsbSimpleModel
from sim.models.controllers.bmsb.fixtures import bmsb_cluster
from sim.models.controllers.sws.fixtures import sws_cluster
from sim.models.controllers.vcfront.fixtures import vcfront_cluster
from sim.models.controllers.vcpdu.fixtures import vcpdu_cluster
from sim.models.controllers.vcrear.fixtures import vcrear_cluster


def controller_node(component_cluster):
    controllers = tuple(
        (name, node) for name, node in component_cluster.nodes.items() if node.has_can
    )
    assert len(controllers) == 1
    return controllers[0]


def heartbeat_message_name(node_name):
    return f"{node_name.upper()}_rtosTaskInfo"


def check_can_tx_streams_emit_controller_heartbeat(component_cluster):
    node_name, controller = controller_node(component_cluster)
    message_name = heartbeat_message_name(node_name)
    assert controller.can.bus_count == len(controller.can.buses)

    component_cluster.run_for(1100)

    message = controller.can.tx_message(message_name)
    event = component_cluster.comm.can.latest_message(
        node_name,
        message,
        bus=message.bus,
    )
    assert event is not None, f"expected {message.name} on {message.bus_name}"
    assert event.bus == message.bus
    assert event.timestamp_ns > 0
    assert event.packet.id == message.id
    assert event.packet.len == message.len
    assert len(event.packet.payload) == event.packet.len


def check_can_tx_streams_emit_packets_on_every_bus(component_cluster):
    node_name, controller = controller_node(component_cluster)
    assert controller.can.bus_count == len(controller.can.buses)

    component_cluster.run_for(1100)

    for bus in controller.can.buses:
        event = component_cluster.comm.can.latest_bus_event(node_name, bus)
        assert event is not None, f"expected {node_name} to transmit on {bus.name}"
        assert event.bus == bus.index
        assert event.timestamp_ns > 0
        assert len(event.packet.payload) == event.packet.len


def check_can_rx_stream_consumes_injected_packet(component_cluster):
    node_name, controller = controller_node(component_cluster)

    for bus in controller.can.buses:
        assert controller.can.send(
            bus, 0x7FF, bytes(8)
        ), f"expected {node_name} to accept an injected CAN packet on {bus.name}"
        component_cluster.run_for(1)
        assert (
            controller.can.rx_count(bus) == 0
        ), f"expected {node_name} to consume injected CAN packets on {bus.name}"


def test_vcfront_can_tx_streams_emit_controller_heartbeat(vcfront_cluster):
    check_can_tx_streams_emit_controller_heartbeat(vcfront_cluster)


def test_vcfront_can_tx_streams_emit_packets_on_every_bus(vcfront_cluster):
    check_can_tx_streams_emit_packets_on_every_bus(vcfront_cluster)


def test_vcfront_can_rx_stream_consumes_injected_packet(vcfront_cluster):
    check_can_rx_stream_consumes_injected_packet(vcfront_cluster)


def test_bmsb_can_tx_streams_emit_controller_heartbeat(bmsb_cluster):
    check_can_tx_streams_emit_controller_heartbeat(bmsb_cluster)


def test_bmsb_can_tx_streams_emit_packets_on_every_bus(bmsb_cluster):
    check_can_tx_streams_emit_packets_on_every_bus(bmsb_cluster)


def test_bmsb_can_rx_stream_consumes_injected_packet(bmsb_cluster):
    check_can_rx_stream_consumes_injected_packet(bmsb_cluster)


def test_sws_can_tx_streams_emit_controller_heartbeat(sws_cluster):
    check_can_tx_streams_emit_controller_heartbeat(sws_cluster)


def test_sws_can_tx_streams_emit_packets_on_every_bus(sws_cluster):
    check_can_tx_streams_emit_packets_on_every_bus(sws_cluster)


def test_sws_can_rx_stream_consumes_injected_packet(sws_cluster):
    check_can_rx_stream_consumes_injected_packet(sws_cluster)


def test_vcpdu_can_tx_streams_emit_controller_heartbeat(vcpdu_cluster):
    check_can_tx_streams_emit_controller_heartbeat(vcpdu_cluster)


def test_vcpdu_can_tx_streams_emit_packets_on_every_bus(vcpdu_cluster):
    check_can_tx_streams_emit_packets_on_every_bus(vcpdu_cluster)


def test_vcpdu_can_rx_stream_consumes_injected_packet(vcpdu_cluster):
    check_can_rx_stream_consumes_injected_packet(vcpdu_cluster)


def test_vcrear_can_tx_streams_emit_controller_heartbeat(vcrear_cluster):
    check_can_tx_streams_emit_controller_heartbeat(vcrear_cluster)


def test_vcrear_can_tx_streams_emit_packets_on_every_bus(vcrear_cluster):
    check_can_tx_streams_emit_packets_on_every_bus(vcrear_cluster)


def test_vcrear_can_rx_stream_consumes_injected_packet(vcrear_cluster):
    check_can_rx_stream_consumes_injected_packet(vcrear_cluster)
