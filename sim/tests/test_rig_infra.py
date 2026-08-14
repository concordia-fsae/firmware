import ctypes

from sim.infra.rig import (
    CanBusDescriptor,
    CanEvent,
    CanPacket,
    ClusterCanComms,
    ClusterRig,
    ComponentRig,
    DataPath,
    ModelRig,
    PeriodicDataPathProducer,
    SchedulerContext,
    SimpleComponent,
    SimpleNodeRig,
    SpiTransaction,
)
from sim.infra.rig.runtime import _RustClusterRuntime, _StandaloneRustRuntimeHost


class FakeNode(ModelRig):
    def __init__(self) -> None:
        super().__init__()
        self.run_count = 0
        self.reset_count = 0

    def reset(self) -> None:
        self.reset_count += 1

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        self.run_count += duration


class ScheduledComponent(ComponentRig):
    def __init__(self) -> None:
        self.scheduled_times_ns = []
        super().__init__(
            scheduler_period=250,
            scheduler_unit="us",
            scheduler_callback=self._record_scheduled_time,
        )

    def reset(self) -> None:
        super().reset()
        self.scheduled_times_ns.clear()

    def _record_scheduled_time(self, context: SchedulerContext) -> None:
        self.scheduled_times_ns.append(context.elapsed_ns)


class TickModel(ModelRig):
    def __init__(self) -> None:
        self.ticks = 0
        super().__init__(
            scheduler_period=1,
            scheduler_callback=self._tick,
        )

    def reset(self) -> None:
        super().reset()
        self.ticks = 0

    def _tick(self, context: SchedulerContext) -> None:
        self.ticks += 1


class BatchObservedModel(TickModel):
    def __init__(self) -> None:
        super().__init__()
        self.run_durations_ns = []

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        super().run_for(duration, unit=unit)
        self.run_durations_ns.append(int(duration))


class PythonOwner(ModelRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.path = path
        self.pending_payloads = []

    def supports_datapath(self, path: DataPath) -> bool:
        return path == self.path

    def configure_datapath(self, path: DataPath) -> None:
        if self.datapaths.outputs(path):
            return
        self.datapaths.add_output(
            path,
            pending=lambda: len(self.pending_payloads),
            recv=lambda: self.pending_payloads.pop(0)
            if self.pending_payloads
            else None,
        )


class PythonConsumer(ComponentRig):
    def __init__(self, path: DataPath) -> None:
        super().__init__()
        self.received_payloads = []
        self.datapaths.add_input(
            path,
            send=lambda payload: not self.received_payloads.append(payload),
        )


class SharedObjectBackedFakeNode(ModelRig):
    def __init__(self, library_path) -> None:
        super().__init__()
        self.library_path = library_path


class NativeCanNetworkHarness:
    _TxCount = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_uint8)
    _RecvEvents = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.c_uint8,
        ctypes.POINTER(CanEvent),
        ctypes.c_uint32,
    )
    _SendMany = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.c_uint8,
        ctypes.POINTER(CanPacket),
        ctypes.c_uint32,
    )

    def __init__(self) -> None:
        self.pending_by_bus = {}
        self.received_by_bus = {}
        self.tx_count = self._TxCount(self._tx_count)
        self.recv_events = self._RecvEvents(self._recv_events)
        self.send_many = self._SendMany(self._send_many)

    def queue(self, bus: int, packet: CanPacket, *, timestamp_ns: int = 0) -> None:
        self.pending_by_bus.setdefault(int(bus), []).append(
            CanEvent.from_packet(bus, packet, timestamp_ns=timestamp_ns)
        )

    def _tx_count(self, bus: int) -> int:
        return len(self.pending_by_bus.get(int(bus), ()))

    def _recv_events(self, bus: int, events, capacity: int) -> int:
        pending = self.pending_by_bus.get(int(bus), [])
        count = min(len(pending), int(capacity))
        for index in range(count):
            events[index] = pending.pop(0)
        return count

    def _send_many(self, bus: int, packets, count: int) -> int:
        received = self.received_by_bus.setdefault(int(bus), [])
        for index in range(int(count)):
            packet = packets[index]
            received.append(CanPacket.from_payload(packet.id, packet.payload))
        return int(count)


