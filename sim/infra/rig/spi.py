from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum

from .datapath import (
    DataPath,
    PeripheralBinding,
    PeripheralInterface,
    require_peripheral_binding,
)


@dataclass(frozen=True)
class SpiRouteEndpoint:
    device: int
    count: int
    recv_many: int
    send_many: int

    @property
    def scalar_source_route_id(self) -> None:
        return None

    def compatible_with(self, sink: object) -> bool:
        return isinstance(sink, SpiRouteEndpoint)

    def connect(
        self, runtime: object, *, source_node: str, sink_node: str, sink: object
    ) -> bool:
        if not isinstance(sink, SpiRouteEndpoint):
            return False
        return runtime.add_spi_route(
            source_node=source_node,
            device=self.device,
            source_count=self.count,
            source_recv_many=self.recv_many,
            sink_node=sink_node,
            sink_send_many=sink.send_many,
        )


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


class SpiPeripheralInterface:
    _SPI_TRANSACTION = PeripheralInterface.SPI_TRANSACTION

    def __init__(self, model: NodeRig) -> None:
        self._model = model

    @classmethod
    def transactions(cls, device: object) -> DataPath:
        return DataPath.peripheral(
            PeripheralInterface.SPI_TRANSACTION,
            device,
            binding=PeripheralBinding(
                cls._SPI_TRANSACTION,
                device=int(device),
            ),
        )

    @classmethod
    def supports(cls, path: DataPath) -> bool:
        return require_peripheral_binding(path).interface == cls._SPI_TRANSACTION

    def send_payload(self, path: DataPath, payload: object) -> bool:
        require_peripheral_binding(path)
        if not isinstance(payload, SpiTransaction):
            raise TypeError(
                f"SPI datapaths require SpiTransaction payloads, got {type(payload).__name__}"
            )
        return bool(self._model._spi_send(ctypes.byref(payload)))

    def send_payloads(self, path: DataPath, payloads: tuple[object, ...]) -> int:
        require_peripheral_binding(path)
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
        binding = require_peripheral_binding(path)
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
        binding = require_peripheral_binding(path)
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
        binding = require_peripheral_binding(path)
        return int(
            self._model._spi_output_count(
                ctypes.c_int(binding.device if binding.device is not None else 0),
            )
        )

    def rust_route_abi(self, path: DataPath) -> SpiRouteEndpoint:
        binding = require_peripheral_binding(path)
        return SpiRouteEndpoint(
            device=int(binding.device if binding.device is not None else 0),
            count=self._model._function_address(self._model._spi_output_count),
            recv_many=self._model._function_address(self._model._spi_recv_many),
            send_many=self._model._function_address(self._model._spi_send_many),
        )


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
