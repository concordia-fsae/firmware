from __future__ import annotations

import ctypes

import pytest

from rig import (
    ClusterConfig,
    ClusterRig,
    ComponentRig,
    DataPath,
    DataflowWait,
    DataflowConfig,
    ModelRig,
    ModelDataPathDescriptor,
    NodeConfig,
    Rig,
    RigElement,
    RigRuntime,
    RustClusterRuntime,
    RustRuntimeHost,
    SchedulerConfig,
    duration_to_ns,
    run_until,
)
from rig.simple import SimpleComponent, SimpleNodeRig
from rig.time import RunUntilTimeout


def test_core_configuration_preserves_generic_scheduler_and_dataflow():
    input_path = DataPath.named("node", "input")
    output_path = DataPath.named("node", "output")
    scheduler = SchedulerConfig(period_ns=1_000_000)
    node = NodeConfig(
        scheduler=scheduler,
        dataflow=DataflowConfig(inputs=(input_path,), outputs=(output_path,)),
    )
    cluster = ClusterConfig(scheduler=scheduler, dataflow=node.dataflow)

    assert node.scheduler is scheduler
    assert node.dataflow.inputs == (input_path,)
    assert node.dataflow.outputs == (output_path,)
    assert cluster.scheduler is scheduler
    assert cluster.dataflow is node.dataflow


def test_core_scheduler_rejects_zero_periods():
    with pytest.raises(ValueError, match="period must be positive"):
        SchedulerConfig(period_ns=0)


def test_model_datapath_descriptor_has_a_stable_c_layout():
    descriptor = ModelDataPathDescriptor(
        interface=7,
        port=1,
        channel=2,
        device=3,
    )

    assert ctypes.sizeof(ModelDataPathDescriptor) == 16
    assert ctypes.alignment(ModelDataPathDescriptor) == 4
    assert (
        descriptor.interface,
        descriptor.port,
        descriptor.channel,
        descriptor.device,
    ) == (
        7,
        1,
        2,
        3,
    )


def test_rust_runtime_is_a_first_class_rig_api():
    assert RustClusterRuntime.__module__ == "rig.runtime"
    assert RustRuntimeHost.__module__ == "rig.runtime"
    assert RigRuntime.__module__ == "rig.contracts"
    assert hasattr(RustClusterRuntime, "add_scalar_route")
    assert hasattr(RustClusterRuntime, "run_until_dataflow_wait")


def test_dataflow_wait_is_single_use_and_cancellable():
    calls = []

    class FakeRuntime:
        def run_until_dataflow_wait(self, wait_id, **kwargs):
            calls.append(("wait", wait_id, kwargs))
            return 123

        def cancel_dataflow_wait(self, wait_id):
            calls.append(("cancel", wait_id))

    completed = DataflowWait(FakeRuntime(), 7)
    assert completed.wait(timeout_ns=1000, step_ns=10, route=False) == 123
    assert calls == [("wait", 7, {"timeout_ns": 1000, "step_ns": 10, "route": False})]
    with pytest.raises(RuntimeError, match="no longer active"):
        completed.wait(timeout_ns=1000, step_ns=10)

    cancelled = DataflowWait(FakeRuntime(), 8)
    cancelled.cancel()
    cancelled.cancel()
    assert calls[-1] == ("cancel", 8)


def test_model_rig_accepts_generic_configuration_and_scheduler_callback():
    scheduled = []
    configuration = NodeConfig(
        scheduler=SchedulerConfig(
            period_ns=2_000_000,
            callback=lambda context: scheduled.append(context.elapsed_ns),
        )
    )
    model = ModelRig(configuration=configuration)

    assert model.configuration is configuration
    assert model._python_scheduler_callbacks().period_ns == 2_000_000
    model.run_for(2, unit="ms")
    assert scheduled == [2_000_000]


def test_core_datapaths_are_named_and_component_identity_stable():
    owner = object()
    named = DataPath.named("component", "input")
    component_path = DataPath.component(owner, "input")

    assert named != component_path
    assert DataPath.component(owner, "output") is DataPath.component(owner, "output")


def test_core_time_helpers_cover_success_and_timeout():
    assert duration_to_ns(1, unit="ns") == 1
    assert duration_to_ns(1, unit="us") == 1_000
    assert duration_to_ns(1, unit="ms") == 1_000_000
    assert duration_to_ns(1, unit="s") == 1_000_000_000
    with pytest.raises(ValueError):
        duration_to_ns(-1)
    with pytest.raises(ValueError):
        duration_to_ns(1, unit="minutes")

    elapsed = []
    assert (
        run_until(
            lambda delta: elapsed.append(delta),
            lambda: sum(elapsed) >= 5,
            timeout_ns=10,
            step_ns=3,
        )
        == 6
    )
    with pytest.raises(RunUntilTimeout, match="deadline"):
        run_until(lambda _delta: None, lambda: False, timeout_ns=2, message="deadline")