class NativeSpiNetworkHarness:
    _Count = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_int32)
    _RecvMany = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.c_int32,
        ctypes.POINTER(SpiTransaction),
        ctypes.c_uint32,
    )
    _SendMany = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.POINTER(SpiTransaction),
        ctypes.c_uint32,
    )

    def __init__(self) -> None:
        self.pending_by_device = {}
        self.received = []
        self.count = self._Count(self._count)
        self.recv_many = self._RecvMany(self._recv_many)
        self.send_many = self._SendMany(self._send_many)

    def queue(self, transaction: SpiTransaction) -> None:
        self.pending_by_device.setdefault(int(transaction.device), []).append(transaction)

    def _count(self, device: int) -> int:
        return len(self.pending_by_device.get(int(device), ()))

    def _recv_many(self, device: int, transactions, capacity: int) -> int:
        pending = self.pending_by_device.get(int(device), [])
        count = min(len(pending), int(capacity))
        for index in range(count):
            transactions[index] = pending.pop(0)
        return count

    def _send_many(self, transactions, count: int) -> int:
        for index in range(int(count)):
            transaction = transactions[index]
            self.received.append(
                SpiTransaction.from_payload(
                    transaction.device,
                    tx_payload=transaction.tx_payload,
                    rx_payload=transaction.rx_payload,
                    timestamp_ns=transaction.timestamp_ns,
                )
            )
        return int(count)


class MockSpiDevice:
    Responder = ctypes.CFUNCTYPE(
        ctypes.c_bool,
        ctypes.POINTER(SpiTransaction),
        ctypes.POINTER(SpiTransaction),
    )

    def __init__(self, *, response_payload: bytes) -> None:
        self.response_payload = bytes(response_payload)
        self.responder = self.Responder(self._respond)

    def _respond(self, transaction, response) -> bool:
        request = transaction.contents
        if request.rx_len == 0:
            return False
        payload = self.response_payload[: request.rx_len]
        next_response = SpiTransaction.from_payload(
            request.device,
            rx_payload=payload,
            timestamp_ns=request.timestamp_ns,
        )
        response[0] = next_response
        return True


class MockSpiControllerDevice:
    def __init__(self) -> None:
        self.host = _StandaloneRustRuntimeHost()
        self.configure_chip_select = self.host.bind_symbol(
            "rig_runtime_spi_configure_device_chip_select",
            [ctypes.c_int, ctypes.c_int],
        )
        self.configure_responder = self.host.bind_symbol(
            "rig_runtime_spi_configure_responder",
            [ctypes.c_int, MockSpiDevice.Responder],
        )
        self.lock_device = self.host.bind_symbol(
            "rig_runtime_spi_lock_device",
            [ctypes.c_int],
            ctypes.c_bool,
        )
        self.release_device = self.host.bind_symbol(
            "rig_runtime_spi_release_device",
            [ctypes.c_int],
            ctypes.c_bool,
        )
        self.set_digital_io = self.host.bind_symbol(
            "rig_model_set_digital_io",
            [ctypes.c_int, ctypes.c_bool],
        )
        self.push_output = self.host.bind_symbol(
            "rig_runtime_spi_push_output",
            [ctypes.POINTER(SpiTransaction)],
            ctypes.c_bool,
        )
        self.pop_input = self.host.bind_symbol(
            "rig_runtime_spi_pop_input",
            [ctypes.c_int, ctypes.POINTER(SpiTransaction)],
            ctypes.c_bool,
        )

    def configure_device(self, *, device: int, chip_select: int, responder) -> None:
        self.configure_chip_select(ctypes.c_int(device), ctypes.c_int(chip_select))
        self.configure_responder(ctypes.c_int(device), responder)

    def set_chip_select(self, chip_select: int, active: bool) -> None:
        self.set_digital_io(ctypes.c_int(chip_select), ctypes.c_bool(not active))

    def transmit_receive(
        self,
        *,
        device: int,
        tx_payload: bytes = b"\x00",
        rx_len: int = 1,
    ) -> tuple[bool, bytes | None]:
        transaction = SpiTransaction.from_payload(
            device,
            tx_payload=tx_payload,
            rx_payload=bytes(rx_len),
        )
        if not self.push_output(ctypes.byref(transaction)):
            return False, None

        response = SpiTransaction()
        if self.pop_input(ctypes.c_int(device), ctypes.byref(response)):
            return True, response.rx_payload
        return True, bytes([0xFF]) * rx_len


def _function_address(function) -> int:
    return _RustClusterRuntime._function_address(function)


