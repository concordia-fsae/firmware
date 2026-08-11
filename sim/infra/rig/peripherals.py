from __future__ import annotations

import ctypes
from enum import IntEnum

from .datapath import DataPath, PeripheralBinding


class TimerChannelEvent(ctypes.Structure):
    _fields_ = [
        ("port", ctypes.c_int32),
        ("channel", ctypes.c_int32),
        ("value", ctypes.c_float),
        ("timestamp_ns", ctypes.c_uint64),
    ]


class TimerCaptureEvent(ctypes.Structure):
    _fields_ = [
        ("channel", ctypes.c_int32),
        ("value", ctypes.c_float),
        ("timestamp_ns", ctypes.c_uint64),
    ]


class SpiTransaction(ctypes.Structure):
    MAX_BYTES = 256
    _fields_ = [
        ("device", ctypes.c_int32),
        ("tx_len", ctypes.c_uint16),
        ("rx_len", ctypes.c_uint16),
        ("tx_data", ctypes.c_uint8 * MAX_BYTES),
        ("rx_data", ctypes.c_uint8 * MAX_BYTES),
        ("timestamp_ns", ctypes.c_uint64),
    ]

    @classmethod
    def from_payload(
        cls,
        device: int,
        *,
        tx_payload: bytes | bytearray | list[int] | tuple[int, ...] = (),
        rx_payload: bytes | bytearray | list[int] | tuple[int, ...] = (),
        timestamp_ns: int = 0,
    ) -> SpiTransaction:
        tx_bytes = bytes(tx_payload)
        rx_bytes = bytes(rx_payload)
        if len(tx_bytes) > cls.MAX_BYTES:
            raise ValueError(f"SPI TX payload must be at most {cls.MAX_BYTES} bytes")
        if len(rx_bytes) > cls.MAX_BYTES:
            raise ValueError(f"SPI RX payload must be at most {cls.MAX_BYTES} bytes")

        transaction = cls()
        transaction.device = int(device)
        transaction.tx_len = len(tx_bytes)
        transaction.rx_len = len(rx_bytes)
        transaction.timestamp_ns = int(timestamp_ns)
        for index, value in enumerate(tx_bytes):
            transaction.tx_data[index] = value
        for index, value in enumerate(rx_bytes):
            transaction.rx_data[index] = value
        return transaction

    @property
    def tx_payload(self) -> bytes:
        return bytes(self.tx_data[: self.tx_len])

    @property
    def rx_payload(self) -> bytes:
        return bytes(self.rx_data[: self.rx_len])


