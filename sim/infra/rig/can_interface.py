from __future__ import annotations

from enum import IntEnum

from .can import (
    CanBusDescriptor,
    CanEvent,
    CanMessageDescriptor,
    CanPacket,
    CanSignalDescriptor,
    DecodedCanMessage,
)
from .datapath import DataPath
from .model import PeriodicDataPathProducer


class PeriodicCanSender(PeriodicDataPathProducer):
    def __init__(
        self,
        can: CanInterface,
        message: CanMessageDescriptor,
        *,
        scheduler_period: int | float = 100,
        scheduler_unit: str = "ms",
        **signals: float | int | IntEnum,
    ) -> None:
        self.can = can
        self.message = message
        self.signals = dict(signals)
        super().__init__(
            DataPath.can_bus(message.bus_name),
            self._produce,
            scheduler_period=scheduler_period,
            scheduler_unit=scheduler_unit,
        )

    def set(self, **signals: float | int | IntEnum) -> PeriodicCanSender:
        self.signals.update(signals)
        return self

    def _produce(self, producer: PeriodicDataPathProducer) -> CanEvent:
        packet = self.can.encode(self.message, **self.signals)
        return CanEvent.from_packet(
            self.message.bus,
            packet,
            timestamp_ns=producer.elapsed_ns,
        )


class CanInterface:
    def __init__(self, model: NodeRig) -> None:
        self._model = model

    @property
    def buses(self) -> tuple[CanBusDescriptor, ...]:
        return self._model._can_buses

    @property
    def bus_count(self) -> int:
        return self._model._can_bus_count_value()

    @property
    def rx_messages(self) -> tuple[CanMessageDescriptor, ...]:
        return self._model._can_messages

    @property
    def tx_messages(self) -> tuple[CanMessageDescriptor, ...]:
        return self._model._can_tx_messages

    @property
    def rx_signals(self) -> tuple[CanSignalDescriptor, ...]:
        return self._model._can_signals

    @property
    def tx_signals(self) -> tuple[CanSignalDescriptor, ...]:
        return self._model._can_tx_signals

    @property
    def enums(self) -> CanEnumNamespace:
        return self._model._can_enums

    def bus(self, bus: int | str | CanBusDescriptor) -> CanBusDescriptor:
        return self._model._can_bus_descriptor(bus)

    def message(
        self,
        name: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        tx: bool = False,
    ) -> CanMessageDescriptor:
        return self._model._can_message_descriptor(name, bus=bus, tx=tx)

    def tx_message(
        self,
        name: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanMessageDescriptor:
        return self._model._can_tx_message_descriptor(name, bus=bus)

    def send(
        self,
        bus_or_message: int | str | CanBusDescriptor | CanMessageDescriptor,
        frame_id: int | None = None,
        payload: bytes | bytearray | list[int] | tuple[int, ...] | None = None,
        **signals: float | int | IntEnum,
    ) -> bool:
        if isinstance(bus_or_message, CanMessageDescriptor):
            if frame_id is not None or payload is not None:
                raise ValueError(
                    "frame_id and payload must not be provided when sending a CAN message descriptor"
                )
            packet = self.encode(bus_or_message, **signals)
            return self._model._can_send_packet(
                bus_or_message.bus,
                bus_or_message.id,
                packet.payload,
            )

        if frame_id is None or payload is None:
            raise ValueError("raw CAN send requires bus, frame_id, and payload")
        if signals:
            raise ValueError("signal values require sending a CAN message descriptor")
        return self._model._can_send_packet(bus_or_message, frame_id, payload)

    def recv(self, bus: int | str | CanBusDescriptor) -> CanEvent | None:
        return self._model._can_recv_event(bus)

    def recv_latest(self, message: CanMessageDescriptor) -> CanEvent | None:
        return self._model._can_recv_message(message)

    def latest_event(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanEvent | None:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is not None and node_name is not None:
            return cluster.comm.can.latest_message(
                node_name,
                descriptor,
                bus=descriptor.bus,
            )
        return self.recv_latest(descriptor)

    def latest_bus_event(self, bus: int | str | CanBusDescriptor) -> CanEvent | None:
        bus_descriptor = self.bus(bus)
        cluster = self._model._cluster_rig
        node_name = self._model._cluster_node_name
        if cluster is not None and node_name is not None:
            return cluster.comm.can.latest_bus_event(node_name, bus_descriptor)

        latest = None
        while self.tx_count(bus_descriptor):
            latest = self.recv(bus_descriptor)
        return latest

    def latest(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        signals: tuple[str, ...] | list[str] | None = None,
    ) -> DecodedCanMessage | None:
        descriptor = self._tx_message_descriptor(message, bus=bus)
        event = self.latest_event(descriptor)
        if event is None:
            return None
        return self.decode(descriptor, event, signals)

    def latest_signal(
        self,
        message: str | CanMessageDescriptor,
        signal: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> object | None:
        decoded = self.latest(message, bus=bus, signals=(signal,))
        if decoded is None:
            return None
        return getattr(decoded, signal)

    def rx_count(self, bus: int | str | CanBusDescriptor) -> int:
        return self._model._can_rx_count_value(bus)

    def tx_count(self, bus: int | str | CanBusDescriptor) -> int:
        return self._model._can_tx_count_value(bus)

    def decode(
        self,
        message: CanMessageDescriptor,
        packet_or_event: CanPacket | CanEvent,
        signal_names: tuple[str, ...] | list[str] | None = None,
    ) -> DecodedCanMessage:
        event = packet_or_event if isinstance(packet_or_event, CanEvent) else None
        packet = packet_or_event.packet if event is not None else packet_or_event
        signals = (
            self._model._signals_for_message(message)
            if signal_names is None
            else signal_names
        )
        raw_values = self._model._can_decode_message_raw(
            message.bus, packet, tuple(signals)
        )
        typed_values = {
            signal_name: self._model._coerce_decoded_can_value(signal_name, value)
            for signal_name, value in raw_values.items()
        }
        return DecodedCanMessage(message=message, values=typed_values, event=event)

    def encode(
        self, message: CanMessageDescriptor, **signals: float | int | IntEnum
    ) -> CanPacket:
        raw_signals = {
            signal_name: int(value) if isinstance(value, IntEnum) else value
            for signal_name, value in signals.items()
        }
        return self._model._can_encode_message_raw(
            message.bus, message.name, **raw_signals
        )

    def periodic_send(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        period: int | float = 100,
        unit: str = "ms",
        **signals: float | int | IntEnum,
    ) -> PeriodicCanSender:
        descriptor = (
            self.message(message, bus=bus) if isinstance(message, str) else message
        )
        if bus is not None and self._model._coerce_can_bus(bus) != descriptor.bus:
            raise ValueError(f"message {descriptor.name!r} is not on bus {bus!r}")
        return PeriodicCanSender(
            self,
            descriptor,
            scheduler_period=period,
            scheduler_unit=unit,
            **signals,
        )

    def _tx_message_descriptor(
        self,
        message: str | CanMessageDescriptor,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanMessageDescriptor:
        if isinstance(message, CanMessageDescriptor):
            if bus is not None and self._model._coerce_can_bus(bus) != message.bus:
                raise ValueError(f"message {message.name!r} is not on bus {bus!r}")
            return message
        return self.tx_message(message, bus=bus)