def test_dataflow_graph_rejects_cyclic_algorithms_from_python_abi():
    runtime = _RustClusterRuntime()
    runtime.add_node("node", FakeNode())
    assert runtime.add_scalar_transform_algorithm(
        owner_node="node",
        sort_index=0,
        input_route_id=2,
        output_route_id=1,
    )
    assert runtime.add_scalar_transform_algorithm(
        owner_node="node",
        sort_index=1,
        input_route_id=1,
        output_route_id=2,
    )

    assert not runtime.compile_dataflow_graph()


def test_custom_datapath_routes_between_cluster_nodes():
    source = FakeNode()
    sink = FakeNode()
    same_node_sink = []
    received_payloads = []
    pending_payloads = [
        {"sample": 1, "value": 12.5},
        {"sample": 2, "value": 37.0},
    ]
    debug_path = DataPath(("sensor-debug", "primary"))

    source.datapaths.add_output(
        debug_path,
        pending=lambda: len(pending_payloads),
        recv=lambda: pending_payloads.pop(0) if pending_payloads else None,
    )
    source.datapaths.add_input(
        debug_path,
        send=lambda payload: not same_node_sink.append(payload),
    )
    sink.datapaths.add_input(
        debug_path,
        send=lambda payload: not received_payloads.append(payload),
    )

    cluster = ClusterRig(source=source, sink=sink)
    cluster.run_for(1)

    assert received_payloads == [
        {"sample": 1, "value": 12.5},
        {"sample": 2, "value": 37.0},
    ]
    assert same_node_sink == []
    records = cluster.dataroutes.records(debug_path)
    assert [record.source for record in records] == ["source", "source"]
    assert [record.path for record in records] == [debug_path, debug_path]
    assert [record.timestamp_ns for record in records] == [1_000_000, 1_000_000]


def test_custom_datapath_routes_explicitly_between_paths():
    source = FakeNode()
    sink = FakeNode()
    received_payloads = []
    pending_payloads = ["packet"]
    ethernet_tx = DataPath(("ethernet", "eth0", "tx"))

    source.datapaths.add_output(
        ethernet_tx,
        pending=lambda: len(pending_payloads),
        recv=lambda: pending_payloads.pop(0) if pending_payloads else None,
    )
    cluster = ClusterRig(source=source, sink=sink)
    sink.datapaths.add_input(
        ethernet_tx,
        send=lambda payload: not received_payloads.append(payload),
    )
    cluster.dataroutes.connect_available_paths()
    cluster.run_for(1)

    assert received_payloads == ["packet"]


def test_custom_datapath_routes_batches_when_supported():
    source = FakeNode()
    sink = FakeNode()
    pending_payloads = ["first", "second", "third"]
    received_batches = []
    recv_many_calls = []
    send_many_calls = []
    batched_path = DataPath(("batched", "stream"))

    def recv_many(count: int):
        recv_many_calls.append(count)
        payloads = tuple(pending_payloads[:count])
        del pending_payloads[:count]
        return payloads

    def send_many(payloads):
        send_many_calls.append(payloads)
        received_batches.append(payloads)
        return len(payloads)

    source.datapaths.add_output(
        batched_path,
        pending=lambda: len(pending_payloads),
        recv=lambda: None,
        recv_many=recv_many,
    )
    sink.datapaths.add_input(
        batched_path,
        send=lambda payload: False,
        send_many=send_many,
    )

    cluster = ClusterRig(source=source, sink=sink)
    cluster.run_for(1)

    assert recv_many_calls == [3]
    assert send_many_calls == [("first", "second", "third")]
    assert received_batches == [("first", "second", "third")]
    assert [record.payload for record in cluster.dataroutes.records(batched_path)] == [
        "first",
        "second",
        "third",
    ]


def test_can_node_connections_use_generated_common_bus_names_only():
    source = FakeNode()
    sink = FakeNode()
    veh = CanBusDescriptor(0, "veh")
    nose = CanBusDescriptor(1, "nose")
    source_payloads = {
        veh: ["veh-packet"],
        nose: ["nose-packet"],
    }
    received_payloads = []

    for bus in (veh, nose):
        source.datapaths.add_output(
            ClusterCanComms.path(bus),
            pending=lambda bus=bus: len(source_payloads[bus]),
            recv=lambda bus=bus: source_payloads[bus].pop(0)
            if source_payloads[bus]
            else None,
        )
    for bus in (CanBusDescriptor(0, "veh"),):
        sink.datapaths.add_input(
            ClusterCanComms.path(bus),
            send=lambda payload, bus=bus: not received_payloads.append(
                (bus.name, payload)
            ),
        )

    cluster = ClusterRig(source=source, sink=sink)
    cluster.run_for(1)

    assert received_payloads == [("veh", "veh-packet")]
    assert [
        record.path
        for record in cluster.dataroutes.records(ClusterCanComms.path("veh"))
    ] == [ClusterCanComms.path("veh")]
    assert [
        record.path
        for record in cluster.dataroutes.records(ClusterCanComms.path("nose"))
    ] == [ClusterCanComms.path("nose")]


