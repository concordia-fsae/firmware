# YIC GNSS decoder tests

`lib_yicGnss` owns YIC/NMEA framing, checksum policy, field counts/formats/ranges,
PAIRMSG decoding, LwGPS adaptation and raw receiver method/dimension values. It
accepts one byte at a time into a caller-owned context and returns sentence
results plus decoded data. No UART, DMA, RTOS, timer, CAN or fault-manager code
is linked into these tests.

`app_gps` owns transport/recovery, counters, reception freshness, combined fix
validity, clock-epoch coherence, fault reporting and conversion to CAN enums.
The library's source flags are decoded receiver reports, not application health.
For example, it can decode valid RMC fields without GSA; the application still
requires a fresh GSA dimension before claiming a valid satellite fix.

Run from the repository root:

```sh
buckle test //components/shared/code/libs/tests/yicGnss:
buckle test //components/shared/code/app/tests/gps:
```

The six library tests run with CRC disabled, CRC enabled and sanitizers. They
check independent contexts, incremental input, classified malformed-source
results, checksum policy, resynchronization, stream reset versus initialization,
and raw data independent of application validity. The 47 application tests retain
the full existing DMA/protocol/freshness regression suite.

A framing/checksum error has an unknown source. A field-validation error names
the malformed GGA/GSA/RMC source so the application can invalidate its timer.
`resetStream` discards only assembly state; application recovery must separately
invalidate freshness. `init` clears the entire context. Callers serialize access
and treat the context's decoded data as read-only.
