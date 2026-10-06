# SWS Rust / RTIC migration

1. Inventory the legacy SWS pin map, task periods, driver combinations, screen/config behavior, CAN validation, UDS services and bootloader ABI.
2. Use Yamcan’s generated no_std Rust wrappers and shared Rust wire codec so message IDs, layouts, discrete values, checksums and timeouts retain one source of truth.
3. Port debounce, driver requests, configuration, warnings, vehicle sleep and application diagnostics to hardware-independent Rust. Verify timing and protocol behavior on the host.
4. Run the application on STM32F103 under RTIC with a 1 kHz TIM2 task, interrupt-driven CAN reception, bounded queues and the existing 64 MHz clock / GPIO configuration.
5. Make Buckle build the locked embedded Cargo package, extract the binary, apply the existing STM32 CRC, and connect existing platform deployment targets to the Rust output. Keep the C implementation available as a migration reference and for legacy SIL.
6. Build production artifacts, inspect linker memory bounds and verify deployment target construction. Document commands and any validation that still requires a physical board.

Acceptance: Buckle produces an ARM ELF and CRC binary for all SWS platforms, host tests exercise behavioral boundaries, and the binary follows the current bootloader ABI. Physical board and vehicle verification are separate from build verification.

## Completed validation

- Shared build rules, Cargo runner/build-script helper are extracted into `components/shared/rust/build_tools`; SWS supplies sources and linker scripts.
- Shared runtime support is extracted into `components/shared/rust`, with application-supplied configuration and declared Buckle inputs.
- Rust 1.86.0 / RTIC 2.2 embedded build passes under Buckle.
- All 19 SWS and 3 shared library host tests, plus 2 shared build-tool checks, pass; embedded Clippy passes with warnings denied.
- CFR25, CFR26 and CFR27 production/deploy targets pass and produce the same CRC image.
- The CAN download target builds and its command wrapper runs successfully with `--help`.
- Yamcan host and embedded Rust wrappers produce identical packets and decoded values for representative driver, sleep and configuration signals; the host Yamcan Rust library also builds.
- Legacy C packers independently produce the same default driver-request packet as Rust.
- Final image: 5,508 bytes; static RAM: 2,036 bytes; remaining RAM above static data: 18,444 bytes. The linker enforces at least 2 KiB for stack.
- Deployment bundle saved to `target/sws-rtic/`: ELF, raw/CRC binaries, map, SHA-256 hashes, UDS manifest and a compressed Linux downloader with a launcher.

Earlier builds used an isolated source workspace while the host disk was full.
The shared build-tool targets now pass in the original workspace. The deployment
bundle is refreshed there. No physical flash or board/vehicle test was performed.
