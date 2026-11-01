# GNSS application C tests

These tests adapt the three GPS scenarios from `jl/c-unit-test` commit
`1aaf23415e8c46cb54cae0b7af70c0ff7bee479f`. They use the shared C/Unity test
infrastructure and FreeRTOS helpers, the real LwGPS parser with VCFront's
configuration, and the real timer implementation. Bytes enter the captured DMA
buffer and are processed through the application's public module callbacks;
there are no parser-only test hooks.

Run from the repository root:

```sh
buckle test //components/shared/code/app/tests/gps:gps_test
buckle test //components/shared/code/app/tests/gps:gps_crc_test
buckle test //components/shared/code/app/tests/gps:gps_sanitized
```

The 35 tests cover:

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

GGA quality is a positioning method, not a dimension: 0 is unavailable, 1 is SPS,
2 is differential, 3 is PPS, and 6 is dead reckoning. The quality regression matrix
crosses these methods (plus an unsupported value) with missing/no-fix/2D/3D/invalid
GSA modes. Navigation validity remains based on GGA/RMC; a missing GSA does not
invalidate an otherwise usable position, but cannot justify reporting 2D or 3D.

These checks cover the documented sentence families and the application's
current acceptance/rejection behavior. They do not claim exhaustive NMEA grammar
validation or support for the unsupported messages above. The pinned LwGPS
parser is permissive about standard sentence fields; the application does not
yet validate every numeric range, field count, or contradictory cross-sentence
status. Satellite details, GSA system ID and GSV signal ID are not exposed by the
application. Their presence must not disturb the fields we consume.
