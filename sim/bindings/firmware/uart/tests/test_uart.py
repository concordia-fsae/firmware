"""Public UART facade against the real native binding, without GNSS firmware."""

import ctypes
import os

import pytest

from rig import ComponentRig, PeriodicDataPathProducer
from sim.bindings.firmware.runtime import FirmwareClusterRig
from sim.bindings.firmware.uart import UartInterface


class UartModel(ComponentRig):
    uart = UartInterface()

    def __init__(self):
        super().__init__()
        self.library = ctypes.CDLL(os.environ["RIG_RUNTIME_LIB"])

    def _bind_symbol(self, name, argtypes=None, restype=None):
        symbol = getattr(self.library, name)
        symbol.argtypes = argtypes
        symbol.restype = restype
        return symbol

    def configure(self, port, mode, capacity=0):
        configure = self._bind_symbol(
            "rig_runtime_uart_configure_rx",
            [ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32],
            ctypes.c_bool,
        )
        assert configure(port, mode, capacity)

    def start(self, port, size):
        buffer = ctypes.create_string_buffer(size)
        start = self._bind_symbol(
            "rig_runtime_uart_start_rx",
            [ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32],
            ctypes.c_bool,
        )
        assert start(port, buffer, size)
        return buffer

    def read(self, port, size):
        buffer = ctypes.create_string_buffer(size)
        read = self._bind_symbol(
            "rig_runtime_uart_read_rx",
            [ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32],
            ctypes.c_int64,
        )
        count = read(port, buffer, size)
        assert count >= 0
        return buffer.raw[:count]


@pytest.fixture
def model():
    model = UartModel()
    reset = model._bind_symbol("rig_runtime_uart_reset", [], None)
    reset()
    yield model
    reset()


def test_uart_facade_preserves_binary_bytes_and_circular_position(model):
    model.configure(0, 0)
    buffer = model.start(0, 4)
    assert model.uart is model.uart
    assert model.uart.receive(0, b"a\x00b") == 3
    assert buffer.raw == b"a\x00b\x00"
    assert model.uart.status(0).remaining == 1
    assert model.uart.receive(0, b"cd") == 2
    assert buffer.raw == b"d\x00bc"
    status = model.uart.status(0)
    assert status.remaining == 3
    assert status.accepted == 5
    assert status.dropped == 0
    with pytest.raises(ValueError):
        model.uart.receive(99, b"unconfigured")


def test_uart_facade_single_shot_stops_at_capacity(model):
    model.configure(0, 1)
    buffer = model.start(0, 4)
    assert model.uart.receive(0, b"abcdef") == 4
    assert buffer.raw == b"abcd"
    status = model.uart.status(0)
    assert status.remaining == 0
    assert status.completed
    assert not status.active
    assert status.accepted == 4
    assert status.dropped == 2
    assert model.uart.receive(0, b"z") == 0
    assert buffer.raw == b"abcd"


def test_uart_facade_polling_fifo_is_bounded_and_ordered(model):
    model.configure(0, 2, capacity=4)
    assert model.uart.receive(0, b"ab\x00cde") == 4
    assert model.read(0, 2) == b"ab"
    assert model.uart.receive(0, b"xy") == 2
    assert model.read(0, 8) == b"\x00cxy"
    assert model.read(0, 8) == b""
    status = model.uart.status(0)
    assert status.queued == 0
    assert status.accepted == 6
    assert status.dropped == 2


def test_uart_datapath_accepts_a_scheduled_10hz_stream(model):
    model.configure(0, 2, capacity=64)
    produced_at = []

    def payload(producer):
        produced_at.append(producer.elapsed_ns)
        return b"a\x00b"

    source = PeriodicDataPathProducer(model.uart.rx(0), payload, scheduler_period=100)
    cluster = FirmwareClusterRig(receiver=model, components=(source,))
    cluster.run_for(99, step=1)
    assert model.uart.status(0).accepted == 0
    cluster.run_for(1, step=1)
    assert model.read(0, 8) == b"a\x00b"
    cluster.run_for(900, step=10)
    assert produced_at == [index * 100_000_000 for index in range(1, 11)]
    assert model.read(0, 64) == b"a\x00b" * 9
    assert model.uart.status(0).accepted == 30
    assert model.uart.status(0).dropped == 0
