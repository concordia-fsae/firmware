# GNSS application C tests

These tests adapt the three GPS scenarios from `jl/c-unit-test` commit
`1aaf23415e8c46cb54cae0b7af70c0ff7bee479f`. They use the shared C/Unity test
infrastructure and FreeRTOS helpers, the real LwGPS parser with VCFront's
configuration, and the real timer implementation. Bytes enter the captured DMA
buffer and are processed through the application's public module callbacks;
there are no parser-only test hooks. YIC formatting and decoding live in
`libs/lib_yicGnss`; these application tests exercise that library through the DMA
consumer. Direct, RTOS-free library tests live in `libs/tests/yicGnss`.

Run from the repository root:

```sh
buckle test //components/shared/code/app/tests/gps:gps_test
buckle test //components/shared/code/app/tests/gps:gps_crc_test
buckle test //components/shared/code/app/tests/gps:gps_sanitized
```

The 47 tests cover:

- GGA/GSA/GSV/RMC and PAIRMSG 90/91 decoding, counters, getters, signed position,
  heading/speed conversion, quality mapping, and fix/time/date validity.
- Every split point and DMA wrap point of a representative sentence, batched
  sentences, no duplicate consumption, LF/CRLF, exact size limits, noise,
  embedded NULs, truncated frames, and resynchronization at `$`.
- Full and overfull DMA buffers, restart and subsequent valid reception,
  partial-sentence reset, UART error bit accounting, and initialization reset.
- Configurable checksum validation before state updates, invalid checksum syntax, strict
  PAIRMSG numeric/field validation, UTC boundaries, and preservation of fields
  belonging to the other PAIRMSG type.
- Timeout boundaries, tick rollover, invalid traffic not extending health,
  stale time/date invalidation, and recovery on valid traffic.

The default target uses VCFront's `LWGPS_CFG_CRC=0`, accepting checksum
mismatches from the receiver. The `gps_crc_test` variant enables that existing
flag for both the application and LwGPS and verifies mismatch rejection. Both
NMEA and PAIRMSG obey this flag; frame and field syntax checks remain enabled.
The suite also runs under AddressSanitizer and UndefinedBehaviorSanitizer.

DMA writes are deterministic, and UART start/stop calls succeed in this harness.
The reset test models the application-facing stop/reset/start sequence; it does
not execute the STM32 UART ISR. Real ISR/DMA races, HAL failures, and peripheral
restart timing still need hardware integration testing. The overrun detector
uses nonzero data in a previously consumed slot; arbitrary full laps of all-zero
noise cannot be distinguished using the modulo DMA position alone.

Tests exposed and drove fixes for the previously ineffective overrun check,
zero-byte consumer stalls, unbounded/wrapping sentence assembly, failed PAIRMSG
field parses refreshing health, and stale time/date validity.
The original reference stream is retained byte-for-byte and injected as one DMA
batch, including its whitespace and mismatched RMC checksum (`75`). The default
target accepts this stream and produces valid navigation. The CRC-enabled target
rejects that RMC sentence and recovers on a subsequent matching checksum (`74`).
The nominal test uses the matching checksum.

## Receiver protocol checks

Reference: [YIC GT-505GGBL5-DR specification, section 3](https://www.yic.com.tw/wp-content/uploads/2025/08/YIC-GT-505GGBL5-DR.pdf).
The GT-605GGBL5-DR specification uses the same sentence examples. The installed
part number has not been confirmed from the BOM.

| Sentence | Application contract tested |
| --- | --- |
| GGA | GP/GN decoding of position, altitude, UTC, satellite count and quality method |
| GSA | GP/GN fix dimension; system IDs 1–4; dimension changes without a new GGA |
| GSV | GP/GN counting; multipart sequences; signal IDs 1/4 and empty SNR; no navigation mutation |
| RMC | GP/GN validity, speed, course and date; position and UTC remain sourced from GGA |
| GLL / VTG / ZDA | Unsupported: counted as invalid; must not mutate or refresh navigation |
| GL / GA / GB / BD GSV | Unsupported talkers: same rejection contract |
| PAIRMSG 90 | Stages 0–3 and UTC millisecond limits |
| PAIRMSG 91 | Dynamic states 0–2, all eight alarm bits and preservation of DR stage |

Every sentence in the original reference stream is also split at every byte
boundary while crossing the DMA wrap boundary. Expected acceptance/rejection is
checked under both checksum configurations.

GGA quality is a positioning method, not a dimension. The conversion to the
existing CAN `gpsQualityIndicator` is explicit:

| YIC GGA | Meaning | CAN quality |
| --- | --- | --- |
| 0 | No fix | 0 (`FIX_NONE`) |
| 1 | SPS/autonomous | 1 (`SPS`) |
| 2 | Differential | 2 (`DIFFERENTIAL`) |
| 3 | PPS | 3 (`PPS`) |
| 6 | Dead reckoning | 4 (`DEAD_RECKONING`) |

The separate `gpsFixDimension` reports GSA's 1 (no fix), 2 (2D), or 3 (3D).
It reports 0 (unknown) before receiving GSA or after its timeout. A stale GGA
reports CAN quality 0. A fresh GSA never modifies the GGA method, and vice versa.
There is no additional `gpsGgaQuality` CAN signal. These changes revise the
meaning/width of the existing quality enum; firmware and CAN decoders must use
the updated network definition together. The clock manager checks both fields.

## Validity rules and audit

- Navigation requires individually fresh GGA and RMC reports (less than 2000 ms
  old), an accepted GGA method, RMC status A, and RMC mode other than N.
- GNSS methods (GGA 1/2/3) also require a fresh GSA reporting 2D or 3D. Missing,
  stale, malformed, or no-fix GSA invalidates a satellite fix.
- GGA 6 is dead reckoning: it still requires valid, fresh GGA/RMC, but permits
  unknown/no-fix GSA because satellite positioning may be unavailable.
- PAIRMSG/GSV traffic can keep receiver-liveness healthy but cannot refresh
  navigation. UART/DMA reset invalidates the previous navigation snapshot.
- Time/date flags require valid navigation and validated calendar/UTC fields.
  GGA and RMC UTC must agree to whole seconds before time/date are marked valid;
  this avoids combining a new time with an earlier date across midnight.
- Before LwGPS updates decoded data, `lib_yicGnss` validates field counts,
  supported states, coordinate ranges/hemispheres, UTC/calendar ranges, speed,
  course, satellite count and altitude. Overlong fields that LwGPS would truncate
  are rejected. The application uses malformed-source results to invalidate its
  source timer. Malformed navigation messages do not refresh liveness. Proper no-fix messages are accepted but not marked valid.
- CRC mismatch handling still follows `LWGPS_CFG_CRC`; checksum/frame failures
  are dropped and existing source data expire normally.

Tests cover source-specific freshness and recovery, unknown/no-fix states,
all GGA/GSA/RMC arrival orders, malformed fields, leap days, midnight epoch
mismatch, DMA reset, and LF-only parser commits. The VCFront sim additionally
checks the encoded/decoded CAN diagnostics without receiver data.

These checks cover the documented sentence families and the application's
current acceptance/rejection behavior, not exhaustive NMEA grammar validation.
GLL/VTG/ZDA and non-GP/GN GSV remain unsupported. Satellite details and unused
DOP/geoidal/differential-correction fields are not fully validated or exposed.
Navigation freshness does not imply physical position accuracy. Repeated,
well-formed reports of the same epoch cannot establish whether the receiver's
underlying solution is frozen; the freshness checks track reception.
