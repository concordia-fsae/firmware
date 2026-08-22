from __future__ import annotations

import ctypes
from collections.abc import Callable
from typing import Generic, TypeVar

from .contracts import Cluster
from rig.datapath import DataPath, DataPathKey, ModelDataPaths, datapath_key
from rig.dataflow import NativeRouteEndpoint
from .node_config import NodeConfig, SchedulerConfig
from rig.scalar import ScalarEvent, ScalarRouteEndpoint
from rig.scheduler import (
    PythonSchedulerCallbacks,
    SchedulerContext,
    _SchedulerCallbackContextAbi,
)
from rig.time import duration_to_ns


ModelClass = TypeVar("ModelClass", bound=type)
OwnerT = TypeVar("OwnerT", bound="ModelRig")


class ModelRig:
    """Schedulable model with datapaths that can participate in a cluster."""

    _ClusterScheduledCallback = ctypes.CFUNCTYPE(
        None,
        ctypes.POINTER(_SchedulerCallbackContextAbi),
    )
    _ClusterResetCallback = ctypes.CFUNCTYPE(None)
    _ScalarCountCallback = ctypes.CFUNCTYPE(ctypes.c_uint32)
    _ScalarRecvManyCallback = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.POINTER(ScalarEvent),
        ctypes.c_uint32,
    )
    _ScalarSendManyCallback = ctypes.CFUNCTYPE(
        ctypes.c_uint32,
        ctypes.POINTER(ScalarEvent),
        ctypes.c_uint32,
    )

    def __init__(
        self,
        *,
        configuration: NodeConfig | None = None,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
        scheduler_callback: Callable[[SchedulerContext], None] | None = None,
    ) -> None:
        if configuration is not None and (
            scheduler_period is not None or scheduler_callback is not None
        ):
            raise ValueError(
                "pass scheduler settings through configuration or legacy scheduler arguments, not both"
            )
        if configuration is None:
            configuration = NodeConfig(
                scheduler=SchedulerConfig(
                    period_ns=(
                        None
                        if scheduler_period is None
                        else duration_to_ns(scheduler_period, unit=scheduler_unit)
                    ),
                    callback=scheduler_callback,
                )
            )
        self.configuration = configuration
        self.datapaths = ModelDataPaths()
        self._cluster_rig: Cluster | None = None
        self._cluster_node_name: str | None = None
        self.elapsed_ns = 0
        self._scalar_route_abis: dict[DataPathKey, tuple[int, int, int, int]] = {}
        self._scalar_callbacks = []
        self._scheduler_period_ns = configuration.scheduler.period_ns
        self._scheduler_callback = configuration.scheduler.callback
        self._cluster_scheduled_callback = self._ClusterScheduledCallback(
            self._cluster_scheduled
        )
        self._cluster_reset_callback = self._ClusterResetCallback(self.reset)

    def attach_to(self, rig: Cluster, name: str) -> None:
        self._cluster_rig = rig
        self._cluster_node_name = name

    def detach_from(self) -> None:
        self._cluster_rig = None
        self._cluster_node_name = None

    def reset(self) -> None:
        self.elapsed_ns = 0

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        duration_ns = duration_to_ns(duration, unit=unit)
        if duration_ns < 0:
            raise ValueError(f"duration must not be negative, got {duration}")
        previous_ns = self.elapsed_ns
        self.elapsed_ns += duration_ns
        if self._scheduler_callback is not None:
            self._scheduler_callback(
                SchedulerContext(
                    elapsed_ns=self.elapsed_ns,
                    delta_ns=self.elapsed_ns - previous_ns,
                )
            )

    def configure_datapath(self, path: DataPath) -> None:
        raise ValueError(f"datapath {path!r} is not supported by {type(self).__name__}")

    def configure_model_outputs_for(self, model: object) -> None:
        datapaths = getattr(model, "datapaths", None)
        if datapaths is None:
            return
        for input_ in datapaths.inputs():
            if self.supports_datapath(input_.path):
                self.configure_datapath(input_.path)

    def supports_datapath(self, path: DataPath) -> bool:
        return bool(self.datapaths.outputs(path))

    def add_scalar_output(
        self,
        path: DataPath,
        *,
        pending: Callable[[], int],
        recv: Callable[[], float | int | None],
    ) -> None:
        key = datapath_key(path)

        def recv_many(events, capacity: int) -> int:
            count = 0
            for _ in range(int(capacity)):
                value = recv()
                if value is None:
                    break
                event = ScalarEvent()
                event.value = float(value)
                event.timestamp_ns = int(self.elapsed_ns)
                events[count] = event
                count += 1
            return count

        count_callback = self._ScalarCountCallback(lambda: int(pending()))
        recv_callback = self._ScalarRecvManyCallback(recv_many)
        send_callback = self._ScalarSendManyCallback(lambda _events, _count: 0)
        self._scalar_callbacks.extend((count_callback, recv_callback, send_callback))
        self._scalar_route_abis[key] = (
            datapath_route_id(key),
            self._callback_address(count_callback),
            self._callback_address(recv_callback),
            self._callback_address(send_callback),
        )
        self.datapaths.add_output(
            path,
            pending=pending,
            recv=recv,
        )

    def add_scalar_input(
        self,
        path: DataPath,
        *,
        send: Callable[[float], bool],
    ) -> None:
        key = datapath_key(path)

        def send_many(events, count: int) -> int:
            accepted = 0
            for index in range(int(count)):
                if not send(float(events[index].value)):
                    break
                accepted += 1
            return accepted

        count_callback = self._ScalarCountCallback(lambda: 0)
        recv_callback = self._ScalarRecvManyCallback(lambda _events, _capacity: 0)
        send_callback = self._ScalarSendManyCallback(send_many)
        self._scalar_callbacks.extend((count_callback, recv_callback, send_callback))
        self._scalar_route_abis[key] = (
            datapath_route_id(key),
            self._callback_address(count_callback),
            self._callback_address(recv_callback),
            self._callback_address(send_callback),
        )
        self.datapaths.add_input(
            path,
            send=send,
        )

    def rust_datapath_route_abi(self, path: DataPath) -> NativeRouteEndpoint | None:
        scalar_abi = self._scalar_route_abis.get(datapath_key(path))
        if scalar_abi is not None:
            return ScalarRouteEndpoint(*scalar_abi)
        return None

    def set_online(self, online: bool) -> None:
        if self._cluster_rig is None or self._cluster_node_name is None:
            raise RuntimeError(
                f"{type(self).__name__} is not attached to a cluster rig"
            )
        self._cluster_rig.set_node_online(self._cluster_node_name, online)

    def is_online(self) -> bool:
        if self._cluster_rig is None or self._cluster_node_name is None:
            return True
        return self._cluster_rig.node_online(self._cluster_node_name)

    def scheduler_callbacks(self) -> PythonSchedulerCallbacks:
        return self._python_scheduler_callbacks()

    def _python_scheduler_callbacks(
        self,
        *,
        period_ns: int | None = None,
    ) -> PythonSchedulerCallbacks:
        resolved_period_ns = (
            self._scheduler_period_ns if period_ns is None else int(period_ns)
        )
        return PythonSchedulerCallbacks(
            scheduled=self._callback_address(self._cluster_scheduled_callback)
            if self._scheduler_callback is not None
            else 0,
            reset=self._callback_address(self._cluster_reset_callback),
            period_ns=0 if resolved_period_ns is None else resolved_period_ns,
        )

    def _cluster_scheduled(
        self,
        context_abi,
    ) -> None:
        if not context_abi:
            return
        context = SchedulerContext.from_abi(context_abi.contents)
        self.elapsed_ns = context.elapsed_ns
        if self._scheduler_callback is not None:
            self._scheduler_callback(context)

    @staticmethod
    def _callback_address(callback) -> int:
        value = ctypes.cast(callback, ctypes.c_void_p).value
        if value is None:
            raise RuntimeError(f"could not resolve callback pointer for {callback!r}")
        return int(value)


