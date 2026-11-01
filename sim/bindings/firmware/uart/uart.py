from __future__ import annotations

import ctypes
from dataclasses import dataclass

from rig.datapath import DataPath


class _RxStatusAbi(ctypes.Structure):
    _fields_ = [
        ("accepted", ctypes.c_uint64),
        ("dropped", ctypes.c_uint64),
        ("remaining", ctypes.c_uint32),
        ("queued", ctypes.c_uint32),
        ("active", ctypes.c_uint32),
        ("completed", ctypes.c_uint32),
    ]


@dataclass(frozen=True)
class UartRxStatus:
    accepted: int
    dropped: int
    remaining: int
    queued: int
    active: bool
    completed: bool


class UartInterface:
    """UART RX facade; controller initialization selects the receive mode."""

    def __get__(self, model, owner=None):
        if model is None:
            return self
        peripheral = model.__dict__.get("_uart_peripheral")
        if peripheral is None:
            peripheral = UartPeripheralInterface(model)
            model.__dict__["_uart_peripheral"] = peripheral
        return peripheral


class UartPeripheralInterface:
    def __init__(self, model):
        self._model = model
        self._paths: dict[int, DataPath] = {}
        self._receive = model._bind_symbol(
            "rig_uart_receive",
            argtypes=[ctypes.c_int32, ctypes.c_char_p, ctypes.c_uint32],
            restype=ctypes.c_int64,
        )
        self._status = model._bind_symbol(
            "rig_uart_rx_status",
            argtypes=[ctypes.c_int32, ctypes.POINTER(_RxStatusAbi)],
            restype=ctypes.c_bool,
        )

    @staticmethod
    def _port(port: int) -> int:
        port = int(port)
        if not 0 <= port <= 0x7FFFFFFF:
            raise ValueError(f"Invalid UART port {port}")
        return port

    def receive(self, port: int, data: bytes) -> int:
        """Deliver bytes now, between simulation steps; return accepted count.

        Inactive/full receivers drop bytes. Consult status() for totals. This
        models arrival, not a reliable transport that retries rejected bytes.
        """
        port = self._port(port)
        if not isinstance(data, bytes):
            raise TypeError("UART payload must be bytes")
        if len(data) > 0xFFFFFFFF:
            raise ValueError("UART payload exceeds ABI size limit")
        accepted = int(self._receive(port, data, len(data)))
        if accepted < 0:
            raise ValueError(f"UART port {port} is not configured")
        return accepted

    def status(self, port: int) -> UartRxStatus:
        port = self._port(port)
        status = _RxStatusAbi()
        if not self._status(port, ctypes.byref(status)):
            raise ValueError(f"UART port {port} is not configured")
        return UartRxStatus(
            status.accepted,
            status.dropped,
            status.remaining,
            status.queued,
            bool(status.active),
            bool(status.completed),
        )

    def rx(self, port: int) -> DataPath[bytes]:
        """Return a registered input path for a scheduled byte-stream producer.

        Pass this same path to PeriodicDataPathProducer and add the producer
        before running the cluster. Use scheduler steps no larger than the
        desired injection interval; UART does not add a separate timing loop.
        """
        port = self._port(port)
        self.status(port)  # Fail early for an unconfigured peripheral.
        if port not in self._paths:
            path = DataPath.named("uart_rx", port)

            def send(payload: bytes) -> bool:
                self.receive(port, payload)
                # Delivery consumes the event even if hardware drops bytes.
                return True

            self._model.datapaths.add_input(path, send=send)
            self._paths[port] = path
        return self._paths[port]
