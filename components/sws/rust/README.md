# SWS in Rust and RTIC

Production SWS targets now build this `no_std` STM32F103 application. RTIC owns
TIM2 (1 kHz, priority 1) and CAN RX0 (priority 2). A single timer task runs the
100 Hz driver/vehicle state work, 10 Hz screen work, and 1 Hz heartbeat. CAN
interrupts feed a bounded queue; application work runs outside the CAN lock.
There is no FreeRTOS or C application code in the production ELF.

Reusable CAN receive state/scheduling, UDS/ISO-TP, input debounce/timers and
STM32 CAN/GPIO/clock support live in [`components/shared/rust`](../../shared/rust/README.md).
SWS supplies protocol adapters, CAN IDs, clock settings, input thresholds and the
board pin map. Its driver, telemetry, screen and sleep behavior remain here.

Shared build rules and tools live in
[`components/shared/rust/build_tools`](../../shared/rust/build_tools/README.md).
SWS uses one `rust_firmware_targets` call, and its linker scripts define the
board layout. `build.rs` calls the shared `firmware-build` helper.

The existing C application remains at `//components/sws:legacy-elf`. The SWS SIL
model now runs this Rust application's timer-task body, Yamcan codecs and shared
CAN queues through the RTIC simulation bindings.

## Simulate

```sh
buckle build //sim/models/controllers/sws:sil-so
buckle test //sim/models/controllers/sws:test
buckle test //sim/tests:sws-cfr25 //sim/tests:sws-cfr26 //sim/tests:sws-cfr27
```

The existing `SwsModel` button, request, CAN and cluster interfaces are preserved.
The model exposes `tick_count`, `rx_dropped` and `reset_count` for task/queue
inspection. CAN receive runs before the 1 kHz timer task, with the firmware's
32-frame receive queue and ordered diagnostic responses. Large simulation steps
execute every intervening timer interrupt; fractional steps retain their remainder.
UDS resets restart application state while preserving physical input levels and
already emitted responses. Diagnostic image metadata comes from the platform's
production CRC binary.

This is software-in-the-loop task simulation. It does not execute the Cortex-M
ELF or emulate NVIC preemption, CPU cycles, electrical CAN arbitration or bus-off.
Task utilization reports zero in simulation; hardware retains DWT measurements.

## Build and deploy

Install the pinned compiler and target once:

```sh
rustup toolchain install 1.86.0 --profile minimal
rustup target add thumbv7m-none-eabi --toolchain 1.86.0
buckle build //components/sws:rust-tests //components/sws:rust-build-tests //components/sws:crc-cfr26 --show-output
```

`elf`, `bin`, `bin_crc`, `crc-cfr25`, `crc-cfr26`, and `crc-cfr27` all resolve to
Rust firmware. `bin_crc` uses the existing STM32 CRC tooling. The descriptor is
at `0x08002000`, the vectors at `0x08002100`, and the STM32 word-order CRC is
appended at the address recorded in the descriptor. Flash is limited to 64 KiB
and RAM to 20 KiB; linking reserves at least 2 KiB for stack.

Inspect the link map:

```sh
buckle build '//components/sws:rust-firmware[map]' --show-output
```

With the vehicle CAN connection configured as for the existing downloader:

```sh
buckle run //components/sws:download-cfr26
```

The existing `deploy-cfr26`, `ota-cfr26`, and `gdb-cfr26` targets also consume the
Rust artifacts. Substitute `cfr25` or `cfr27` for the other vehicle platforms.
Builds do not connect to hardware or flash it.

Cargo dependencies are pinned in this package's separate `Cargo.lock`. Buckle
tracks the application, shared crate and generated protocol as action inputs; Cargo builds
in an isolated temporary directory using Rust 1.86.0. The initial build requires
access to crates.io. Set `FIRMWARE_BUILD_TMPDIR` before starting the Buckle daemon to
choose a larger scratch volume if necessary.

The Yamcan Python tools follow the repository's existing build environment. If a
local uv configuration redirects public packages to an unavailable private index,
start the daemon with `UV_NO_CONFIG=1` for this public repository.

## Compatibility and verification

CAN encoding and decoding use Yamcan's generated `no_std` Rust wrappers
(`generate_embedded_rust`), with the same allocation-free wire codec as Yamcan's
host Rust wrappers. Yamcan owns bit positions, enums, scales, timeouts, counters
and checksum seeds; SWS owns receive timestamps and transmit scheduling. The driver
request golden packet was checked against the actual legacy C signal packers.
Tests cover input bounce and holds, reverse priority, crash reset lockout, page
repeat, configuration unlock, stale CAN data, warnings, sleep inhibition and the
conUDS diagnostics wire format. The linker enforces flash/RAM limits and stack headroom; deployment consumes
the CRC image directly.

Behavior corrections in the port:

- Debounce timers reset when a transition commits, so the old press timer cannot
  immediately commit a release.
- Expired launch feedback stops overriding page navigation; CAN getters enforce
  the network-defined timeout even across the millisecond counter wrap.
- The resolver-calibration failure warning reads the VCREAR fault message that
  owns its bit, rather than the VCPDU fault message.

Application diagnostics retain the existing 100 ms session expiry, little-endian
DIDs, no DID echo, 128-byte ISO-TP request buffer and 10 ms delayed hard reset.
The bootloader still performs flash transfer. RTIC task utilization is measured
with DWT and reported in `taskUsage1kHz` for the combined timer task; the other
usage fields are zero because those rates run inside that task. Iteration fields
retain their cumulative counts and network bit widths.

The eight active-low inputs use PB5, PB7–PB13. CAN uses PA11/PA12 and an active-low
open-drain transceiver enable on PA8; PC13 provides the heartbeat. The clock is
64 MHz from an 8 MHz HSE, with CAN at 1 Mbit/s. The legacy unused analog channels
are not sampled by this application.

Build and host checks do not establish operation on a physical board. Before
vehicle use, verify bootloader startup, interrupt/timer timing, physical inputs,
CAN behavior under congestion/bus-off, sleep/wake behavior and reset/download on
a bench board. The SWS SIL tests exercise the Rust application.

## This workspace's built bundle

`target/sws-rtic/` contains the CRC image, ELF, map,
hashes, UDS manifest and a built Linux downloader. With the existing
`can0` interface configured, `./target/sws-rtic/deploy.sh` performs the download.
`./target/sws-rtic/deploy.sh --help` displays usage without accessing hardware.

Final builds pass in the original workspace using the shared build tools.
The copied bundle can deploy without rebuilding.
