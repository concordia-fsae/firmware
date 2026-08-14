from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import ctypes

import pytest

from enum import IntEnum

from sim.infra.rig.can import (
    CanEnumNamespace,
    CanEvent,
    CanInterface,
    CanMessageDescriptor,
    CanPacket,
    CanSignalDescriptor,
    DecodedCanMessage,
    PeriodicCanMessage,
    python_enum_attr_member,
    python_enum_class_name,
    python_enum_member,
)
from sim.infra.rig.catalog import ClusterCatalog, rig_node_name
from sim.infra.rig.datapath import (
    DataPath,
    PeripheralBinding,
    PeripheralInterface,
    can_datapath_bus,
    is_can_datapath,
    require_peripheral_binding,
)
from sim.infra.rig.scalar import (
    ScalarInputRouteEndpoint,
    ScalarRouteEndpoint,
    ScalarSinkRouteEndpoint,
    ScalarStateSinkRouteEndpoint,
)
from sim.infra.rig.power import PowerControlEvent, PowerControlPath, PowerInterface
from sim.infra.rig.spi import SpiInterface, SpiPeripheralInterface, SpiTransaction
from sim.infra.rig.time import RunUntilTimeout, duration_to_ns, run_until
from sim.infra.rig.timer import (
    TimerChannelEvent,
    TimerInterface,
    TimerPeripheralInterface,
    TimerRouteEndpoint,
)


def test_datapath_namespaces_and_peripheral_bindings_are_typed():
    component = object()
    named = DataPath.named("component", "input")
    component_path = DataPath.component(component, "input")
    can_path = DataPath.can_bus("veh")
    timer_path = DataPath.peripheral(
        "timer",
        binding=PeripheralBinding(
            interface=PeripheralInterface.TIMER_DUTY,
            port=1,
            channel=2,
        ),
    )

    assert named != component_path
    assert is_can_datapath(can_path)
    assert can_datapath_bus(can_path) == "veh"
    assert require_peripheral_binding(timer_path).channel == 2
    with pytest.raises(ValueError):
        require_peripheral_binding(named)
    with pytest.raises(ValueError):
        can_datapath_bus(named)


def test_datapath_component_and_can_factories_cache_identity():
    owner = object()
    assert DataPath.component(owner, "output") is DataPath.component(owner, "output")
    assert DataPath.can_bus("veh") is DataPath.can_bus("veh")


def test_can_packet_and_event_round_trip_payload_and_timestamp():
    packet = CanPacket.from_payload(0x123, b"abc")
    event = CanEvent.from_packet(2, packet, timestamp_ns=17)

    assert packet.id == 0x123
    assert packet.payload == b"abc"
    assert event.bus == 2
    assert event.timestamp_ns == 17
    assert event.packet.payload == b"abc"
    with pytest.raises(ValueError):
        CanPacket.from_payload(1, bytes(9))


def test_can_contract_exposes_first_class_messages_signals_and_enums():
    signal = CanSignalDescriptor(
        0, 0, "veh", "status", 0x123, "state", None, "Enum", "State"
    )
    message = CanMessageDescriptor(0, "veh", "status", 0x123, 2, (signal,))

    class Model:
        _can_messages = (message,)
        _can_tx_messages = _can_messages
        _can_signals = (signal,)
        _can_tx_signals = _can_signals
        _can_enums = CanEnumNamespace({"State": IntEnum("State", {"ON": 1})})

    can = CanInterface(Model())
    assert message.id == 0x123
    assert message.signal("state") == signal
    assert can.enums.State.ON == 1


def test_time_conversion_and_run_until_cover_success_and_timeout():
    assert duration_to_ns(1, unit="ns") == 1
    assert duration_to_ns(1, unit="us") == 1_000
    assert duration_to_ns(1, unit="ms") == 1_000_000
    assert duration_to_ns(1, unit="s") == 1_000_000_000
    with pytest.raises(ValueError):
        duration_to_ns(-1)
    with pytest.raises(ValueError):
        duration_to_ns(1, unit="minutes")

    elapsed = []
    assert run_until(
        lambda delta: elapsed.append(delta),
        lambda: sum(elapsed) >= 5,
        timeout_ns=10,
        step_ns=3,
    ) == 6
    with pytest.raises(RunUntilTimeout, match="deadline"):
        run_until(lambda _delta: None, lambda: False, timeout_ns=2, message="deadline")


class _ClusterName(Enum):
    PRIMARY = 1


