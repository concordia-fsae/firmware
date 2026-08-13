from sim.infra.rig import (
    CanBusDescriptor,
    ClusterCanComms,
    ClusterRig,
    ComponentRig,
    DataPath,
    ModelRig,
    PeriodicDataPathProducer,
    SimpleComponent,
    SimpleNodeRig,
)


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
        super().__init__(scheduler_period=250, scheduler_unit="us")
        self.scheduled_times_ns = []

    def reset(self) -> None:
        super().reset()
        self.scheduled_times_ns.clear()

    def _run_scheduled(self) -> None:
        self.scheduled_times_ns.append(self.elapsed_ns)


class TickModel(ModelRig):
    def __init__(self) -> None:
        super().__init__(scheduler_period=1)
        self.ticks = 0

    def reset(self) -> None:
        super().reset()
        self.ticks = 0

    def _run_scheduled(self) -> None:
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


def test_component_scheduler_coalesces_missed_periods_during_cluster_fast_forward():
    controller = FakeNode()
    component = ScheduledComponent()

    cluster = ClusterRig(controller=controller, component=component)
    cluster.run_for(1)

    assert component.scheduled_times_ns == [1_000_000]
    assert cluster.elapsed_ns == 1_000_000


def test_component_scheduler_runs_all_due_periods_when_used_standalone():
    component = ScheduledComponent()

    component.run_for(1)

    assert component.scheduled_times_ns == [250_000, 500_000, 750_000, 1_000_000]
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

    assert sink.received_payloads == [
        {"timestamp_ns": 250_000},
    ]


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