def test_rust_can_network_fans_out_and_records_latest_message():
    runtime = _RustClusterRuntime()
    runtime.add_node("source", FakeNode())
    runtime.add_node("sink", FakeNode())
    source = NativeCanNetworkHarness()
    sink = NativeCanNetworkHarness()
    packet = CanPacket.from_payload(0x123, b"\x11\x22\x33")
    source.queue(0, packet, timestamp_ns=10)

    assert runtime.add_can_route(
        source_node="source",
        source_bus=0,
        source_tx_count=_function_address(source.tx_count),
        source_recv_events=_function_address(source.recv_events),
        sink_node="sink",
        sink_bus=0,
        sink_send_many=_function_address(sink.send_many),
    )
    runtime.run_for(1_000_000, 1_000_000)

    assert [packet.payload for packet in sink.received_by_bus[0]] == [b"\x11\x22\x33"]
    latest = CanEvent()
    assert runtime.latest_can_message("source", 0, 0x123, latest)
    assert latest.packet.id == 0x123
    assert latest.packet.payload == b"\x11\x22\x33"
    assert latest.timestamp_ns == 10
    latest_on_bus = CanEvent()
    assert runtime.latest_can_bus_event("source", 0, latest_on_bus)
    assert latest_on_bus.packet.id == 0x123


def test_rust_can_network_records_source_only_events_without_sink():
    runtime = _RustClusterRuntime()
    runtime.add_node("source", FakeNode())
    source = NativeCanNetworkHarness()
    source.queue(1, CanPacket.from_payload(0x456, b"\xaa"))

    assert runtime.add_can_route(
        source_node="source",
        source_bus=1,
        source_tx_count=_function_address(source.tx_count),
        source_recv_events=_function_address(source.recv_events),
    )
    runtime.run_for(1_000_000, 1_000_000)

    latest = CanEvent()
    assert runtime.latest_can_message("source", 1, 0x456, latest)
    assert latest.packet.payload == b"\xaa"


def test_rust_can_network_rejects_invalid_routes_and_gates_offline_sources():
    runtime = _RustClusterRuntime()
    runtime.add_node("source", FakeNode())
    runtime.add_node("sink", FakeNode())
    source = NativeCanNetworkHarness()
    sink = NativeCanNetworkHarness()
    source.queue(0, CanPacket.from_payload(0x321, b"\x01"))

    assert not runtime.add_can_route(
        source_node="missing",
        source_bus=0,
        source_tx_count=_function_address(source.tx_count),
        source_recv_events=_function_address(source.recv_events),
        sink_node="sink",
        sink_bus=0,
        sink_send_many=_function_address(sink.send_many),
    )
    assert not runtime.add_can_route(
        source_node="source",
        source_bus=0,
        source_tx_count=_function_address(source.tx_count),
        source_recv_events=_function_address(source.recv_events),
        sink_node="missing",
        sink_bus=0,
        sink_send_many=_function_address(sink.send_many),
    )
    assert not runtime.add_can_route(
        source_node="source",
        source_bus=0,
        source_tx_count=0,
        source_recv_events=_function_address(source.recv_events),
        sink_node="sink",
        sink_bus=0,
        sink_send_many=_function_address(sink.send_many),
    )
    assert not runtime.add_can_route(
        source_node="source",
        source_bus=0,
        source_tx_count=_function_address(source.tx_count),
        source_recv_events=_function_address(source.recv_events),
        sink_send_many=_function_address(sink.send_many),
    )

    assert runtime.add_can_route(
        source_node="source",
        source_bus=0,
        source_tx_count=_function_address(source.tx_count),
        source_recv_events=_function_address(source.recv_events),
        sink_node="sink",
        sink_bus=0,
        sink_send_many=_function_address(sink.send_many),
    )
    runtime.set_node_online("source", False)
    runtime.run_for(1_000_000, 1_000_000)
    assert sink.received_by_bus == {}
    assert not runtime.latest_can_message("source", 0, 0x321, CanEvent())

    runtime.set_node_online("source", True)
    runtime.run_for(1_000_000, 1_000_000)
    assert [packet.payload for packet in sink.received_by_bus[0]] == [b"\x01"]


