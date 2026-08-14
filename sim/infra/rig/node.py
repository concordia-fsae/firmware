from __future__ import annotations

import ctypes
import os
import pathlib
from enum import IntEnum

from .artifacts import buck_output, load_shared_library
from .can import (
    SIGNAL_KIND_NAMES,
    CanBusDescriptor,
    CanEnumNamespace,
    CanEnumValueDescriptor,
    CanSignalValue,
    CanEvent,
    CanMessageDescriptor,
    CanPacket,
    CanSignalDescriptor,
    _CanEnumValueDescriptorAbi,
    _CanMessageDescriptorAbi,
    _CanSignalDescriptorAbi,
    CanInterface,
    python_enum_attr_member,
    python_enum_class_name,
    python_enum_member,
)
from .cluster import ClusterCanComms
from .datapath import DataPath, datapath_key
from .model import ModelRig, datapath_route_id
from .peripherals import (
    SpiInterface,
    SpiPeripheralInterface,
    SpiTransaction,
    TimerCaptureEvent,
    TimerChannelEvent,
    TimerInterface,
    TimerPeripheralInterface,
)
from .scheduler import RustSchedulerCallbacks
from .time import duration_to_ns


RIG_MODEL_DATAPATH_TIMER_DUTY = 1
RIG_MODEL_DATAPATH_TIMER_FREQUENCY = 2
RIG_MODEL_DATAPATH_SPI_TRANSACTION = 3

_CAN_METADATA_CACHE: dict[str, dict[str, object]] = {}
_CAN_INDEX_CACHE: dict[str, dict[str, object]] = {}


class _ModelDataPathDescriptorAbi(ctypes.Structure):
    _fields_ = [
        ("interface", ctypes.c_uint16),
        ("port", ctypes.c_int32),
        ("channel", ctypes.c_int32),
        ("device", ctypes.c_int32),
    ]


