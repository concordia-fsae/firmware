# UART receive simulation

This binding delivers arbitrary bytes to the same buffers read by firmware. It
contains no protocol parser. Controller initialization configures each port
before `Module_Init` starts a receive operation:

```rust
assert!(rig_runtime::uart::configure(
    GPS_PORT,
    rig_runtime::uart::RxMode::CircularDma,
));
```

`RxMode::SingleShotDma` and `RxMode::Polling { capacity }` use the same injection
interface. Configuration is explicit; unknown ports are rejected. A controller's
`reset_node` configures ports after the shared backend reset clears them.

| Receive mode | Incoming bytes | Firmware observation |
| --- | --- | --- |
| Circular DMA | Write into the registered buffer and wrap; overwrite unread bytes on a full lap | Remaining transfers reload to the buffer length at wrap |
| Single-shot DMA | Fill the registered buffer once; drop later bytes until rearmed | Remaining reaches zero; completed is set and the buffer pointer is released |
| Polling | Append to a bounded FIFO; drop new bytes when full | Nonblocking reads remove bytes in order |

`HW_UART_startDMARX`, `HW_UART_stopDMA`, and `HW_UART_getDmaRxRemaining` adapt the
existing firmware API. Starting an already active DMA operation fails; starting
a completed or stopped operation rearms it at position zero. Stop releases the
DMA pointer and clears completion, while preserving port configuration and
lifetime byte counters. Stopping DMA has no effect on a polling FIFO.

The simulation-only C API in `uart_backend.h` exposes mode configuration,
nonblocking polling reads and status. The production UART abstraction currently
has no polling consumer; a future consumer's binding can call
`rig_runtime_uart_read_rx`. Polling reads return immediately and do not advance
time. DMA completion is observable through status, without an emulated interrupt
callback. An interrupt-driven consumer would need a callback bridge.

## Injection and scheduling

Models expose the shared `UartInterface` descriptor. For immediate delivery:

```python
accepted = controller.uart.receive(UartPort.GPS, b"arbitrary\x00bytes")
status = controller.uart.status(UartPort.GPS)
```

`accepted` is the number of bytes delivered to DMA memory or the polling FIFO.
The status contains cumulative `accepted` and `dropped` byte counts, DMA
`remaining`, polling `queued`, and `active`/`completed` flags. Circular overwrite
counts as accepted: the peripheral cannot know which bytes firmware consumed.
Firmware is responsible for detecting ring overrun. Drops are not retried.

For periodic streams, reuse Rig's existing scheduler and producer:

```python
path = controller.uart.rx(UartPort.GPS)
source = PeriodicDataPathProducer(path, payload_for_tick, scheduler_period=100)
cluster.add_component(source)
cluster.run_for(6000, step=10)
```

Register the source before the cluster starts. A producer callable may return
`None` to skip an arrival, bytes for a chunk, or a tuple of chunks. Advance with a
step no larger than the intended arrival interval; larger scheduler steps can
coalesce producer callbacks. Stream callbacks run on simulation time, with no
wall-clock waits. Splitting a sentence across separate source ticks exercises
firmware polling of an incomplete DMA buffer.

Injection represents bytes arriving at a particular simulation boundary. It
writes the entire chunk immediately; this is not baud-rate serialization. Use
smaller chunks at separate scheduled times when arrival timing matters.

## Lifetime and execution

UART state lives in a thread-local `RefCell`, without locks or shared ownership.
Configuration, firmware execution, injection, and reset must all run on the same
simulation thread. Mutable borrows are scoped to each binding call.

Firmware owns DMA memory until stop, reset, or single-shot completion. Injection
runs between firmware scheduler steps, and configuration/reset require a stopped
scheduler. Model reset discards all receive state, configuration, queues,
counters, and stored DMA pointers.

## Tests

```sh
buckle test //sim/bindings/firmware/uart:unit
buckle test //sim/bindings/firmware/uart/tests:python_unit
```