class ComponentRig(ModelRig, Generic[OwnerT]):
    """Pure Python model that can run standalone or inside a cluster."""

    def configure_owner(self, owner: OwnerT) -> None:
        if not isinstance(owner, ModelRig):
            raise TypeError(
                f"component owner must implement ModelRig, got {type(owner).__name__}"
            )
        self._owner = owner

    @property
    def owner(self) -> OwnerT:
        try:
            return self._owner
        except AttributeError as exc:
            raise RuntimeError(
                f"{type(self).__name__} is not attached to a Rig node"
            ) from exc

    def _bind_native_model_symbol(
        self,
        name: str,
        argtypes: list[object],
        restype: object = ctypes.c_bool,
    ):
        binder = None
        if self._cluster_rig is not None and self._cluster_rig.runtime is not None:
            binder = self._cluster_rig.runtime.bind_symbol
        if binder is None:
            owner = getattr(self, "_owner", None)
            binder = getattr(owner, "bind_symbol", None)
        if binder is None:
            raise RuntimeError("native model symbols require a Rust-backed owner")
        return binder(name, argtypes, restype)


class PeriodicDataPathProducer(ComponentRig):
    """Scheduled component that emits model-input payloads on a datapath."""

    def __init__(
        self,
        path: DataPath,
        payload: object
        | Callable[[PeriodicDataPathProducer], object | tuple[object, ...] | None],
        *,
        scheduler_period: int | float = 1,
        scheduler_unit: str = "ms",
    ) -> None:
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_unit=scheduler_unit,
            scheduler_callback=self._produce_scheduled,
        )
        self.path = path
        self._payload = payload
        self._pending_payloads: list[object] = []
        self.datapaths.add_output(
            path,
            pending=lambda: len(self._pending_payloads),
            recv=self._recv,
            recv_many=self._recv_many,
        )

    def reset(self) -> None:
        super().reset()
        self._pending_payloads.clear()

    def _produce_scheduled(self, context: SchedulerContext) -> None:
        produced = self._payload(self) if callable(self._payload) else self._payload
        if produced is None:
            return
        if isinstance(produced, tuple):
            self._pending_payloads.extend(produced)
            return
        self._pending_payloads.append(produced)

    def _recv(self) -> object | None:
        return self._pending_payloads.pop(0) if self._pending_payloads else None

    def _recv_many(self, count: int) -> tuple[object, ...]:
        payloads = tuple(self._pending_payloads[:count])
        del self._pending_payloads[:count]
        return payloads


def extend_model_class(
    model_class: ModelClass,
    *mixins: type,
    name: str | None = None,
) -> ModelClass:
    if not mixins:
        return model_class

    extended = type(
        name or model_class.__name__,
        (*mixins, model_class),
        {
            "__module__": model_class.__module__,
            "__doc__": model_class.__doc__,
        },
    )
    return extended  # type: ignore[return-value]


_DATAPATH_ROUTE_IDS: dict[DataPathKey, int] = {}


def datapath_route_id(key: DataPathKey) -> int:
    route_id = _DATAPATH_ROUTE_IDS.get(key)
    if route_id is not None:
        return route_id
    route_id = len(_DATAPATH_ROUTE_IDS) + 1
    if route_id > 0xFFFF_FFFF:
        raise RuntimeError("exhausted native datapath route ids")
    _DATAPATH_ROUTE_IDS[key] = route_id
    return route_id