@dataclass
class _Cluster:
    name: str


def test_catalog_names_selection_and_enum_node_names():
    catalog = ClusterCatalog(_Cluster("a"), _Cluster("b"))
    assert catalog.names == ("a", "b")
    assert catalog.get("a").name == "a"
    assert catalog.selected("UNSET_TEST_CLUSTER") == catalog.clusters
    assert rig_node_name(_ClusterName.PRIMARY) == "primary"
    with pytest.raises(KeyError, match="expected one of a, b"):
        catalog.get("missing")


class _RouteRuntime:
    def __init__(self):
        self.calls = []

    def add_scalar_route(self, **kwargs):
        self.calls.append(("scalar", kwargs))
        return True

    def add_scalar_input_route(self, **kwargs):
        self.calls.append(("input", kwargs))
        return True

    def add_scalar_state_route(self, **kwargs):
        self.calls.append(("state", kwargs))
        return True

    def add_scalar_sink_route(self, **kwargs):
        self.calls.append(("sink", kwargs))
        return True

    def add_timer_route(self, **kwargs):
        self.calls.append(("timer", kwargs))
        return True


def test_route_endpoints_validate_and_dispatch_homogeneous_contracts():
    runtime = _RouteRuntime()
    source = ScalarRouteEndpoint(route_id=1, count=2, recv_many=3, send_many=4)
    assert source.compatible_with(ScalarInputRouteEndpoint(route_id=1))
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarInputRouteEndpoint(route_id=2),
    )
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarStateSinkRouteEndpoint(route_id=3, initial_value=0.0),
    )
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarSinkRouteEndpoint(
            route_id=1,
            sink_id=4,
            value_scale=0.5,
            set_value=5,
        ),
    )
    assert not source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarSinkRouteEndpoint(
            route_id=9,
            sink_id=4,
            value_scale=0.5,
            set_value=5,
        ),
    )
    timer = TimerRouteEndpoint(1, 2, 3, 4, 5, 6)
    assert timer.compatible_with(TimerRouteEndpoint(1, 2, 3, 7, 8, 9))
    assert timer.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=TimerRouteEndpoint(1, 2, 3, 7, 8, 9),
    )
    assert [kind for kind, _ in runtime.calls] == ["input", "state", "sink", "timer"]


def test_timer_channel_event_has_stable_ffi_shape():
    event = TimerChannelEvent(port=1, channel=2, value=3.5, timestamp_ns=9)
    assert (event.port, event.channel, event.value, event.timestamp_ns) == (
        1,
        2,
        3.5,
        9,
    )


class _Device(IntEnum):
    SENSOR = 3


class _Port(IntEnum):
    MAIN = 1


class _Channel(IntEnum):
    PWM = 2


class _PeripheralModel:
    def __init__(self):
        self.sent_spi = []
        self.sent_timer = []
        self._can_buses = ()
        self._can_messages = ()
        self._can_tx_messages = ()
        self._can_signals = ()
        self._can_tx_signals = ()
        self._can_enums = CanEnumNamespace({})

    def _spi_send(self, pointer):
        self.sent_spi.append(pointer._obj)
        return True

    def _spi_send_many(self, transactions, count):
        self.sent_spi.extend(transactions[: count.value])
        return count.value

    def _spi_recv(self, device, pointer):
        result = SpiTransaction.from_payload(device.value, rx_payload=b"ok")
        output = ctypes.cast(pointer, ctypes.POINTER(SpiTransaction)).contents
        output.device = result.device
        output.rx_len = result.rx_len
        output.rx_data = result.rx_data
        return True

    def _spi_recv_many(self, device, transactions, capacity):
        if capacity.value:
            transactions[0] = SpiTransaction.from_payload(device.value, rx_payload=b"x")
            return 1
        return 0

    def _spi_output_count(self, _device):
        return len(self.sent_spi)

    def _timer_send_duty(self, pointer):
        self.sent_timer.append(pointer._obj)
        return True

    _timer_send_frequency = _timer_send_duty

    def _timer_send_duties(self, events, count):
        self.sent_timer.extend(events[: count.value])
        return count.value

    _timer_send_frequencies = _timer_send_duties

    def _timer_recv_duty(self, _port, _channel, pointer):
        output = ctypes.cast(pointer, ctypes.POINTER(TimerChannelEvent)).contents
        output.port = 1
        output.channel = 2
        output.value = 0.25
        output.timestamp_ns = 4
        return True

    _timer_recv_frequency = _timer_recv_duty

    def _timer_recv_duties(self, _port, _channel, events, capacity):
        if capacity.value:
            events[0] = TimerChannelEvent(port=1, channel=2, value=0.5)
            return 1
        return 0

    _timer_recv_frequencies = _timer_recv_duties

    def _timer_duty_output_count(self, _port, _channel):
        return len(self.sent_timer)

    _timer_frequency_output_count = _timer_duty_output_count

    @staticmethod
    def _function_address(function):
        return id(function)