class TimerPeripheralInterface:
    _TIMER_DUTY = "timer.duty"
    _TIMER_FREQUENCY = "timer.frequency"

    def __init__(self, model: NodeRig) -> None:
        self._model = model

    @classmethod
    def timer_duty_events(cls, port: object, channel: object) -> DataPath:
        return cls._timer_events(cls._TIMER_DUTY, "timer", "duty", port, channel)

    @classmethod
    def timer_frequency_events(cls, port: object, channel: object) -> DataPath:
        return cls._timer_events(
            cls._TIMER_FREQUENCY, "timer", "frequency", port, channel
        )

    @classmethod
    def _timer_events(
        cls,
        interface: str,
        peripheral: object,
        event: object,
        port: object,
        channel: object,
    ) -> DataPath:
        return DataPath.peripheral(
            peripheral,
            event,
            port,
            channel,
            binding=PeripheralBinding(
                interface,
                channel=int(channel),
                port=int(port),
            ),
        )

    @classmethod
    def supports(cls, path: DataPath) -> bool:
        binding = _peripheral_binding(path)
        return binding.interface in (cls._TIMER_DUTY, cls._TIMER_FREQUENCY)

    def send(
        self,
        path: DataPath,
        *,
        channel: int = 0,
        value: float = 0.0,
        timestamp_ns: int = 0,
    ) -> bool:
        binding = _peripheral_binding(path)
        event = TimerChannelEvent()
        event.port = int(binding.port if binding.port is not None else 0)
        event.channel = int(binding.channel if binding.channel is not None else channel)
        event.value = float(value)
        event.timestamp_ns = int(timestamp_ns)
        return self._send_event(binding, event)

    def send_payload(self, path: DataPath, payload: object) -> bool:
        if not isinstance(payload, TimerChannelEvent):
            raise TypeError(
                f"timer datapaths require TimerChannelEvent payloads, got {type(payload).__name__}"
            )
        return self._send_event(_peripheral_binding(path), payload)

    def send_payloads(self, path: DataPath, payloads: tuple[object, ...]) -> int:
        if not payloads:
            return 0
        binding = _peripheral_binding(path)
        events = (TimerChannelEvent * len(payloads))()
        for index, payload in enumerate(payloads):
            if not isinstance(payload, TimerChannelEvent):
                raise TypeError(
                    f"timer datapaths require TimerChannelEvent payloads, got {type(payload).__name__}"
                )
            events[index] = payload
        send_many = self._send_many_symbol(binding)
        return int(send_many(events, ctypes.c_uint32(len(payloads))))

    def recv(
        self,
        path: DataPath,
    ) -> TimerChannelEvent | None:
        binding = _peripheral_binding(path)
        event = TimerChannelEvent()
        recv = self._recv_symbol(binding)
        if recv(
            ctypes.c_int(binding.port if binding.port is not None else 0),
            ctypes.c_int(binding.channel if binding.channel is not None else 0),
            ctypes.byref(event),
        ):
            return event
        return None

    def recv_many(
        self,
        path: DataPath,
        capacity: int,
    ) -> tuple[TimerChannelEvent, ...]:
        if capacity <= 0:
            return ()
        binding = _peripheral_binding(path)
        events = (TimerChannelEvent * capacity)()
        recv_many = self._recv_many_symbol(binding)
        count = int(
            recv_many(
                ctypes.c_int(binding.port if binding.port is not None else 0),
                ctypes.c_int(binding.channel if binding.channel is not None else 0),
                events,
                ctypes.c_uint32(capacity),
            )
        )
        return tuple(events[index] for index in range(count))

    def output_count(
        self,
        path: DataPath,
    ) -> int:
        binding = _peripheral_binding(path)
        count = self._count_symbol(binding)
        return int(
            count(
                ctypes.c_int(binding.port if binding.port is not None else 0),
                ctypes.c_int(binding.channel if binding.channel is not None else 0),
            )
        )

    def _send_event(self, binding: PeripheralBinding, event: TimerChannelEvent) -> bool:
        if binding.interface == self._TIMER_DUTY:
            return bool(self._model._timer_send_duty(ctypes.byref(event)))
        if binding.interface == self._TIMER_FREQUENCY:
            return bool(self._model._timer_send_frequency(ctypes.byref(event)))
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _send_many_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_send_duties
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_send_frequencies
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _recv_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_recv_duty
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_recv_frequency
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _recv_many_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_recv_duties
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_recv_frequencies
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def _count_symbol(self, binding: PeripheralBinding):
        if binding.interface == self._TIMER_DUTY:
            return self._model._timer_duty_output_count
        if binding.interface == self._TIMER_FREQUENCY:
            return self._model._timer_frequency_output_count
        raise ValueError(f"unsupported timer interface {binding.interface!r}")

    def rust_route_abi(self, path: DataPath) -> tuple[int, int, int, int, int, int]:
        binding = _peripheral_binding(path)
        interface = (
            1 if binding.interface == self._TIMER_DUTY else 2
        )
        return (
            interface,
            int(binding.port if binding.port is not None else 0),
            int(binding.channel if binding.channel is not None else 0),
            self._model._function_address(self._count_symbol(binding)),
            self._model._function_address(self._recv_many_symbol(binding)),
            self._model._function_address(self._send_many_symbol(binding)),
        )


