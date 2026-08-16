from __future__ import annotations

import pytest

from rig import (
    ClusterConfig,
    ComponentDataPathOutput,
    DataPath,
    DataPathLink,
    DataPathRecord,
    DataflowConfig,
    ModelDataPathInputConnector,
    ModelDataPaths,
    NodeConfig,
    PeriodicDataPathProducer,
    SchedulerConfig,
    duration_to_ns,
    extend_model_class,
)
from rig.artifacts import load_generated_enums, load_generated_module, repo_root
from rig.scalar import (
    ScalarInputRouteEndpoint,
    ScalarRouteEndpoint,
    ScalarSinkRouteEndpoint,
    ScalarStateSinkRouteEndpoint,
)
from rig.scheduler import (
    PythonSchedulerCallbacks,
    RustSchedulerCallbacks,
    SchedulerContext,
    _SchedulerCallbackContextAbi,
)


def test_generic_configuration_and_scheduler_abi_are_typed():
    path = DataPath.named("config", "value")
    interface = object()
    scheduler = SchedulerConfig(period_ns=100, callback=lambda _context: None)
    dataflow = DataflowConfig(inputs=(path,), outputs=(path,))
    node = NodeConfig(scheduler=scheduler, dataflow=dataflow, interfaces=(interface,))
    cluster = ClusterConfig(scheduler=scheduler, dataflow=dataflow, interfaces=(interface,))

    assert node.interfaces == (interface,)
    assert cluster.interfaces == (interface,)

    abi = _SchedulerCallbackContextAbi(23, 7)
    assert SchedulerContext.from_abi(abi) == SchedulerContext(23, 7)
    assert PythonSchedulerCallbacks(1, 2, 3).period_ns == 3
    assert RustSchedulerCallbacks(4, 5).reset == 5


class _ScalarRuntime:
    def __init__(self):
        self.calls = []

    def add_scalar_route(self, **kwargs):
        self.calls.append(("route", kwargs))
        return True

    def add_scalar_sink_route(self, **kwargs):
        self.calls.append(("sink", kwargs))
        return True

    def add_scalar_state_route(self, **kwargs):
        self.calls.append(("state", kwargs))
        return True

    def add_scalar_input_route(self, **kwargs):
        self.calls.append(("input", kwargs))
        return True


def test_scalar_route_endpoints_cover_each_generic_connection_kind():
    runtime = _ScalarRuntime()
    source = ScalarRouteEndpoint(route_id=4, count=1, recv_many=2, send_many=3)

    assert source.scalar_source_route_id == 4
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarRouteEndpoint(4, 5, 6, 7),
    )
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarSinkRouteEndpoint(4, 8, 1.5, 9),
    )
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarStateSinkRouteEndpoint(4, 0.0, sink_id=10),
    )
    assert source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=ScalarInputRouteEndpoint(11),
    )
    assert not source.connect(
        runtime,
        source_node="source",
        sink_node="sink",
        sink=object(),
    )
    assert [kind for kind, _kwargs in runtime.calls] == [
        "route",
        "sink",
        "state",
        "input",
    ]
    assert not ScalarSinkRouteEndpoint(1, 2, 1.0, 3).compatible_with(source)


def test_datapath_collections_and_component_binding_preserve_identity():
    path = DataPath.named("paths", "value")
    other = DataPath.named("paths", "other")
    data_paths = ModelDataPaths()
    sent = []
    data_paths.add_output(path, pending=lambda: 0, recv=lambda: None)
    data_paths.add_input(path, send=lambda value: sent.append(value) or True)
    data_paths.add_output(other, pending=lambda: 0, recv=lambda: None)

    assert data_paths.outputs(path)[0].path is path
    assert data_paths.inputs(path)[0].path is path
    assert data_paths.paths == (path, other)

    bound = []
    output = ComponentDataPathOutput(lambda component: DataPath.component(component, "out"))
    sink = ModelDataPathInputConnector(lambda owner, bound_path: bound.append((owner, bound_path)))
    output.bind_to(sink).bind("owner", "component")
    assert bound == [("owner", DataPath.component("component", "out"))]
    assert sent == []

    record = DataPathRecord("node", path, 5, 10)
    assert DataPathLink(path, "node", "sink").path is record.path


def test_periodic_producer_handles_tuple_none_and_reset():
    path = DataPath.named("producer", "output")
    producer = PeriodicDataPathProducer(path, ("a", "b"), scheduler_period=1)
    producer._produce_scheduled(None)
    assert producer._recv_many(3) == ("a", "b")

    producer._payload = None
    producer._produce_scheduled(None)
    producer._pending_payloads.append("stale")
    producer.reset()
    assert producer._recv() is None
    assert duration_to_ns(1, unit="ms") == 1_000_000


def test_model_extension_and_artifact_loading_are_backend_neutral(tmp_path, monkeypatch):
    class Base:
        pass

    class Mixin:
        marker = "mixin"

    assert extend_model_class(Base) is Base
    Extended = extend_model_class(Base, Mixin, name="Extended")
    assert Extended.__name__ == "Extended"
    assert Extended().marker == "mixin"
    assert repo_root().joinpath("pyproject.toml").is_file()

    generated = tmp_path / "generated.py"
    generated.write_text(
        "from enum import Enum\nclass State(Enum):\n    READY = 1\nvalue = 7\n"
    )
    monkeypatch.setenv("RIG_TEST_GENERATED", str(generated))
    module = load_generated_module("RIG_TEST_GENERATED", "//test:generated", "rig_test_generated")
    assert module.value == 7
    namespace = {}
    enums = load_generated_enums(
        "RIG_TEST_GENERATED", "//test:generated", "rig_test_generated", namespace
    )
    assert enums.State.READY.value == 1
    assert namespace["State"].READY.value == 1
    assert load_generated_enums(
        "RIG_TEST_GENERATED", "//test:generated", "rig_test_generated", namespace
    ) is enums

    monkeypatch.delenv("RIG_TEST_GENERATED")
    with pytest.raises(RuntimeError, match="RIG_TEST_GENERATED"):
        load_generated_module("RIG_TEST_GENERATED", "//test:generated", "missing")