class NodeRig(ModelRig):
    """ctypes-backed fixture for a Buck-built simulated node."""

    has_can = True
    buck_target: str
    env_var: str
    symbol_prefix: str
    timer = TimerInterface()
    spi = SpiInterface()

    def __init__(self, library_path: str | pathlib.Path | None = None) -> None:
        super().__init__()
        self._root = pathlib.Path(__file__).resolve().parents[3]
        self.library_path = self._resolve_library_path(library_path)
        self._lib = load_shared_library(self.library_path)
        self._can_metadata: dict[str, object] | None = None
        self._can_indexes: dict[str, object] | None = None
        self._scalar_sink_abis: dict[str, tuple[int, int, float, int]] = {}
        self._timer_scaled_scalar_outputs: dict[
            str, tuple[DataPath, int, int, int, float, float]
        ] = {}
        self.can = CanInterface(self) if self.has_can else None
        self._timer_peripherals = TimerPeripheralInterface(self)
        self._spi_peripherals = SpiPeripheralInterface(self)
        self._configure_model_abi()
        self._configure_abi()
        self.reset()
        self._configure_datapaths()

    def reset(self) -> None:
        super().reset()
        self._new()

    def run_for(self, duration: int | float, *, unit: str = "ms") -> None:
        duration_ns = duration_to_ns(duration, unit=unit)
        self.elapsed_ns += duration_ns
        self._run_for(ctypes.c_uint64(duration_ns))

    def fast_forward_for(self, duration: int | float, *, unit: str = "ms") -> None:
        duration_ns = duration_to_ns(duration, unit=unit)
        self.elapsed_ns += duration_ns
        self._fast_forward_for(ctypes.c_uint64(duration_ns))

    def scheduler_callbacks(self) -> RustSchedulerCallbacks:
        return RustSchedulerCallbacks(
            run_for=self._function_address(self._run_for),
            fast_forward_for=self._function_address(self._fast_forward_for),
            next_step=self._function_address(self._next_scheduler_step),
            reset=self._function_address(self._new),
        )

    def rust_can_route_abi(
        self, bus: int | str | CanBusDescriptor
    ) -> tuple[int, int, int, int] | None:
        if not self.has_can:
            return None
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        return (
            bus_index,
            self._function_address(self._can_tx_count),
            self._function_address(self._ffi_can_recv_events),
            self._function_address(self._can_send_many),
        )

    def rust_datapath_route_abi(
        self, path: DataPath
    ) -> tuple[str, tuple[int, ...]] | None:
        model_abi = super().rust_datapath_route_abi(path)
        if model_abi is not None:
            return model_abi
        scalar_sink_abi = self._scalar_sink_abis.get(datapath_key(path))
        if scalar_sink_abi is not None:
            return ("scalar_sink", scalar_sink_abi)
        native_scalar_output = self._timer_scaled_scalar_outputs.get(datapath_key(path))
        if native_scalar_output is not None:
            (
                timer_path,
                route_id,
                timer_interface,
                timer_port,
                timer_channel,
                scale,
                offset,
            ) = native_scalar_output
            timer_abi = self._timer_peripherals.rust_route_abi(timer_path)
            if self._cluster_rig is not None and self._cluster_node_name is not None:
                if not self._cluster_rig._rust_runtime.add_timer_source(
                    source_node=self._cluster_node_name,
                    interface=timer_interface,
                    port=timer_port,
                    channel=timer_channel,
                    source_count=timer_abi[3],
                    source_recv_many=timer_abi[4],
                ):
                    raise RuntimeError("failed to register native timer source")
                if not self._cluster_rig._rust_runtime.add_timer_scaled_scalar_source(
                    node=self._cluster_node_name,
                    route_id=route_id,
                    timer_interface=timer_interface,
                    timer_port=timer_port,
                    timer_channel=timer_channel,
                    scale=scale,
                    offset=offset,
                ):
                    raise RuntimeError(
                        "failed to register native timer-scaled scalar source"
                    )
            count_callback, recv_callback, send_callback = (
                self._cluster_rig._rust_runtime.noop_scalar_route_abi
                if self._cluster_rig is not None
                else (0, 0, 0)
            )
            return ("scalar", (route_id, count_callback, recv_callback, send_callback))
        try:
            if self._timer_peripherals.supports(path):
                return ("timer", self._timer_peripherals.rust_route_abi(path))
            if self._spi_peripherals.supports(path):
                return ("spi", self._spi_peripherals.rust_route_abi(path))
        except ValueError:
            return None
        return None

    def set_analog_input(self, channel: int, voltage: float) -> None:
        self._set_analog_input(ctypes.c_int(channel), ctypes.c_float(voltage))

    def add_scalar_sink(
        self,
        path: DataPath,
        *,
        sink_id: int,
        value_scale: float,
        set_value,
    ) -> None:
        key = datapath_key(path)
        self._scalar_sink_abis[key] = (
            datapath_route_id(key),
            int(sink_id),
            float(value_scale),
            self._function_address(set_value),
        )

        def send(value: float | int) -> bool:
            set_value(sink_id, float(value) * float(value_scale))
            return True

        self.datapaths.add_input(path, send=send)

    def add_timer_scaled_scalar_output(
        self,
        path: DataPath,
        *,
        timer_path: DataPath,
        scale: float,
        offset: float = 0.0,
    ) -> None:
        binding = timer_path.peripheral_binding
        if binding is None or binding.interface not in (
            "timer.duty",
            "timer.frequency",
        ):
            raise ValueError(f"datapath {timer_path!r} is not a timer channel")
        route_id = datapath_route_id(datapath_key(path))
        timer_interface = 1 if binding.interface == "timer.duty" else 2
        timer_port = int(binding.port if binding.port is not None else 0)
        timer_channel = int(binding.channel if binding.channel is not None else 0)
        self._timer_scaled_scalar_outputs[datapath_key(path)] = (
            timer_path,
            route_id,
            timer_interface,
            timer_port,
            timer_channel,
            float(scale),
            float(offset),
        )
        self.datapaths.add_output(
            path,
            pending=lambda: 0,
            recv=lambda: None,
        )

    def get_analog_input(self, channel: int) -> float:
        return float(self._get_analog_input(ctypes.c_int(channel)))

    def set_digital_io(self, channel: int, state: bool) -> None:
        self._set_digital_io(ctypes.c_int(channel), ctypes.c_bool(state))

    def get_digital_io(self, channel: int) -> bool:
        return bool(self._get_digital_io(ctypes.c_int(channel)))

    def get_fault(self, fault: int) -> bool:
        if self._get_fault is None:
            raise NotImplementedError(
                f"{self.__class__.__name__} does not expose local fault state"
            )
        return bool(self._get_fault(ctypes.c_int(fault)))

    def _can_bus_count_value(self) -> int:
        return int(self._can_bus_count())

    @property
    def _can_buses(self) -> tuple[CanBusDescriptor, ...]:
        return self._load_can_metadata()["buses"]  # type: ignore[return-value]

    @property
    def _can_messages(self) -> tuple[CanMessageDescriptor, ...]:
        return self._load_can_metadata()["messages"]  # type: ignore[return-value]

    @property
    def _can_tx_messages(self) -> tuple[CanMessageDescriptor, ...]:
        return self._load_can_metadata()["tx_messages"]  # type: ignore[return-value]

    @property
    def _can_signals(self) -> tuple[CanSignalDescriptor, ...]:
        return self._load_can_metadata()["signals"]  # type: ignore[return-value]

    @property
    def _can_tx_signals(self) -> tuple[CanSignalDescriptor, ...]:
        return self._load_can_metadata()["tx_signals"]  # type: ignore[return-value]

    @property
    def _can_enum_values(self) -> tuple[CanEnumValueDescriptor, ...]:
        return self._load_can_metadata()["enum_values"]  # type: ignore[return-value]

    @property
    def _can_enums(self) -> CanEnumNamespace:
        return self._load_can_metadata()["enums"]  # type: ignore[return-value]

    @property
    def _can_rx_messages_by_key(
        self,
    ) -> dict[tuple[str, int | None], CanMessageDescriptor]:
        return self._load_can_indexes()["rx_messages_by_key"]  # type: ignore[return-value]

    @property
    def _can_tx_messages_by_key(
        self,
    ) -> dict[tuple[str, int | None], CanMessageDescriptor]:
        return self._load_can_indexes()["tx_messages_by_key"]  # type: ignore[return-value]

    @property
    def _can_buses_by_name(self) -> dict[str, CanBusDescriptor]:
        return self._load_can_indexes()["buses_by_name"]  # type: ignore[return-value]

    @property
    def _signal_enum_names(self) -> dict[str, str]:
        return self._load_can_indexes()["signal_enum_names"]  # type: ignore[return-value]

    @property
    def _signal_kinds(self) -> dict[str, str]:
        return self._load_can_indexes()["signal_kinds"]  # type: ignore[return-value]

    @property
    def _signals_by_message(self) -> dict[tuple[int, int, str], tuple[str, ...]]:
        return self._load_can_indexes()["signals_by_message"]  # type: ignore[return-value]

    @property
    def _can_tx_signals_by_key(
        self,
    ) -> dict[tuple[int, int, str, str], CanSignalDescriptor]:
        return self._load_can_indexes()["tx_signals_by_key"]  # type: ignore[return-value]

    def _can_enum(self, enum_name: str) -> type[IntEnum]:
        try:
            return self._can_enums[enum_name]
        except KeyError as exc:
            raise KeyError(f"CAN enum {enum_name!r} was not found") from exc

    def _can_signal_enum(self, signal_name: str) -> type[IntEnum]:
        enum_name = self._signal_enum_names.get(signal_name)
        if enum_name is None:
            raise KeyError(
                f"CAN signal {signal_name!r} does not have generated enum metadata"
            )
        return self._can_enum(enum_name)

    def _can_bus_descriptor(
        self, bus: int | str | CanBusDescriptor
    ) -> CanBusDescriptor:
        index = self._coerce_can_bus(bus)
        return self._can_buses[index]

    def _can_message_descriptor(
        self,
        name: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
        tx: bool = False,
    ) -> CanMessageDescriptor:
        bus_index = None if bus is None else self._coerce_can_bus(bus)
        messages_by_key = (
            self._can_tx_messages_by_key if tx else self._can_rx_messages_by_key
        )
        if bus_index is not None:
            message = messages_by_key.get((name, bus_index))
            if message is not None:
                return message
        else:
            message = messages_by_key.get((name, None))
            if message is not None:
                return message
        direction = "TX" if tx else "RX"
        bus_text = "" if bus is None else f" on bus {bus!r}"
        raise KeyError(f"{direction} CAN message {name!r}{bus_text} was not found")

    def _can_tx_message_descriptor(
        self,
        name: str,
        *,
        bus: int | str | CanBusDescriptor | None = None,
    ) -> CanMessageDescriptor:
        return self._can_message_descriptor(name, bus=bus, tx=True)

    def _can_send_packet(
        self,
        bus: int | str | CanBusDescriptor,
        frame_id: int,
        payload: bytes | bytearray | list[int] | tuple[int, ...],
    ) -> bool:
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        packet = CanPacket.from_payload(frame_id, payload)
        return bool(self._can_send(ctypes.c_uint8(bus_index), ctypes.byref(packet)))

    def _can_recv_packet(self, bus: int | str | CanBusDescriptor) -> CanPacket | None:
        event = self._can_recv_event(bus)
        return None if event is None else event.packet

    def _can_recv_event(self, bus: int | str | CanBusDescriptor) -> CanEvent | None:
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        event = CanEvent()
        if self._ffi_can_recv_event(ctypes.c_uint8(bus_index), ctypes.byref(event)):
            return event
        return None

    def _can_recv_events(
        self, bus: int | str | CanBusDescriptor, capacity: int
    ) -> tuple[CanEvent, ...]:
        if capacity <= 0:
            return ()
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        events = (CanEvent * capacity)()
        count = int(
            self._ffi_can_recv_events(
                ctypes.c_uint8(bus_index),
                events,
                ctypes.c_uint32(capacity),
            )
        )
        return tuple(events[index] for index in range(count))

    def _can_send_events(
        self, bus: int | str | CanBusDescriptor, events: tuple[object, ...]
    ) -> int:
        if not events:
            return 0
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        packets = (CanPacket * len(events))()
        for index, event in enumerate(events):
            if not isinstance(event, CanEvent):
                raise TypeError(
                    f"CAN datapaths require CanEvent payloads, got {type(event).__name__}"
                )
            packets[index] = event.packet
        return int(
            self._can_send_many(
                ctypes.c_uint8(bus_index),
                packets,
                ctypes.c_uint32(len(events)),
            )
        )

    def _can_recv_message(self, message: CanMessageDescriptor) -> CanEvent | None:
        latest = None
        while self._can_tx_count_value(message.bus):
            event = self._can_recv_event(message.bus)
            if event is not None and event.packet.id == message.id:
                latest = event
        return latest

    def _can_rx_count_value(self, bus: int | str | CanBusDescriptor) -> int:
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        return int(self._can_rx_count(ctypes.c_uint8(bus_index)))

    def _can_tx_count_value(self, bus: int | str | CanBusDescriptor) -> int:
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        return int(self._can_tx_count(ctypes.c_uint8(bus_index)))

    def _can_decode_signal_raw(
        self, bus: int | str | CanBusDescriptor, packet: CanPacket, signal_name: str
    ) -> float:
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        value = ctypes.c_double()
        ok = self._decode_can_signal(
            ctypes.c_uint8(bus_index),
            ctypes.byref(packet),
            signal_name.encode(),
            ctypes.byref(value),
        )
        if not ok:
            raise ValueError(
                f"failed to decode CAN signal {signal_name!r} from packet id={packet.id}"
            )
        return float(value.value)

    def _can_decode_message_raw(
        self,
        bus: int | str | CanBusDescriptor,
        packet: CanPacket,
        signal_names: list[str] | tuple[str, ...],
    ) -> dict[str, float]:
        if not signal_names:
            return {}
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        encoded_names = tuple(signal_name.encode() for signal_name in signal_names)
        name_array = (ctypes.c_char_p * len(encoded_names))(*encoded_names)
        values = (CanSignalValue * len(encoded_names))()
        decoded = int(
            self._decode_can_signals(
                ctypes.c_uint8(bus_index),
                ctypes.byref(packet),
                name_array,
                values,
                ctypes.c_uint32(len(encoded_names)),
            )
        )
        if decoded != len(encoded_names):
            failed_signal = (
                signal_names[decoded]
                if decoded < len(signal_names)
                else signal_names[-1]
            )
            raise ValueError(
                f"failed to decode CAN signal {failed_signal!r} from packet id={packet.id}"
            )
        return {
            signal_name: float(values[index].value)
            for index, signal_name in enumerate(signal_names)
        }

    def _can_encode_signal_raw(
        self,
        bus: int | str | CanBusDescriptor,
        message_name: str,
        signal_name: str,
        value: float,
    ) -> CanPacket:
        packet = CanPacket()
        self._encode_can_signal_into(bus, message_name, signal_name, value, packet)
        return packet

    def _can_encode_message_raw(
        self, bus: int | str | CanBusDescriptor, message_name: str, **signals: float
    ) -> CanPacket:
        packet = CanPacket()
        if not signals:
            return packet
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        signal_names = tuple(signals)
        encoded_names = tuple(signal_name.encode() for signal_name in signal_names)
        name_array = (ctypes.c_char_p * len(encoded_names))(*encoded_names)
        values = (CanSignalValue * len(signal_names))()
        for index, signal_name in enumerate(signal_names):
            values[index].value = float(signals[signal_name])
        encoded = int(
            self._encode_can_signals(
                ctypes.c_uint8(bus_index),
                message_name.encode(),
                name_array,
                values,
                ctypes.c_uint32(len(signal_names)),
                ctypes.byref(packet),
            )
        )
        if encoded != len(signal_names):
            failed_signal = (
                signal_names[encoded]
                if encoded < len(signal_names)
                else signal_names[-1]
            )
            raise ValueError(
                f"failed to encode CAN signal {failed_signal!r} for {message_name!r}"
            )
        return packet

    def _encode_can_signal_into(
        self,
        bus: int | str | CanBusDescriptor,
        message_name: str,
        signal_name: str,
        value: float,
        packet: CanPacket,
    ) -> None:
        bus_index = self._coerce_can_bus(bus)
        self._require_can_bus(bus_index)
        ok = self._encode_can_signal(
            ctypes.c_uint8(bus_index),
            message_name.encode(),
            signal_name.encode(),
            ctypes.c_double(value),
            ctypes.byref(packet),
        )
        if not ok:
            raise ValueError(
                f"failed to encode CAN signal {signal_name!r} for {message_name!r}"
            )

    def _configure_abi(self) -> None:
        if self.has_can:
            self._bind_can_codec()
            self._bind_can_codegen()

    def _configure_datapaths(self) -> None:
        for path in self._model_datapaths():
            self.configure_datapath(path)

        if self.has_can:
            for bus in self.can.buses:
                path = ClusterCanComms.path(bus)
                self.datapaths.add_output(
                    path,
                    pending=lambda bus=bus: self.can.tx_count(bus),
                    recv=lambda bus=bus: self.can.recv(bus),
                    recv_many=lambda count, bus=bus: self._can_recv_events(bus, count),
                )
                self.datapaths.add_input(
                    path,
                    send=lambda event, bus=bus: self.can.send(
                        bus,
                        event.packet.id,
                        event.packet.payload,
                    ),
                    send_many=lambda events, bus=bus: self._can_send_events(
                        bus, events
                    ),
                )

    def configure_datapath(self, path: DataPath) -> None:
        if self.datapaths.outputs(path) or self.datapaths.inputs(path):
            return
        peripheral = self._peripheral_model(path)
        self.datapaths.add_output(
            path,
            pending=lambda path=path, peripheral=peripheral: peripheral.output_count(
                path
            ),
            recv=lambda path=path, peripheral=peripheral: peripheral.recv(path),
            recv_many=lambda count,
            path=path,
            peripheral=peripheral: peripheral.recv_many(path, count),
        )
        self.datapaths.add_input(
            path,
            send=lambda event,
            path=path,
            peripheral=peripheral: peripheral.send_payload(path, event),
            send_many=lambda events,
            path=path,
            peripheral=peripheral: peripheral.send_payloads(path, events),
        )

    def supports_datapath(self, path: DataPath) -> bool:
        return self._peripheral_model_or_none(path) is not None

    def _peripheral_model(self, path: DataPath):
        peripheral = self._peripheral_model_or_none(path)
        if peripheral is not None:
            return peripheral
        raise ValueError(
            f"datapath {path!r} is not backed by a known controller peripheral"
        )

    def _peripheral_model_or_none(self, path: DataPath):
        try:
            if self._timer_peripherals.supports(path):
                return self._timer_peripherals
            if self._spi_peripherals.supports(path):
                return self._spi_peripherals
        except ValueError:
            return None
        return None

    def _model_datapaths(self) -> tuple[DataPath, ...]:
        paths = []
        for index in range(int(self._datapath_count())):
            descriptor = _ModelDataPathDescriptorAbi()
            if not self._datapath_descriptor(
                ctypes.c_uint32(index), ctypes.byref(descriptor)
            ):
                continue
            paths.append(self._datapath_from_descriptor(descriptor))
        return tuple(paths)

    def _datapath_from_descriptor(
        self, descriptor: _ModelDataPathDescriptorAbi
    ) -> DataPath:
        if descriptor.interface == RIG_MODEL_DATAPATH_TIMER_DUTY:
            return self.timer.duty_events(descriptor.port, descriptor.channel)
        if descriptor.interface == RIG_MODEL_DATAPATH_TIMER_FREQUENCY:
            return self.timer.frequency_events(descriptor.port, descriptor.channel)
        if descriptor.interface == RIG_MODEL_DATAPATH_SPI_TRANSACTION:
            return self.spi.transactions(descriptor.device)
        raise ValueError(
            f"unsupported Rust model datapath interface {descriptor.interface}"
        )

    def _library_from_env(self) -> pathlib.Path | None:
        library_path = os.environ.get(self.env_var)
        return pathlib.Path(library_path) if library_path else None

    def _build_library(self) -> pathlib.Path:
        return buck_output(self.buck_target, self._root)

    def _resolve_library_path(
        self, library_path: str | pathlib.Path | None
    ) -> pathlib.Path:
        path = pathlib.Path(library_path) if library_path else self._library_from_env()
        if path is None:
            path = self._build_library()
        return path.resolve()

    def _configure_model_abi(self) -> None:
        self._new = self._bind_symbol("rig_model_new")
        self._run_for = self._bind_symbol("rig_model_run_for", [ctypes.c_uint64])
        self._fast_forward_for = self._bind_symbol(
            "rig_model_fast_forward_for", [ctypes.c_uint64]
        )
        self._next_scheduler_step = self._bind_symbol(
            "rig_model_next_scheduler_step",
            [ctypes.c_uint64],
            ctypes.c_uint64,
        )
        self._datapath_count = self._bind_symbol(
            "rig_model_datapath_count",
            restype=ctypes.c_uint32,
        )
        self._datapath_descriptor = self._bind_symbol(
            "rig_model_datapath_descriptor",
            [ctypes.c_uint32, ctypes.POINTER(_ModelDataPathDescriptorAbi)],
            ctypes.c_bool,
        )
        self._set_analog_input = self._bind_symbol(
            "rig_model_set_analog_input",
            [ctypes.c_int, ctypes.c_float],
        )
        self._get_analog_input = self._bind_symbol(
            "rig_model_get_analog_input",
            [ctypes.c_int],
            ctypes.c_float,
        )
        self._set_digital_io = self._bind_symbol(
            "rig_model_set_digital_io",
            [ctypes.c_int, ctypes.c_bool],
        )
        self._get_digital_io = self._bind_symbol(
            "rig_model_get_digital_io",
            [ctypes.c_int],
            ctypes.c_bool,
        )
        self._get_fault = self._bind_optional_symbol(
            "rig_model_get_fault",
            [ctypes.c_int],
            ctypes.c_bool,
        )
        self._can_bus_count = self._bind_symbol(
            "rig_model_can_bus_count",
            restype=ctypes.c_uint8,
        )
        self._can_send = self._bind_symbol(
            "rig_model_can_send",
            [ctypes.c_uint8, ctypes.POINTER(CanPacket)],
            ctypes.c_bool,
        )
        self._can_recv = self._bind_symbol(
            "rig_model_can_recv",
            [ctypes.c_uint8, ctypes.POINTER(CanPacket)],
            ctypes.c_bool,
        )
        self._ffi_can_recv_event = self._bind_symbol(
            "rig_model_can_recv_event",
            [ctypes.c_uint8, ctypes.POINTER(CanEvent)],
            ctypes.c_bool,
        )
        self._ffi_can_recv_events = self._bind_symbol(
            "rig_model_can_recv_events",
            [ctypes.c_uint8, ctypes.POINTER(CanEvent), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self._can_send_many = self._bind_symbol(
            "rig_model_can_send_many",
            [ctypes.c_uint8, ctypes.POINTER(CanPacket), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self._can_rx_count = self._bind_symbol(
            "rig_model_can_rx_count",
            [ctypes.c_uint8],
            ctypes.c_uint32,
        )
        self._can_tx_count = self._bind_symbol(
            "rig_model_can_tx_count",
            [ctypes.c_uint8],
            ctypes.c_uint32,
        )
        self._timer_send_duty = self._bind_symbol(
            "rig_model_timer_send_duty",
            [ctypes.POINTER(TimerChannelEvent)],
            ctypes.c_bool,
        )
        self._timer_recv_duty = self._bind_symbol(
            "rig_model_timer_recv_duty",
            [ctypes.c_int, ctypes.c_int, ctypes.POINTER(TimerChannelEvent)],
            ctypes.c_bool,
        )
        self._timer_recv_duties = self._bind_symbol(
            "rig_model_timer_recv_duties",
            [
                ctypes.c_int,
                ctypes.c_int,
                ctypes.POINTER(TimerChannelEvent),
                ctypes.c_uint32,
            ],
            ctypes.c_uint32,
        )
        self._timer_duty_output_count = self._bind_symbol(
            "rig_model_timer_duty_output_count",
            [ctypes.c_int, ctypes.c_int],
            ctypes.c_uint32,
        )
        self._timer_send_frequency = self._bind_symbol(
            "rig_model_timer_send_frequency",
            [ctypes.POINTER(TimerChannelEvent)],
            ctypes.c_bool,
        )
        self._timer_send_duties = self._bind_symbol(
            "rig_model_timer_send_duties",
            [ctypes.POINTER(TimerChannelEvent), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self._timer_recv_frequency = self._bind_symbol(
            "rig_model_timer_recv_frequency",
            [ctypes.c_int, ctypes.c_int, ctypes.POINTER(TimerChannelEvent)],
            ctypes.c_bool,
        )
        self._timer_recv_frequencies = self._bind_symbol(
            "rig_model_timer_recv_frequencies",
            [
                ctypes.c_int,
                ctypes.c_int,
                ctypes.POINTER(TimerChannelEvent),
                ctypes.c_uint32,
            ],
            ctypes.c_uint32,
        )
        self._timer_frequency_output_count = self._bind_symbol(
            "rig_model_timer_frequency_output_count",
            [ctypes.c_int, ctypes.c_int],
            ctypes.c_uint32,
        )
        self._timer_send_frequencies = self._bind_symbol(
            "rig_model_timer_send_frequencies",
            [ctypes.POINTER(TimerChannelEvent), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self._timer_send_capture = self._bind_symbol(
            "rig_model_timer_send_capture",
            [ctypes.POINTER(TimerCaptureEvent)],
            ctypes.c_bool,
        )
        self._spi_send = self._bind_symbol(
            "rig_model_spi_send",
            [ctypes.POINTER(SpiTransaction)],
            ctypes.c_bool,
        )
        self._spi_send_many = self._bind_symbol(
            "rig_model_spi_send_many",
            [ctypes.POINTER(SpiTransaction), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self._spi_recv = self._bind_symbol(
            "rig_model_spi_recv",
            [ctypes.c_int, ctypes.POINTER(SpiTransaction)],
            ctypes.c_bool,
        )
        self._spi_recv_many = self._bind_symbol(
            "rig_model_spi_recv_many",
            [ctypes.c_int, ctypes.POINTER(SpiTransaction), ctypes.c_uint32],
            ctypes.c_uint32,
        )
        self._spi_output_count = self._bind_symbol(
            "rig_model_spi_output_count",
            [ctypes.c_int],
            ctypes.c_uint32,
        )

    def _require_can_bus(self, bus: int) -> None:
        bus_count = self._can_bus_count_value()
        if bus < 0 or bus >= bus_count:
            raise ValueError(
                f"CAN bus {bus} out of range for model with {bus_count} buses"
            )

    def _coerce_can_bus(self, bus: int | str | CanBusDescriptor) -> int:
        if isinstance(bus, CanBusDescriptor):
            return bus.index
        if isinstance(bus, str):
            descriptor = self._can_buses_by_name.get(bus.lower())
            if descriptor is not None:
                return descriptor.index
            raise ValueError(f"CAN bus {bus!r} was not found")
        return int(bus)

    def _bind_model_symbol(
        self,
        suffix: str,
        argtypes: list[object] | None = None,
        restype: object | None = None,
    ):
        return self._bind_symbol(f"{self.symbol_prefix}_{suffix}", argtypes, restype)

    def _bind_symbol(
        self,
        name: str,
        argtypes: list[object] | None = None,
        restype: object | None = None,
    ):
        symbol = getattr(self._lib, name)
        symbol.argtypes = [] if argtypes is None else argtypes
        symbol.restype = restype
        return symbol

    def _bind_optional_symbol(
        self,
        name: str,
        argtypes: list[object] | None = None,
        restype: object | None = None,
    ):
        try:
            return self._bind_symbol(name, argtypes, restype)
        except AttributeError:
            return None

    @staticmethod
    def _function_address(symbol) -> int:
        value = ctypes.cast(symbol, ctypes.c_void_p).value
        if value is None:
            raise RuntimeError(f"could not resolve function pointer for {symbol!r}")
        return int(value)

    def _bind_can_codec(self) -> None:
        decode_name = "rig_model_can_decode_signal"
        encode_name = "rig_model_can_encode_signal"
        self._decode_can_signal = self._bind_symbol(
            decode_name,
            [
                ctypes.c_uint8,
                ctypes.POINTER(CanPacket),
                ctypes.c_char_p,
                ctypes.POINTER(ctypes.c_double),
            ],
            ctypes.c_bool,
        )
        self._decode_can_signals = self._bind_symbol(
            "rig_model_can_decode_signals",
            [
                ctypes.c_uint8,
                ctypes.POINTER(CanPacket),
                ctypes.POINTER(ctypes.c_char_p),
                ctypes.POINTER(CanSignalValue),
                ctypes.c_uint32,
            ],
            ctypes.c_uint32,
        )
        self._encode_can_signal = self._bind_symbol(
            encode_name,
            [
                ctypes.c_uint8,
                ctypes.c_char_p,
                ctypes.c_char_p,
                ctypes.c_double,
                ctypes.POINTER(CanPacket),
            ],
            ctypes.c_bool,
        )
        self._encode_can_signals = self._bind_symbol(
            "rig_model_can_encode_signals",
            [
                ctypes.c_uint8,
                ctypes.c_char_p,
                ctypes.POINTER(ctypes.c_char_p),
                ctypes.POINTER(CanSignalValue),
                ctypes.c_uint32,
                ctypes.POINTER(CanPacket),
            ],
            ctypes.c_uint32,
        )

    def _bind_can_codegen(self) -> None:
        prefix = "rig_model_can_codegen_"
        self._can_codegen_bus_count = self._bind_symbol(
            prefix + "bus_count", restype=ctypes.c_uint8
        )
        self._can_codegen_bus_name = self._bind_symbol(
            prefix + "bus_name",
            [ctypes.c_uint8, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_message_count = self._bind_symbol(
            prefix + "message_count", restype=ctypes.c_uint32
        )
        self._can_codegen_message_descriptor = self._bind_symbol(
            prefix + "message_descriptor",
            [ctypes.c_uint32, ctypes.POINTER(_CanMessageDescriptorAbi)],
            ctypes.c_bool,
        )
        self._can_codegen_message_name = self._bind_symbol(
            prefix + "message_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_tx_message_count = self._bind_symbol(
            prefix + "tx_message_count", restype=ctypes.c_uint32
        )
        self._can_codegen_tx_message_descriptor = self._bind_symbol(
            prefix + "tx_message_descriptor",
            [ctypes.c_uint32, ctypes.POINTER(_CanMessageDescriptorAbi)],
            ctypes.c_bool,
        )
        self._can_codegen_tx_message_name = self._bind_symbol(
            prefix + "tx_message_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_signal_count = self._bind_symbol(
            prefix + "signal_count", restype=ctypes.c_uint32
        )
        self._can_codegen_signal_descriptor = self._bind_symbol(
            prefix + "signal_descriptor",
            [ctypes.c_uint32, ctypes.POINTER(_CanSignalDescriptorAbi)],
            ctypes.c_bool,
        )
        self._can_codegen_signal_message_name = self._bind_symbol(
            prefix + "signal_message_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_signal_name = self._bind_symbol(
            prefix + "signal_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_signal_unit = self._bind_symbol(
            prefix + "signal_unit",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_signal_enum_name = self._bind_symbol(
            prefix + "signal_enum_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_tx_signal_count = self._bind_symbol(
            prefix + "tx_signal_count", restype=ctypes.c_uint32
        )
        self._can_codegen_tx_signal_descriptor = self._bind_symbol(
            prefix + "tx_signal_descriptor",
            [ctypes.c_uint32, ctypes.POINTER(_CanSignalDescriptorAbi)],
            ctypes.c_bool,
        )
        self._can_codegen_tx_signal_message_name = self._bind_symbol(
            prefix + "tx_signal_message_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_tx_signal_name = self._bind_symbol(
            prefix + "tx_signal_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_tx_signal_unit = self._bind_symbol(
            prefix + "tx_signal_unit",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_tx_signal_enum_name = self._bind_symbol(
            prefix + "tx_signal_enum_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_enum_value_count = self._bind_symbol(
            prefix + "enum_value_count", restype=ctypes.c_uint32
        )
        self._can_codegen_enum_value_descriptor = self._bind_symbol(
            prefix + "enum_value_descriptor",
            [ctypes.c_uint32, ctypes.POINTER(_CanEnumValueDescriptorAbi)],
            ctypes.c_bool,
        )
        self._can_codegen_enum_value_enum_name = self._bind_symbol(
            prefix + "enum_value_enum_name",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )
        self._can_codegen_enum_value_label = self._bind_symbol(
            prefix + "enum_value_label",
            [ctypes.c_uint32, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
            ctypes.c_bool,
        )

    def _load_can_metadata(self) -> dict[str, object]:
        if self._can_metadata is not None:
            return self._can_metadata
        cache_key = str(self.library_path)
        cached = _CAN_METADATA_CACHE.get(cache_key)
        if cached is not None:
            self._can_metadata = cached
            return cached

        buses = tuple(
            CanBusDescriptor(
                index, self._read_codegen_string(self._can_codegen_bus_name, index)
            )
            for index in range(int(self._can_codegen_bus_count()))
        )
        metadata = {
            "buses": buses,
            "messages": self._read_can_messages(
                self._can_codegen_message_count,
                self._can_codegen_message_descriptor,
                self._can_codegen_message_name,
                buses,
            ),
            "tx_messages": self._read_can_messages(
                self._can_codegen_tx_message_count,
                self._can_codegen_tx_message_descriptor,
                self._can_codegen_tx_message_name,
                buses,
            ),
            "signals": self._read_can_signals(
                self._can_codegen_signal_count,
                self._can_codegen_signal_descriptor,
                self._can_codegen_signal_message_name,
                self._can_codegen_signal_name,
                self._can_codegen_signal_unit,
                self._can_codegen_signal_enum_name,
                buses,
            ),
            "tx_signals": self._read_can_signals(
                self._can_codegen_tx_signal_count,
                self._can_codegen_tx_signal_descriptor,
                self._can_codegen_tx_signal_message_name,
                self._can_codegen_tx_signal_name,
                self._can_codegen_tx_signal_unit,
                self._can_codegen_tx_signal_enum_name,
                buses,
            ),
        }
        enum_values = self._read_can_enum_values()
        metadata["enum_values"] = enum_values
        metadata["enums"] = self._build_can_enums(enum_values)
        _CAN_METADATA_CACHE[cache_key] = metadata
        self._can_metadata = metadata
        return metadata

    def _load_can_indexes(self) -> dict[str, object]:
        if self._can_indexes is not None:
            return self._can_indexes
        cache_key = str(self.library_path)
        cached = _CAN_INDEX_CACHE.get(cache_key)
        if cached is not None:
            self._can_indexes = cached
            return cached

        def message_index(
            messages: tuple[CanMessageDescriptor, ...],
        ) -> dict[tuple[str, int | None], CanMessageDescriptor]:
            by_key: dict[tuple[str, int | None], CanMessageDescriptor] = {}
            names_seen: set[str] = set()
            ambiguous_names: set[str] = set()
            for message in messages:
                by_key[(message.name, message.bus)] = message
                if message.name in names_seen:
                    ambiguous_names.add(message.name)
                names_seen.add(message.name)
            for message in messages:
                if message.name not in ambiguous_names:
                    by_key[(message.name, None)] = message
            return by_key

        all_signals = self._can_signals + self._can_tx_signals
        signal_enum_names = {
            signal.signal_name: signal.enum_name
            for signal in all_signals
            if signal.enum_name
        }
        signal_kinds = {signal.signal_name: signal.kind for signal in all_signals}
        signals_by_message: dict[tuple[int, int, str], list[str]] = {}
        for signal in all_signals:
            signals_by_message.setdefault(
                (signal.bus, signal.message_id, signal.message_name),
                [],
            ).append(signal.signal_name)

        self._can_indexes = {
            "buses_by_name": {bus.name.lower(): bus for bus in self._can_buses},
            "rx_messages_by_key": message_index(self._can_messages),
            "tx_messages_by_key": message_index(self._can_tx_messages),
            "signal_enum_names": signal_enum_names,
            "signal_kinds": signal_kinds,
            "signals_by_message": {
                key: tuple(signal_names)
                for key, signal_names in signals_by_message.items()
            },
            "tx_signals_by_key": {
                (
                    signal.bus,
                    signal.message_id,
                    signal.message_name,
                    signal.signal_name,
                ): signal
                for signal in self._can_tx_signals
            },
        }
        _CAN_INDEX_CACHE[cache_key] = self._can_indexes
        return self._can_indexes

    def _read_can_messages(
        self,
        count_fn,
        descriptor_fn,
        name_fn,
        buses: tuple[CanBusDescriptor, ...],
    ) -> tuple[CanMessageDescriptor, ...]:
        messages = []
        for index in range(int(count_fn())):
            descriptor = _CanMessageDescriptorAbi()
            if not descriptor_fn(ctypes.c_uint32(index), ctypes.byref(descriptor)):
                raise RuntimeError(f"failed to read CAN message descriptor {index}")
            messages.append(
                CanMessageDescriptor(
                    bus=int(descriptor.bus),
                    bus_name=buses[int(descriptor.bus)].name,
                    name=self._read_codegen_string(name_fn, index),
                    id=int(descriptor.id),
                    len=int(descriptor.len),
                )
            )
        return tuple(messages)

    def _read_can_signals(
        self,
        count_fn,
        descriptor_fn,
        message_name_fn,
        signal_name_fn,
        unit_fn,
        enum_name_fn,
        buses: tuple[CanBusDescriptor, ...],
    ) -> tuple[CanSignalDescriptor, ...]:
        signals = []
        for index in range(int(count_fn())):
            descriptor = _CanSignalDescriptorAbi()
            if not descriptor_fn(ctypes.c_uint32(index), ctypes.byref(descriptor)):
                raise RuntimeError(f"failed to read CAN signal descriptor {index}")
            unit = self._read_codegen_string(unit_fn, index)
            enum_name = self._read_codegen_string(enum_name_fn, index)
            signals.append(
                CanSignalDescriptor(
                    index=int(index),
                    bus=int(descriptor.bus),
                    bus_name=buses[int(descriptor.bus)].name,
                    message_name=self._read_codegen_string(message_name_fn, index),
                    message_id=int(descriptor.message_id),
                    signal_name=self._read_codegen_string(signal_name_fn, index),
                    unit=unit or None,
                    kind=SIGNAL_KIND_NAMES.get(
                        int(descriptor.kind), f"Unknown({int(descriptor.kind)})"
                    ),
                    enum_name=enum_name or None,
                )
            )
        return tuple(signals)

    def _read_can_enum_values(self) -> tuple[CanEnumValueDescriptor, ...]:
        enum_values = []
        for index in range(int(self._can_codegen_enum_value_count())):
            descriptor = _CanEnumValueDescriptorAbi()
            if not self._can_codegen_enum_value_descriptor(
                ctypes.c_uint32(index), ctypes.byref(descriptor)
            ):
                raise RuntimeError(f"failed to read CAN enum value descriptor {index}")
            enum_values.append(
                CanEnumValueDescriptor(
                    enum_name=self._read_codegen_string(
                        self._can_codegen_enum_value_enum_name, index
                    ),
                    label=self._read_codegen_string(
                        self._can_codegen_enum_value_label, index
                    ),
                    raw=int(descriptor.raw),
                )
            )
        return tuple(enum_values)

    def _build_can_enums(
        self,
        enum_values: tuple[CanEnumValueDescriptor, ...],
    ) -> CanEnumNamespace:
        values_by_enum: dict[str, dict[str, int]] = {}
        for enum_value in enum_values:
            members = values_by_enum.setdefault(enum_value.enum_name, {})
            members[python_enum_member(enum_value.label)] = enum_value.raw
            members[python_enum_attr_member(enum_value.label)] = enum_value.raw
        return CanEnumNamespace(
            {
                enum_name: IntEnum(python_enum_class_name(enum_name), values)
                for enum_name, values in values_by_enum.items()
            }
        )

    def _signals_for_message(self, message: CanMessageDescriptor) -> tuple[str, ...]:
        signals = self._signals_by_message.get(
            (message.bus, message.id, message.name), ()
        )
        if not signals:
            raise KeyError(
                f"CAN message {message.name!r} has no generated signal metadata"
            )
        return signals

    def _can_tx_signal_descriptor(
        self,
        message: CanMessageDescriptor,
        signal_name: str,
    ) -> CanSignalDescriptor:
        try:
            return self._can_tx_signals_by_key[
                (message.bus, message.id, message.name, signal_name)
            ]
        except KeyError as exc:
            raise KeyError(
                f"CAN TX signal {signal_name!r} was not found on message "
                f"{message.name!r} ({message.id:#x}) bus {message.bus_name!r}"
            ) from exc

    def _coerce_decoded_can_value(self, signal_name: str, value: float) -> object:
        enum_name = self._signal_enum_names.get(signal_name)
        if enum_name:
            enum_type = self._can_enum(enum_name)
            return enum_type(int(value))
        return bool(value) if self._signal_kind(signal_name) == "Boolean" else value

    def _signal_kind(self, signal_name: str) -> str | None:
        return self._signal_kinds.get(signal_name)

    def _read_codegen_string(self, symbol, index: int) -> str:
        for size in (256, 1024, 4096):
            buffer = ctypes.create_string_buffer(size)
            if symbol(index, buffer, ctypes.c_size_t(size)):
                return buffer.value.decode()
        raise RuntimeError(f"failed to read generated CAN string {index}")