def test_rust_spi_network_fans_out_by_device_immediately():
    runtime = _RustClusterRuntime()
    runtime.add_node("source", FakeNode())
    runtime.add_node("sink", FakeNode())
    source = NativeSpiNetworkHarness()
    sink = NativeSpiNetworkHarness()
    source.queue(
        SpiTransaction.from_payload(
            7,
            tx_payload=b"\x9a\xbc",
            rx_payload=b"\x55",
            timestamp_ns=123,
        )
    )

    assert runtime.add_spi_route(
        source_node="source",
        device=7,
        source_count=_function_address(source.count),
        source_recv_many=_function_address(source.recv_many),
        sink_node="sink",
        sink_send_many=_function_address(sink.send_many),
    )
    runtime.run_for(1_000_000, 1_000_000)

    assert [(tx.device, tx.tx_payload, tx.rx_payload) for tx in sink.received] == [
        (7, b"\x9a\xbc", b"\x55")
    ]


def test_rust_spi_network_rejects_invalid_routes_and_gates_offline_sources():
    runtime = _RustClusterRuntime()
    runtime.add_node("source", FakeNode())
    runtime.add_node("sink", FakeNode())
    source = NativeSpiNetworkHarness()
    sink = NativeSpiNetworkHarness()
    source.queue(SpiTransaction.from_payload(2, tx_payload=b"\x01"))

    assert not runtime.add_spi_route(
        source_node="missing",
        device=2,
        source_count=_function_address(source.count),
        source_recv_many=_function_address(source.recv_many),
        sink_node="sink",
        sink_send_many=_function_address(sink.send_many),
    )
    assert not runtime.add_spi_route(
        source_node="source",
        device=2,
        source_count=_function_address(source.count),
        source_recv_many=_function_address(source.recv_many),
        sink_node="missing",
        sink_send_many=_function_address(sink.send_many),
    )
    assert not runtime.add_spi_route(
        source_node="source",
        device=2,
        source_count=0,
        source_recv_many=_function_address(source.recv_many),
        sink_node="sink",
        sink_send_many=_function_address(sink.send_many),
    )

    assert runtime.add_spi_route(
        source_node="source",
        device=2,
        source_count=_function_address(source.count),
        source_recv_many=_function_address(source.recv_many),
        sink_node="sink",
        sink_send_many=_function_address(sink.send_many),
    )
    runtime.set_node_online("source", False)
    runtime.run_for(1_000_000, 1_000_000)
    assert sink.received == []

    runtime.set_node_online("source", True)
    runtime.run_for(1_000_000, 1_000_000)
    assert [(tx.device, tx.tx_payload) for tx in sink.received] == [(2, b"\x01")]


def test_runtime_spi_chip_select_gates_mock_device_response():
    controller = MockSpiControllerDevice()
    mock_device = MockSpiDevice(response_payload=b"\xa5")
    device = 107
    chip_select = 1007
    controller.configure_device(
        device=device,
        chip_select=chip_select,
        responder=mock_device.responder,
    )

    ok, payload = controller.transmit_receive(device=device)
    assert ok
    assert payload == b"\xff"

    assert controller.lock_device(ctypes.c_int(device))
    ok, payload = controller.transmit_receive(device=device)
    assert ok
    assert payload == b"\xa5"
    assert controller.release_device(ctypes.c_int(device))

    ok, payload = controller.transmit_receive(device=device)
    assert ok
    assert payload == b"\xff"


def test_runtime_spi_chip_select_rejects_unconfigured_and_multi_selected_devices():
    controller = MockSpiControllerDevice()
    first_device = 117
    first_chip_select = 1017
    second_device = 118
    second_chip_select = 1018
    first_mock = MockSpiDevice(response_payload=b"\x11")
    second_mock = MockSpiDevice(response_payload=b"\x22")
    controller.configure_device(
        device=first_device,
        chip_select=first_chip_select,
        responder=first_mock.responder,
    )
    controller.configure_device(
        device=second_device,
        chip_select=second_chip_select,
        responder=second_mock.responder,
    )

    assert not controller.lock_device(ctypes.c_int(9999))
    assert controller.lock_device(ctypes.c_int(first_device))
    assert not controller.lock_device(ctypes.c_int(second_device))
    assert controller.release_device(ctypes.c_int(first_device))

    controller.set_chip_select(first_chip_select, active=True)
    controller.set_chip_select(second_chip_select, active=True)
    ok, payload = controller.transmit_receive(device=first_device)
    assert not ok
    assert payload is None