def test_power_control_path_delivers_typed_events_to_node():
    class Datapaths:
        def add_input(self, path, *, send):
            self.path, self.send = path, send

    class Node:
        def __init__(self):
            self.datapaths = Datapaths()
            self.online = None

        def set_online(self, online):
            self.online = online

    node = Node()
    path = PowerControlPath(PowerInterface._control_datapath(node), lambda _: None)
    PowerInterface.connect_node_input(node, path)
    assert node.datapaths.path == path.path
    assert node.datapaths.send(PowerControlEvent(True, timestamp_ns=8))
    assert node.online is True
    with pytest.raises(TypeError):
        node.datapaths.send(object())


def test_spi_interface_coerces_devices_and_exercises_batch_io():
    model = _PeripheralModel()
    interface = SpiInterface(_Device)
    path = interface.transactions(3)
    peripheral = SpiPeripheralInterface(model)
    assert peripheral.supports(path)
    transaction = SpiTransaction.from_payload(3, tx_payload=[1, 2], rx_payload=b"r", timestamp_ns=9)
    assert transaction.tx_payload == b"\x01\x02"
    assert transaction.rx_payload == b"r"
    assert peripheral.send_payload(path, transaction)
    assert peripheral.send_payloads(path, (transaction,)) == 1
    assert peripheral.recv(path).rx_payload == b"ok"
    assert peripheral.recv_many(path, 2)[0].rx_payload == b"x"
    assert peripheral.recv_many(path, 0) == ()
    assert peripheral.output_count(path) == 2
    with pytest.raises(ValueError, match="valid SPI device"):
        interface.transactions(99)
    with pytest.raises(ValueError, match="at most 256"):
        SpiTransaction.from_payload(1, tx_payload=bytes(257))


def test_timer_interface_coerces_channels_and_exercises_batch_io():
    model = _PeripheralModel()
    interface = TimerInterface(_Port, _Channel)
    path = interface.duty_events(1, 2)
    peripheral = TimerPeripheralInterface(model)
    assert peripheral.supports(path)
    assert peripheral.send(path, value=0.75, timestamp_ns=6)
    event = TimerChannelEvent(port=1, channel=2, value=0.5)
    assert peripheral.send_payload(path, event)
    assert peripheral.send_payloads(path, (event,)) == 1
    assert peripheral.recv(path).value == pytest.approx(0.25)
    assert peripheral.recv_many(path, 2)[0].value == pytest.approx(0.5)
    assert peripheral.recv_many(path, 0) == ()
    assert peripheral.output_count(path) == 3
    assert interface.frequency_events(1, 2).peripheral_binding.interface == PeripheralInterface.TIMER_FREQUENCY
    with pytest.raises(ValueError, match="valid timer channel"):
        interface.duty_events(1, 99)
    with pytest.raises(TypeError, match="TimerChannelEvent"):
        peripheral.send_payload(path, object())


def test_can_helpers_namespace_periodic_updates_and_decoded_access():
    class State(IntEnum):
        OFF = 0

    namespace = CanEnumNamespace({"power_state": State})
    assert namespace["power_state"] is State
    assert namespace.PowerState is State
    assert tuple(namespace) == ("power_state",)
    assert python_enum_member("1st value") == "_1ST_VALUE"
    assert python_enum_attr_member("1st-value") == "_1st_value"
    assert python_enum_class_name("power-state") == "PowerState"

    message = CanMessageDescriptor(0, "bus", "status", 4, 1)
    updates = []
    periodic = PeriodicCanMessage(
        message,
        10,
        {"value": 1},
        lambda _message, signals: CanPacket.from_payload(4, [signals["value"]]),
        native_update=updates.append,
    )
    periodic.set(value=2)
    assert periodic.packet.payload == b"\x02"
    assert len(updates) == 2
    decoded = DecodedCanMessage(message, {"value": 2})
    assert decoded["value"] == 2
    assert decoded.value == 2
    with pytest.raises(AttributeError):
        _ = decoded.missing
