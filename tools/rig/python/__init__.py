"""Firmware-independent Python facade for the Rig core."""

__all__ = [
    "Algorithm",
    "Component",
    "ComponentDataPathBinding",
    "ComponentDataPathOutput",
    "ComponentRig",
    "ClusterConfig",
    "ClusterRig",
    "DataPath",
    "DataPathLink",
    "DataPathRecord",
    "Dataflow",
    "DataflowRoutes",
    "Rig",
    "DataflowEvent",
    "DataflowConfig",
    "DataflowWait",
    "Edge",
    "Interface",
    "Cluster",
    "ModelDataPathInputConnector",
    "ModelDataPathOutputConnector",
    "ModelDataPathDescriptor",
    "RustClusterRuntime",
    "RustRuntimeHost",
    "ModelDataPaths",
    "Model",
    "ModelRig",
    "Node",
    "RigElement",
    "RigInterface",
    "RigRuntime",
    "NodeConfig",
    "PeriodicDataPathProducer",
    "PythonSchedulerCallbacks",
    "RunUntilTimeout",
    "RustSchedulerCallbacks",
    "ScalarEvent",
    "Scheduler",
    "SchedulerConfig",
    "SchedulerContext",
    "buck_output",
    "duration_to_ns",
    "load_generated_enums",
    "load_generated_module",
    "load_shared_library",
    "repo_root",
    "run_until",
    "shared_library_mode",
    "extend_model_class",
    "SimpleComponent",
    "SimpleNodeRig",
]


def __getattr__(name):
    if name not in __all__:
        raise AttributeError(name)
    from . import _api

    value = getattr(_api, name)
    globals()[name] = value
    return value