def test_component_scheduler_runs_once_when_due_with_larger_cluster_step():
    controller = FakeNode()
    component = ScheduledComponent()

    cluster = ClusterRig(controller=controller, component=component)
    cluster.run_for(1, step=1)

    assert component.scheduled_times_ns == [1_000_000]
    assert cluster.elapsed_ns == 1_000_000


def test_component_scheduler_runs_once_when_due_standalone():
    component = ScheduledComponent()

    component.run_for(1)

    assert component.scheduled_times_ns == [1_000_000]
    assert component.elapsed_ns == 1_000_000


def test_periodic_datapath_producer_routes_model_inputs():
    path = DataPath(("periodic", "input"))
    sink = PythonConsumer(path)
    producer = PeriodicDataPathProducer(
        path,
        lambda model: {"timestamp_ns": model.elapsed_ns},
        scheduler_period=100,
        scheduler_unit="us",
    )

    cluster = ClusterRig(producer=producer, sink=sink)
    cluster.run_for(250, unit="us", step=250)

    assert sink.received_payloads == [{"timestamp_ns": 250_000}]


def test_simple_node_routes_component_ingress_to_explicit_egress_datapath():
    ingress_path = DataPath(("simple", "ingress"))
    egress_path = DataPath(("simple", "egress"))
    source_component = SimpleComponent()
    transformer_component = SimpleComponent()
    sink = PythonConsumer(egress_path)

    source_component.add_egress_datapath(ingress_path)
    transformer_component.add_egress_datapath(egress_path)

    def emit_observed_payload(payload):
        transformer_component.emit_egress(egress_path, {"observed": payload})

    transformer_component.add_ingress_datapath(
        ingress_path,
        handler=emit_observed_payload,
    )
    source_component.emit_egress(ingress_path, "payload")

    source = SimpleNodeRig(source_component)
    transformer = SimpleNodeRig(transformer_component, SimpleComponent())
    cluster = ClusterRig(source=source, transformer=transformer, sink=sink)
    cluster.run_for(1)

    assert transformer_component.ingress_datapaths == (ingress_path,)
    assert transformer_component.egress_datapaths == (egress_path,)
    assert transformer_component.ingress_events(ingress_path) == ("payload",)
    assert transformer_component.latest_ingress(ingress_path) == "payload"
    assert sink.received_payloads == [{"observed": "payload"}]


def test_model_rig_set_online_resets_and_gates_scheduler_ticks():
    model = TickModel()
    cluster = ClusterRig(model=model)

    assert model.is_online()
    cluster.run_for(3)
    assert model.ticks == 3

    model.set_online(False)
    assert not model.is_online()
    assert model.ticks == 0

    cluster.run_for(3)
    assert model.ticks == 0

    model.set_online(True)
    assert model.is_online()
    cluster.run_for(2)
    assert model.ticks == 2


def test_single_model_cluster_uses_rust_runtime_scheduler():
    model = BatchObservedModel()
    cluster = ClusterRig(model=model)

    cluster.run_for(5)

    assert model.ticks == 5
    assert model.elapsed_ns == 5_000_000
    assert model.run_durations_ns == []


def test_cluster_rejects_duplicate_rust_backed_shared_object_instances(tmp_path):
    shared_object = tmp_path / "libcontroller.so"
    shared_object.touch()

    first = SharedObjectBackedFakeNode(shared_object)
    second = SharedObjectBackedFakeNode(shared_object)

    try:
        ClusterRig(first=first, second=second)
    except ValueError as exc:
        assert "same Rust-backed controller shared object" in str(exc)
        assert "first" in str(exc)
        assert "second" in str(exc)
    else:
        raise AssertionError(
            "expected duplicate shared-object controller instances to fail"
        )


def test_python_model_owner_configures_outputs_for_component_inputs():
    path = DataPath(("python", "stream"))
    owner = PythonOwner(path)
    component = PythonConsumer(path)

    owner.configure_model_outputs_for(component)
    owner.pending_payloads.append("payload")
    cluster = ClusterRig(owner=owner, component=component)
    cluster.dataroutes.connect_available_paths()
    cluster.run_for(1)

    assert component.received_payloads == ["payload"]