class SpiPeripheralInterface:
    _SPI_TRANSACTION = "spi.transaction"

    def __init__(self, model: NodeRig) -> None:
        self._model = model

    @classmethod
    def transactions(cls, device: object) -> DataPath:
        return DataPath.peripheral(
            "spi",
            "transactions",
            device,
            binding=PeripheralBinding(
                cls._SPI_TRANSACTION,
                device=int(device),
            ),
        )

    @classmethod
    def supports(cls, path: DataPath) -> bool:
        return _peripheral_binding(path).interface == cls._SPI_TRANSACTION

    def send_payload(self, path: DataPath, payload: object) -> bool:
        _peripheral_binding(path)
        if not isinstance(payload, SpiTransaction):
            raise TypeError(
                f"SPI datapaths require SpiTransaction payloads, got {type(payload).__name__}"
            )
        return bool(self._model._spi_send(ctypes.byref(payload)))

    def send_payloads(self, path: DataPath, payloads: tuple[object, ...]) -> int:
        _peripheral_binding(path)
        if not payloads:
            return 0
        transactions = (SpiTransaction * len(payloads))()
        for index, payload in enumerate(payloads):
            if not isinstance(payload, SpiTransaction):
                raise TypeError(
                    f"SPI datapaths require SpiTransaction payloads, got {type(payload).__name__}"
                )
            transactions[index] = payload
        return int(
            self._model._spi_send_many(transactions, ctypes.c_uint32(len(payloads)))
        )

    def recv(self, path: DataPath) -> SpiTransaction | None:
        binding = _peripheral_binding(path)
        transaction = SpiTransaction()
        if self._model._spi_recv(
            ctypes.c_int(binding.device if binding.device is not None else 0),
            ctypes.byref(transaction),
        ):
            return transaction
        return None

    def recv_many(self, path: DataPath, capacity: int) -> tuple[SpiTransaction, ...]:
        if capacity <= 0:
            return ()
        binding = _peripheral_binding(path)
        transactions = (SpiTransaction * capacity)()
        count = int(
            self._model._spi_recv_many(
                ctypes.c_int(binding.device if binding.device is not None else 0),
                transactions,
                ctypes.c_uint32(capacity),
            )
        )
        return tuple(transactions[index] for index in range(count))

    def output_count(self, path: DataPath) -> int:
        binding = _peripheral_binding(path)
        return int(
            self._model._spi_output_count(
                ctypes.c_int(binding.device if binding.device is not None else 0),
            )
        )

    def rust_route_abi(self, path: DataPath) -> tuple[int, int, int, int]:
        binding = _peripheral_binding(path)
        return (
            int(binding.device if binding.device is not None else 0),
            self._model._function_address(self._model._spi_output_count),
            self._model._function_address(self._model._spi_recv_many),
            self._model._function_address(self._model._spi_send_many),
        )


def _peripheral_binding(path: DataPath) -> PeripheralBinding:
    if path.peripheral_binding is None:
        raise ValueError(f"datapath {path!r} is not backed by a controller peripheral")
    return path.peripheral_binding


class TimerInterface:
    def __init__(
        self,
        port_enum: type[IntEnum] | None = None,
        channel_enum: type[IntEnum] | None = None,
    ) -> None:
        self._port_enum = port_enum
        self._channel_enum = channel_enum

    def duty_events(self, port: object, channel: object) -> DataPath:
        port, channel = self._coerce(port, channel)
        return TimerPeripheralInterface.timer_duty_events(port, channel)

    def frequency_events(self, port: object, channel: object) -> DataPath:
        port, channel = self._coerce(port, channel)
        return TimerPeripheralInterface.timer_frequency_events(port, channel)

    def _coerce(
        self, port: object, channel: object
    ) -> tuple[IntEnum | int, IntEnum | int]:
        return (
            self._coerce_enum(self._port_enum, port, "timer port"),
            self._coerce_enum(self._channel_enum, channel, "timer channel"),
        )

    @staticmethod
    def _coerce_enum(
        enum_type: type[IntEnum] | None, value: object, label: str
    ) -> IntEnum | int:
        if enum_type is None:
            return int(value)  # type: ignore[arg-type]
        if isinstance(value, enum_type):
            return value
        try:
            return enum_type(value)
        except ValueError as exc:
            raise ValueError(
                f"{value!r} is not a valid {label} for {enum_type.__name__}"
            ) from exc


class SpiInterface:
    def __init__(self, device_enum: type[IntEnum] | None = None) -> None:
        self._device_enum = device_enum

    def transactions(self, device: object) -> DataPath:
        device = self._coerce_device(device)
        return SpiPeripheralInterface.transactions(device)

    def _coerce_device(self, device: object) -> IntEnum | int:
        if self._device_enum is None:
            return int(device)  # type: ignore[arg-type]
        if isinstance(device, self._device_enum):
            return device
        try:
            return self._device_enum(device)
        except ValueError as exc:
            raise ValueError(
                f"{device!r} is not a valid SPI device for {self._device_enum.__name__}"
            ) from exc