class _ScheduledComponent(ComponentRig):
    def __init__(self):
        self.scheduled_times_ns = []
        super().__init__(scheduler_period=1, scheduler_unit="ms")

    def _on_scheduled(self, context):
        self.scheduled_times_ns.append(context.elapsed_ns)


def test_component_scheduler_runs_once_when_due_standalone():
    component = _ScheduledComponent()
    component._scheduler_callback = component._on_scheduled

    component.run_for(1)

    assert component.scheduled_times_ns == [1_000_000]
    assert component.elapsed_ns == 1_000_000


def test_cluster_rig_routes_generic_nodes_and_controls_online_state():
    path = DataPath.named("generic", "signal")
    source = SimpleComponent()
    source.add_egress_datapath(path)
    source.emit_egress(path, 42)
    sink = SimpleComponent()
    sink.add_ingress_datapath(path)

    cluster = ClusterRig(source=source, sink=sink)
    cluster.run_for(1)

    assert sink.latest_ingress(path) == 42
    assert cluster.dataroutes.latest_record(path).payload == 42
    cluster.disable_node("sink")
    assert not cluster.node_online("sink")
    cluster.enable_node("sink")
    assert cluster.node_online("sink")
    assert isinstance(cluster, Rig)
    assert isinstance(source, RigElement)
    assert isinstance(sink, RigElement)


def test_cluster_rig_routes_batched_datapaths_and_records_each_event():
    path = DataPath.named("generic", "batched")
    source = SimpleComponent()
    sink = SimpleComponent()
    source.add_egress_datapath(path)
    sink.add_ingress_datapath(path)
    for payload in ("first", "second", "third"):
        source.emit_egress(path, payload)

    cluster = ClusterRig(source=source, sink=sink)
    cluster.run_for(1)

    assert sink.ingress_events(path) == ("first", "second", "third")
    assert [record.payload for record in cluster.dataroutes.records(path)] == [
        "first",
        "second",
        "third",
    ]


def test_simple_node_forwards_native_component_route_abi():
    path = DataPath.named("generic", "native")
    component = SimpleComponent()
    component.add_scalar_output(path, pending=lambda: 0, recv=lambda: None)
    node = SimpleNodeRig(component)

    assert node.rust_datapath_route_abi(path) == component.rust_datapath_route_abi(path)


def test_python_route_cache_reuses_topology_and_invalidates_on_new_link():
    path = DataPath.named("generic", "cached")
    source = SimpleComponent()
    sink = SimpleComponent()
    second_sink = SimpleComponent()
    source.add_egress_datapath(path)
    sink.add_ingress_datapath(path)
    second_sink.add_ingress_datapath(path)

    cluster = ClusterRig(
        components=(source, sink, second_sink),
        connect=False,
    )
    cluster.dataroutes.connect(
        path,
        source_node="__component_0",
        sink_node="__component_1",
    )

    first_routes = cluster.dataroutes._routes_for_path(path)
    assert cluster.dataroutes._routes_for_path(path) is first_routes

    # Re-registering an existing edge must preserve the cached topology.
    cluster.dataroutes.connect(
        path,
        source_node="__component_0",
        sink_node="__component_1",
    )
    assert cluster.dataroutes._routes_for_path(path) is first_routes

    # Adding a real edge changes fanout topology and must invalidate it.
    cluster.dataroutes.connect(
        path,
        source_node="__component_0",
        sink_node="__component_2",
    )
    second_routes = cluster.dataroutes._routes_for_path(path)
    assert second_routes is not first_routes
    assert {sink.node for route in second_routes for sink in route.sinks} == {
        "__component_1",
        "__component_2",
    }


def test_python_routes_reject_same_path_feedback_edges():
    path = DataPath.named("generic", "feedback")
    node = SimpleComponent()
    node.add_egress_datapath(path)
    node.add_ingress_datapath(path)
    cluster = ClusterRig(node=node, connect=False)

    with pytest.raises(ValueError, match="route graph contains a cycle"):
        cluster.dataroutes.connect(
            path,
            source_node="node",
            sink_node="node",
        )


def test_cluster_rig_rejects_untyped_elements_at_the_boundary():
    with pytest.raises(TypeError, match="RigElement contract"):
        ClusterRig(node=object())

    cluster = ClusterRig(node=SimpleComponent())
    with pytest.raises(TypeError, match="RigElement contract"):
        cluster.add_components(object())
