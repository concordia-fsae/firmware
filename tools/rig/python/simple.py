from __future__ import annotations

from collections.abc import Callable

from rig.datapath import DataPath, DataPathKey, datapath_key
from .model import ComponentRig, ModelRig


SimpleDataPathHandler = Callable[[object], None]


class SimpleComponent(ComponentRig):
    """Python-only component with generic ingress and egress datapaths."""

    def __init__(
        self,
        *,
        scheduler_period: int | float | None = None,
        scheduler_unit: str = "ms",
    ) -> None:
        super().__init__(
            scheduler_period=scheduler_period,
            scheduler_unit=scheduler_unit,
        )
        self._ingress_events: dict[DataPathKey, list[object]] = {}
        self._ingress_paths: dict[DataPathKey, DataPath] = {}
        self._egress_events: dict[DataPathKey, list[object]] = {}
        self._egress_paths: dict[DataPathKey, DataPath] = {}

    @property
    def ingress_datapaths(self) -> tuple[DataPath, ...]:
        return tuple(self._ingress_paths.values())

    @property
    def egress_datapaths(self) -> tuple[DataPath, ...]:
        return tuple(self._egress_paths.values())

    def reset(self) -> None:
        super().reset()
        for events in self._ingress_events.values():
            events.clear()
        for events in self._egress_events.values():
            events.clear()

    def add_ingress_datapath(
        self,
        path: DataPath,
        *,
        handler: SimpleDataPathHandler | None = None,
    ) -> DataPath:
        key = datapath_key(path)
        self._ingress_paths.setdefault(key, path)
        self._ingress_events.setdefault(key, [])
        self.datapaths.add_input(
            path,
            send=lambda payload, path=path, handler=handler: self._send_ingress(
                path,
                payload,
                handler=handler,
            ),
            send_many=lambda payloads,
            path=path,
            handler=handler: self._send_ingress_many(
                path,
                payloads,
                handler=handler,
            ),
        )
        return path

    def add_egress_datapath(self, path: DataPath) -> DataPath:
        key = datapath_key(path)
        self._egress_paths.setdefault(key, path)
        self._egress_events.setdefault(key, [])
        self.datapaths.add_output(
            path,
            pending=lambda path=path: self.egress_count(path),
            recv=lambda path=path: self.recv_egress(path),
            recv_many=lambda count, path=path: self.recv_egress_many(path, count),
        )
        return path

    def emit_egress(self, path: DataPath, payload: object) -> bool:
        key = datapath_key(path)
        if key not in self._egress_events:
            raise ValueError(f"egress datapath {path!r} is not configured")
        self._egress_events[key].append(payload)
        return True

    def ingress_events(self, path: DataPath) -> tuple[object, ...]:
        return tuple(self._events_for(self._ingress_events, path, "ingress"))

    def latest_ingress(self, path: DataPath) -> object | None:
        events = self._events_for(self._ingress_events, path, "ingress")
        return events[-1] if events else None

    def egress_count(self, path: DataPath) -> int:
        return len(self._events_for(self._egress_events, path, "egress"))

    def recv_egress(self, path: DataPath) -> object | None:
        events = self._events_for(self._egress_events, path, "egress")
        return events.pop(0) if events else None

    def recv_egress_many(self, path: DataPath, count: int) -> tuple[object, ...]:
        events = self._events_for(self._egress_events, path, "egress")
        payloads = tuple(events[:count])
        del events[:count]
        return payloads

    def _send_ingress(
        self,
        path: DataPath,
        payload: object,
        *,
        handler: SimpleDataPathHandler | None,
    ) -> bool:
        key = datapath_key(path)
        if key not in self._ingress_events:
            raise ValueError(f"ingress datapath {path!r} is not configured")
        self._ingress_events[key].append(payload)
        if handler is not None:
            handler(payload)
        return True

    def _send_ingress_many(
        self,
        path: DataPath,
        payloads: tuple[object, ...],
        *,
        handler: SimpleDataPathHandler | None,
    ) -> int:
        for payload in payloads:
            self._send_ingress(path, payload, handler=handler)
        return len(payloads)

    @staticmethod
    def _events_for(
        events_by_path: dict[DataPathKey, list[object]],
        path: DataPath,
        direction: str,
    ) -> list[object]:
        key = datapath_key(path)
        try:
            return events_by_path[key]
        except KeyError as exc:
            raise ValueError(
                f"{direction} datapath {path!r} is not configured"
            ) from exc


class SimpleNodeRig(ModelRig):
    """Python-only node composed from arbitrary generic components."""

    def __init__(self, *components: ComponentRig) -> None:
        super().__init__()
        self.components: list[ComponentRig] = []
        for component in components:
            self.add_component(component)

    def add_component(self, component: ComponentRig) -> ComponentRig:
        if self._cluster_rig is not None:
            raise RuntimeError("simple components must be added before clustering")
        if getattr(component, "_scheduler_period_ns", None) is not None:
            raise ValueError(
                "scheduled components must be added to a cluster directly so "
                "Rust can schedule each component independently"
            )
        self.components.append(component)
        self._bind_component_interfaces(component)
        self._bind_component_datapaths(component)
        return component

    def reset(self) -> None:
        super().reset()
        for component in self.components:
            component.reset()

    def _bind_component_datapaths(self, component: ComponentRig) -> None:
        for input_ in component.datapaths.inputs():
            self.datapaths.add_input(
                input_.path,
                send=input_.send,
                send_many=input_.send_many,
            )
        for output in component.datapaths.outputs():
            self.datapaths.add_output(
                output.path,
                pending=output.pending,
                recv=output.recv,
                recv_many=output.recv_many,
            )

    def _bind_component_interfaces(self, component: ComponentRig) -> None:
        for interface in getattr(component, "interfaces", ()):
            configure = getattr(interface, "configure", None)
            if configure is not None:
                configure(self)
