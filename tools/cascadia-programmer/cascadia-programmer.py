#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
import time
from dataclasses import dataclass
from typing import Any, Dict, Optional, List, Tuple, Union

import can
import yaml  # PyYAML


# ============================================================
# CAN IDs
# Manual's parameter messages are shown as 0x0C1/0x0C2 when
# CAN ID Offset (base) is 0x0A0.
#
# We expose only --id-offset and compute:
#   PARAM_CMD_ID  = id_offset + 0x21
#   PARAM_RESP_ID = id_offset + 0x22
# ============================================================
DELTA_PARAM_CMD = 0x21
DELTA_PARAM_RESP = 0x22


# ============================================================
# Parameter Message payload layout (8 bytes), ALWAYS 2-byte data:
#   Byte 0-1: Parameter Address (uint16 LE)
#   Byte 2  : R/W (0=read, 1=write)
#   Byte 3  : reserved (0)
#   Byte 4-5: data (uint16 or int16 LE)
#   Byte 6-7: reserved (0)
# ============================================================

def u16_to_le(x: int) -> bytes:
    return int(x).to_bytes(2, "little", signed=False)


def le_to_u16(b: bytes) -> int:
    return int.from_bytes(b, "little", signed=False)


def encode_i16_le(value: int, signed: bool) -> bytes:
    if signed:
        if not (-0x8000 <= value <= 0x7FFF):
            raise ValueError("Signed 16-bit value out of range")
        return int(value).to_bytes(2, "little", signed=True)
    else:
        if not (0 <= value <= 0xFFFF):
            raise ValueError("Unsigned 16-bit value out of range")
        return int(value).to_bytes(2, "little", signed=False)


def decode_i16_le(b: bytes, signed: bool) -> int:
    return int.from_bytes(b, "little", signed=signed)


def build_param_cmd(
    arbid_cmd: int,
    addr: int,
    is_write: bool,
    raw_value: int = 0,
    signed: bool = False,
) -> can.Message:
    data = bytearray(8)
    data[0:2] = u16_to_le(addr)
    data[2] = 1 if is_write else 0
    data[3] = 0
    if is_write:
        data[4:6] = encode_i16_le(raw_value, signed=signed)
    return can.Message(arbitration_id=arbid_cmd, data=bytes(data), is_extended_id=False)


def parse_param_resp(arbid_resp: int, msg: can.Message) -> Tuple[int, bool, bytes]:
    """
    Parse Cascadia param response.

    Payload layout (typical):
      Byte 0-1: addr (uint16 LE)
      Byte 2  : status/ack (often 0=ok, nonzero=failure)  <-- varies by device/firmware
      Byte 3  : reserved
      Byte 4-5: data (uint16/int16 LE) (may be absent)
      Byte 6-7: reserved

    NOTE: If you know the exact meaning of byte 2 for your controller, update the ok decode below.
    We treat 0 as OK, nonzero as failure.
    """
    if msg.arbitration_id != arbid_resp:
        raise ValueError("Not a parameter response")

    # Need at least addr (2) + status (1)
    if len(msg.data) < 3:
        raise ValueError("Response too short to contain addr+status")

    addr = le_to_u16(msg.data[0:2])

    # Data bytes (4-5) may be absent if DLC < 6; default them to 0
    b4 = msg.data[4] if len(msg.data) > 4 else 0
    b5 = msg.data[5] if len(msg.data) > 5 else 0
    payload = bytes([b4, b5])

    return addr, True, payload


def recv_param_response(
    bus: can.BusABC,
    arbid_resp: int,
    expected_addr: int,
    timeout_s: float,
) -> Optional[Tuple[int, bool, bytes]]:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        msg = bus.recv(timeout=max(0.0, deadline - time.time()))
        if msg is None:
            continue

        if msg.arbitration_id != arbid_resp:
            continue

        # IMPORTANT: do not reject on DLC here; parse_param_resp handles it
        try:
            addr, ok, payload = parse_param_resp(arbid_resp, msg)
        except Exception:
            continue

        if addr == expected_addr:
            return addr, ok, payload

    return None


# ============================================================
# Retry helper (applies to all reads/writes/verify)
# ============================================================

def _with_retries(
    op_name: str,
    retries: int,
    backoff_s: float,
    fn,
):
    """
    Retry only on TimeoutError. Any other exception fails fast.
    backoff is linear: backoff_s * (attempt+1)
    """
    last_err: Optional[Exception] = None
    for attempt in range(retries + 1):
        try:
            return fn()
        except TimeoutError as e:
            last_err = e
            if attempt >= retries:
                break
            time.sleep(backoff_s * (attempt + 1))
    raise TimeoutError(f"{op_name}: {last_err}")


# ============================================================
# Engineering conversion
# raw = eng * scale
# eng = raw / scale
# ============================================================

@dataclass(frozen=True)
class ParamSpec:
    addr: int
    name: str
    fmt: str
    signed: bool
    scale: float
    unit: Optional[str]
    enum_map: Optional[Dict[int, str]]

    def is_eeprom(self) -> bool:
        return 100 <= self.addr <= 499

    def raw_to_eng(self, raw: int) -> Any:
        if self.enum_map is not None:
            return self.enum_map.get(raw, raw)
        if self.fmt.lower() == "boolean":
            return bool(raw)
        if self.scale == 0:
            return float(raw)
        return float(raw) / self.scale

    def eng_to_raw(self, eng: Any) -> int:
        if self.enum_map is not None:
            # enum writing handled elsewhere (label -> int)
            if isinstance(eng, int):
                return eng
            raise ValueError("Enum param requires string label or integer raw")
        if self.fmt.lower() == "boolean":
            return 1 if bool(eng) else 0
        if not isinstance(eng, (int, float)):
            raise ValueError(f"Expected numeric value, got {type(eng)}")
        return int(round(float(eng) * self.scale))


# ============================================================
# Command + EEPROM Parameter registry
# From Cascadia Motion CAN Protocol (V6_3) tables 2.3.3 + 2.3.4
# raw = eng * scale
# eng = raw / scale
# ============================================================

PARAMS: Dict[int, ParamSpec] = {
    # ----------------------------
    # 2.3.3 Command Parameters
    # ----------------------------
    10: ParamSpec(10, "Flux command", "Flux", True, 100.0, "Wb", None),
    11: ParamSpec(11, "Resolver PWM Delay Command", "Unsigned integer", False, 1.0, None, None),
    20: ParamSpec(20, "Fault Clear", "Boolean", False, 1.0, None, None),
    21: ParamSpec(21, "Set PWM Frequency", "Unsigned integer", False, 1.0, "kHz", None),
    # value>0 => enabled and Kp_Shudder = value/100, value==0 disables
    23: ParamSpec(23, "Shudder Compensation Gain Control", "Unsigned integer", False, 100.0, None, None),
    30: ParamSpec(30, "OBD2 Enable Command", "Unsigned integer", False, 1.0, None, None),

    # ----------------------------
    # 2.3.4 EEPROM Parameters
    # ----------------------------

    # 2.3.4.1 Motor Configuration
    150: ParamSpec(150, "Motor Parameter Set", "Unsigned char", False, 1.0, None, None),
    151: ParamSpec(151, "Resolver PWM Delay", "Unsigned integer", False, 1.0, None, None),
    152: ParamSpec(152, "Gamma Adjust", "Degrees", True, 1.0, "deg", None),
    154: ParamSpec(154, "Sin Offset", "Low Voltage", False, 100.0, "V", None),
    155: ParamSpec(155, "Cos Offset", "Low Voltage", False, 100.0, "V", None),

    # 2.3.4.2 System Configuration
    140: ParamSpec(140, "Pre-charge Bypassed", "Boolean", False, 1.0, None, None),
    142: ParamSpec(142, "Inverter Run Mode", "Boolean", False, 1.0, None, {0: "Torque Mode", 1: "Speed Mode"}),
    143: ParamSpec(143, "Inverter Command Mode", "Boolean", False, 1.0, None, {0: "CAN Mode", 1: "VSM Mode"}),
    149: ParamSpec(149, "Key Switch Mode", "Unsigned integer", False, 1.0, None, {
        0: "Simple on/off switch",
        1: "Ignition switch (momentary START)"
    }),
    170: ParamSpec(170, "Relay Output State", "Unsigned integer", False, 1.0, None, None),
    173: ParamSpec(173, "Discharge Enable", "Unsigned integer", False, 1.0, None, None),
    174: ParamSpec(174, "Serial Number", "Unsigned integer", False, 1.0, None, None),
    204: ParamSpec(204, "Analog Output Function Select", "Unsigned integer", False, 1.0, None, None),

    # CAN Configuration
    141: ParamSpec(141, "CAN ID Offset", "Unsigned integer", False, 1.0, None, None),
    144: ParamSpec(144, "CAN Extended Message Identifier", "Boolean", False, 1.0, None, {0: "Standard", 1: "Extended"}),
    145: ParamSpec(145, "CAN Term Resistor Present", "Boolean", False, 1.0, None, None),
    146: ParamSpec(146, "CAN Command Message Active", "Boolean", False, 1.0, None, None),
    147: ParamSpec(147, "CAN Bit Rate", "Unsigned integer", False, 1.0, "kbps", {
        125: "125Kbps", 250: "250Kbps", 500: "500Kbps", 1000: "1Mbps"
    }),
    148: ParamSpec(148, "CAN Active Messages Lo Word", "Unsigned integer", False, 1.0, None, None),
    158: ParamSpec(158, "CAN Diagnostic Data Transmit Active", "Boolean", False, 1.0, None, None),
    159: ParamSpec(159, "CAN Inverter Enable Switch Active", "Boolean", False, 1.0, None, None),
    171: ParamSpec(171, "CAN J1939 Option Active", "Boolean", False, 1.0, None, {0: "Not active", 1: "Active"}),
    172: ParamSpec(172, "CAN Timeout", "Unsigned integer", False, (1.0 / 3.0), "ms", None),  # counts of 3ms
    177: ParamSpec(177, "CAN OBD2 Enable", "Unsigned integer", False, 1.0, None, None),
    178: ParamSpec(178, "CAN BMS Limit Enable", "Boolean", False, 1.0, None, None),
    233: ParamSpec(233, "CAN Slave Cmd ID", "Unsigned integer", False, 1.0, None, None),
    234: ParamSpec(234, "CAN Slave Dir", "Unsigned integer", False, 1.0, None, {0: "Same as Master", 1: "Opposite of Master"}),
    235: ParamSpec(235, "CAN Fast Msg Rate", "Unsigned integer", False, 1.0, "ms", None),
    236: ParamSpec(236, "CAN Slow Msg Rate", "Unsigned integer", False, 1.0, "ms", None),
    237: ParamSpec(237, "CAN Active Messages Hi Word", "Unsigned integer", False, 1.0, None, None),

    # 2.3.4.3 Current
    100: ParamSpec(100, "Iq Limit", "Current", True, 10.0, "A", None),
    101: ParamSpec(101, "Id Limit", "Current", True, 10.0, "A", None),
    107: ParamSpec(107, "Ia Offset EEPROM", "ADC Count", False, 1.0, None, None),
    108: ParamSpec(108, "Ib Offset EEPROM", "ADC Count", False, 1.0, None, None),
    109: ParamSpec(109, "Ic Offset EEPROM", "ADC Count", False, 1.0, None, None),

    # 2.3.4.4 Voltage & Flux
    102: ParamSpec(102, "DC Voltage Limit", "High Voltage", False, 10.0, "V", None),
    103: ParamSpec(103, "DC Voltage Hysteresis", "High Voltage", False, 10.0, "V", None),
    104: ParamSpec(104, "DC Under-voltage Limit", "High Voltage", False, 10.0, "V", None),
    106: ParamSpec(106, "Vehicle Flux Command", "Flux", True, 100.0, "Wb", None),

    # 2.3.4.5 Temperature
    112: ParamSpec(112, "Inverter Over-Temperature", "Temperature", True, 10.0, "degC", None),
    113: ParamSpec(113, "Motor Over-Temperature", "Temperature", True, 10.0, "degC", None),
    114: ParamSpec(114, "Zero Torque Temperature", "Temperature", True, 10.0, "degC", None),
    115: ParamSpec(115, "Full Torque Temperature", "Temperature", True, 10.0, "degC", None),
    203: ParamSpec(203, "RTD Selection", "Boolean", False, 1.0, None, None),  # bitfield

    # 2.3.4.6 Accelerator Pedal
    120: ParamSpec(120, "ACCEL Pedal Low", "Low Voltage", False, 100.0, "V", None),
    121: ParamSpec(121, "ACCEL Pedal Min", "Low Voltage", False, 100.0, "V", None),
    122: ParamSpec(122, "ACCEL Coast Low", "Low Voltage", False, 100.0, "V", None),
    123: ParamSpec(123, "ACCEL Coast High", "Low Voltage", False, 100.0, "V", None),
    124: ParamSpec(124, "ACCEL Pedal Max", "Low Voltage", False, 100.0, "V", None),
    125: ParamSpec(125, "ACCEL Pedal High", "Low Voltage", False, 100.0, "V", None),
    132: ParamSpec(132, "Accel Pedal Flipped", "Boolean", False, 1.0, None, None),

    # 2.3.4.7 Torque
    129: ParamSpec(129, "Motor Torque Limit", "Torque", True, 10.0, "Nm", None),
    130: ParamSpec(130, "REGEN Torque Limit", "Torque", True, 10.0, "Nm", None),
    131: ParamSpec(131, "Braking Torque Limit", "Torque", True, 10.0, "Nm", None),
    164: ParamSpec(164, "Kp Torque", "Proportional Gain", False, 10000.0, None, None),
    165: ParamSpec(165, "Ki Torque", "Integral Gain", False, 10000.0, None, None),
    166: ParamSpec(166, "Kd Torque", "Derivative Gain", False, 100.0, None, None),
    167: ParamSpec(167, "Klp Torque", "Low-Pass Filter Gain", False, 10000.0, None, None),
    168: ParamSpec(168, "Torque Rate Limit", "Torque", False, 10.0, "Nm", None),

    # 2.3.4.8 Speed
    111: ParamSpec(111, "Motor Over-speed", "Angular velocity", False, 1.0, "rpm", None),
    126: ParamSpec(126, "REGEN Fade Speed", "Angular velocity", False, 1.0, "rpm", None),
    127: ParamSpec(127, "Break Speed", "Angular velocity", False, 1.0, "rpm", None),
    128: ParamSpec(128, "Max Speed", "Angular velocity", False, 1.0, "rpm", None),
    160: ParamSpec(160, "Kp Speed", "Proportional Gain", False, 1.0, None, None),
    161: ParamSpec(161, "Ki Speed", "Integral Gain", False, 1.0, None, None),
    162: ParamSpec(162, "Kd Speed", "Derivative Gain", False, 1.0, None, None),
    163: ParamSpec(163, "Klp Speed", "Low-Pass Filter Gain", False, 1.0, None, None),
    169: ParamSpec(169, "Speed Rate Limit", "Speed", False, 1.0, "rpm", None),

    # 2.3.4.9 Shudder Compensation
    187: ParamSpec(187, "Shudder Compensation Enable", "Boolean", False, 1.0, None, {0: "Disabled", 1: "Enabled"}),
    188: ParamSpec(188, "Kp Shudder", "Counts x 100", False, 100.0, None, None),
    189: ParamSpec(189, "TCLAMP Shudder", "Torque", True, 10.0, "Nm", None),
    190: ParamSpec(190, "Shudder Filter Frequency", "Frequency", False, 10.0, "Hz", None),
    191: ParamSpec(191, "Shudder Speed Fade", "Angular velocity", False, 1.0, "rpm", None),
    192: ParamSpec(192, "Shudder Speed Low", "Angular velocity", False, 1.0, "rpm", None),
    193: ParamSpec(193, "Shudder Speed High", "Angular velocity", False, 1.0, "rpm", None),

    # 2.3.4.10 Brake Pedal
    180: ParamSpec(180, "Brake Mode", "Boolean", False, 1.0, None, {0: "Brake switch mode", 1: "Brake pot mode"}),
    181: ParamSpec(181, "Brake Low", "Low Voltage", False, 100.0, "V", None),
    182: ParamSpec(182, "Brake Min", "Low Voltage", False, 100.0, "V", None),
    183: ParamSpec(183, "Brake Max", "Low Voltage", False, 100.0, "V", None),
    184: ParamSpec(184, "Brake High", "Low Voltage", False, 100.0, "V", None),
    185: ParamSpec(185, "REGEN Ramp Period", "(Counts x 0.001) sec", False, 1.0, "ms", None),
    186: ParamSpec(186, "Brake Pedal Flipped", "Boolean", False, 1.0, None, None),
    199: ParamSpec(199, "Brake Input Bypassed", "Boolean", False, 1.0, None, None),
}


# ============================================================
# Name lookup
# ============================================================

def find_by_name(name_query: str) -> List[ParamSpec]:
    q = name_query.strip().lower()
    scored: List[Tuple[int, ParamSpec]] = []
    for p in PARAMS.values():
        n = p.name.lower()
        if q == n:
            scored.append((1000, p))
        elif q in n:
            scored.append((500 + len(q), p))
        else:
            toks = [t for t in re.split(r"\W+", q) if t]
            if toks and all(t in n for t in toks):
                scored.append((100 + len(toks), p))
    scored.sort(key=lambda x: (-x[0], x[1].addr))
    return [p for _, p in scored]


# ============================================================
# Low-level read/write helpers (used by YAML commands + CLI)
# ============================================================

def flush_bus_rx(bus: can.BusABC, max_frames: int = 200) -> None:
    # Drain already-queued frames quickly (non-blocking)
    for _ in range(max_frames):
        msg = bus.recv(timeout=0.0)
        if msg is None:
            break


def set_resp_filter(bus: can.BusABC, arbid_resp: int) -> None:
    # Keep RX load low by only receiving responses for the current offset.
    bus.set_filters([{"can_id": arbid_resp, "can_mask": 0x7FF, "extended": False}])


def read_param(
    bus: can.BusABC,
    arbid_cmd: int,
    arbid_resp: int,
    spec: ParamSpec,
    timeout: float,
    retries: int,
    retry_backoff: float,
) -> Tuple[int, Any]:
    def attempt_once() -> Tuple[int, Any]:
        flush_bus_rx(bus)
        bus.send(build_param_cmd(arbid_cmd, spec.addr, is_write=False, signed=spec.signed))
        resp = recv_param_response(bus, arbid_resp, spec.addr, timeout)
        if resp is None:
            raise TimeoutError(f"timeout waiting for response addr={spec.addr}")
        _, ok, payload = resp
        if not ok:
            raise RuntimeError(f"controller reported read failure addr={spec.addr}")
        raw = decode_i16_le(payload, signed=spec.signed)
        eng = spec.raw_to_eng(raw)
        return raw, eng

    return _with_retries(
        op_name=f"read addr={spec.addr}",
        retries=retries,
        backoff_s=retry_backoff,
        fn=attempt_once,
    )


def write_param(
    bus: can.BusABC,
    arbid_cmd: int,
    arbid_resp: int,
    spec: ParamSpec,
    raw_value: int,
    timeout: float,
    retries: int,
    retry_backoff: float,
) -> None:
    def attempt_once() -> None:
        flush_bus_rx(bus)
        bus.send(build_param_cmd(arbid_cmd, spec.addr, is_write=True, raw_value=raw_value, signed=spec.signed))
        resp = recv_param_response(bus, arbid_resp, spec.addr, timeout)
        if resp is None:
            raise TimeoutError(f"timeout waiting for write response addr={spec.addr}")
        _, ok, _ = resp
        if not ok:
            raise RuntimeError(f"controller reported write failure addr={spec.addr}")

    _with_retries(
        op_name=f"write addr={spec.addr}",
        retries=retries,
        backoff_s=retry_backoff,
        fn=attempt_once,
    )


def verify_param(
    bus: can.BusABC,
    arbid_cmd: int,
    arbid_resp: int,
    spec: ParamSpec,
    expected_raw: int,
    timeout: float,
    retries: int,
    retry_backoff: float,
) -> None:
    raw, _ = read_param(bus, arbid_cmd, arbid_resp, spec, timeout, retries, retry_backoff)
    if raw != expected_raw:
        raise RuntimeError(f"verify mismatch addr={spec.addr}: wrote raw={expected_raw} read raw={raw}")


def coerce_yaml_value_to_raw(spec: ParamSpec, value: Any) -> int:
    # YAML may store:
    # - enums: string label (preferred) or int raw
    # - boolean: true/false
    # - numeric: int/float
    if spec.enum_map is not None:
        if isinstance(value, int):
            return value
        if isinstance(value, str):
            inv = {v.lower(): k for k, v in spec.enum_map.items()}
            key = value.strip().lower()
            if key not in inv:
                raise ValueError(f"Unknown enum label '{value}' for addr={spec.addr}")
            return inv[key]
        raise ValueError(f"Enum value must be int or str for addr={spec.addr}")

    if spec.fmt.lower() == "boolean":
        if isinstance(value, bool):
            return 1 if value else 0
        if isinstance(value, (int, float)):
            return 1 if bool(value) else 0
        if isinstance(value, str):
            t = value.strip().lower()
            if t in ("true", "yes", "on", "1"):
                return 1
            if t in ("false", "no", "off", "0"):
                return 0
        raise ValueError(f"Boolean value not understood for addr={spec.addr}: {value!r}")

    if isinstance(value, (int, float)):
        return spec.eng_to_raw(value)

    raise ValueError(f"Value type not supported for addr={spec.addr}: {type(value)}")


# ============================================================
# CLI commands
# ============================================================

def cmd_list(args) -> int:
    specs = list(PARAMS.values())
    if args.eeprom_only:
        specs = [s for s in specs if s.is_eeprom()]
    specs.sort(key=lambda s: s.addr)

    for s in specs:
        unit = f" {s.unit}" if s.unit else ""
        enum = ""
        if s.enum_map:
            enum = " enum=" + ", ".join(f"{k}:{v}" for k, v in sorted(s.enum_map.items()))
        print(f"{s.addr:>4}  {s.name}  [{s.fmt}]  signed={s.signed} scale={s.scale}{unit}{enum}")
    return 0


def cmd_read(args, bus: can.BusABC, arbid_cmd: int, arbid_resp: int) -> int:
    if args.addr is None:
        matches = find_by_name(args.name)
        if not matches:
            print(f"ERROR: no match for name '{args.name}'", file=sys.stderr)
            return 2
        spec = matches[0]
    else:
        spec = PARAMS.get(args.addr)
        if spec is None:
            print(f"ERROR: address {args.addr} not in embedded registry", file=sys.stderr)
            return 2

    try:
        raw, eng = read_param(bus, arbid_cmd, arbid_resp, spec, args.timeout, args.retries, args.retry_backoff)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 3

    if spec.enum_map:
        print(f"READ {spec.addr} {spec.name}: raw={raw} -> {eng}")
    elif spec.fmt.lower() == "boolean":
        print(f"READ {spec.addr} {spec.name}: {bool(raw)}")
    else:
        unit = f" {spec.unit}" if spec.unit else ""
        print(f"READ {spec.addr} {spec.name}: raw={raw} -> {eng}{unit} (scale={spec.scale})")

    return 0


def cmd_write(args, bus: can.BusABC, arbid_cmd: int, arbid_resp: int) -> int:
    spec = PARAMS.get(args.addr)
    if spec is None:
        print(f"ERROR: address {args.addr} not in embedded registry", file=sys.stderr)
        return 2

    # Determine raw_value from --enum or --value
    if args.enum is not None:
        if spec.enum_map is None:
            print(f"ERROR: addr={spec.addr} is not enum-like; use --value", file=sys.stderr)
            return 2
        label = args.enum.strip().lower()
        inv = {v.lower(): k for k, v in spec.enum_map.items()}
        if label not in inv:
            print("ERROR: enum label not recognized. Known:", file=sys.stderr)
            for k, v in sorted(spec.enum_map.items()):
                print(f"  {k} = {v}", file=sys.stderr)
            return 2
        raw_value = inv[label]
    else:
        if args.value is None:
            print("ERROR: must provide --value or --enum", file=sys.stderr)
            return 2
        raw_value = spec.eng_to_raw(args.value)

    # Send write
    try:
        write_param(bus, arbid_cmd, arbid_resp, spec, raw_value, args.timeout, args.retries, args.retry_backoff)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 4

    # Print write summary in engineering units
    eng_written = spec.raw_to_eng(raw_value)
    if spec.enum_map:
        print(f"WRITE {spec.addr} {spec.name}: raw={raw_value} ({eng_written}) -> success")
    elif spec.fmt.lower() == "boolean":
        print(f"WRITE {spec.addr} {spec.name}: {bool(raw_value)} -> success")
    else:
        unit = f" {spec.unit}" if spec.unit else ""
        print(f"WRITE {spec.addr} {spec.name}: raw={raw_value} ({eng_written}{unit}) -> success")

    # Verify
    try:
        verify_param(bus, arbid_cmd, arbid_resp, spec, raw_value, args.timeout, args.retries, args.retry_backoff)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 6
    print("VERIFY OK")

    return 0


# ============================================================
# YAML dump / flash
# ============================================================

def iter_specs(eeprom_only: bool) -> List[ParamSpec]:
    specs = list(PARAMS.values())
    if eeprom_only:
        specs = [s for s in specs if s.is_eeprom()]
    specs.sort(key=lambda s: s.addr)
    return specs


def cmd_dump_yaml(args, bus: can.BusABC, arbid_cmd: int, arbid_resp: int) -> int:
    #   id_offset: '0x...'
    #   params:
    #     10: {name: Flux, value: 0.26}
    out: Dict[str, Any] = {
        "id_offset": hex(args.id_offset),
        "params": {},
    }

    failures = 0
    set_resp_filter(bus, arbid_resp)

    for spec in iter_specs(args.eeprom_only):
        try:
            _, eng = read_param(bus, arbid_cmd, arbid_resp, spec, args.timeout, args.retries, args.retry_backoff)
            out["params"][int(spec.addr)] = {"name": spec.name, "value": eng}
        except Exception:
            failures += 1

        time.sleep(0.01)

    with open(args.out, "w", encoding="utf-8") as f:
        yaml.safe_dump(out, f, sort_keys=False)

    if failures:
        print(f"Wrote {args.out} with {failures} read failures.", file=sys.stderr)
        return 7
    print(f"Wrote {args.out}")
    return 0


YamlParamBlock = Union[Dict[str, Any], List[Any]]


def _extract_params_block(doc: Dict[str, Any]) -> Dict[int, Dict[str, Any]]:
    """
    Accept either:
      params: { 100: {...}, 101: {...} }
    or
      params:
        - addr: 100
          value: ...
    """
    params: YamlParamBlock = doc.get("params", {})
    if isinstance(params, dict):
        out: Dict[int, Dict[str, Any]] = {}
        for k, v in params.items():
            try:
                addr = int(k)
            except Exception:
                addr = int(v.get("addr"))
            if not isinstance(v, dict):
                raise ValueError(f"params[{k}] must be a dict")
            out[addr] = v
        return out

    if isinstance(params, list):
        out2: Dict[int, Dict[str, Any]] = {}
        for item in params:
            if not isinstance(item, dict) or "addr" not in item:
                raise ValueError("params list items must be dicts with 'addr'")
            out2[int(item["addr"])] = item
        return out2

    raise ValueError("YAML 'params' must be a mapping or a list")


def _yaml_id_offset_or_default(doc: Dict[str, Any], default_offset: int) -> int:
    if "id_offset" not in doc or doc["id_offset"] is None:
        return default_offset
    v = doc["id_offset"]
    if isinstance(v, int):
        return int(v)
    if isinstance(v, str):
        return parse_hex_int(v)
    raise ValueError(f"Unsupported id_offset type in YAML: {type(v)}")


def cmd_flash_yaml(args, bus: can.BusABC, arbid_cmd: int, arbid_resp: int) -> int:
    with open(args.input, "r", encoding="utf-8") as f:
        doc = yaml.safe_load(f) or {}

    # If YAML contains id_offset, use it (file is self-contained).
    try:
        yaml_offset = _yaml_id_offset_or_default(doc, args.id_offset)
    except Exception as e:
        print(f"ERROR: invalid id_offset in YAML: {e}", file=sys.stderr)
        return 20

    if yaml_offset != args.id_offset:
        print(f"NOTE: using YAML id_offset={hex(yaml_offset)} (CLI was {hex(args.id_offset)})", file=sys.stderr)

    arbid_cmd = yaml_offset + DELTA_PARAM_CMD
    arbid_resp = yaml_offset + DELTA_PARAM_RESP
    set_resp_filter(bus, arbid_resp)

    try:
        param_entries = _extract_params_block(doc)
    except Exception as e:
        print(f"ERROR: invalid YAML format: {e}", file=sys.stderr)
        return 21

    addrs = sorted(param_entries.keys())
    failures = 0

    for addr in addrs:
        entry = param_entries[addr]
        spec = PARAMS.get(addr)

        if spec is None:
            msg = f"addr={addr} not in embedded registry"
            if args.skip_unknown:
                print(f"SKIP {msg}")
                continue
            print(f"ERROR: {msg} (use --skip-unknown to ignore)", file=sys.stderr)
            failures += 1
            if not args.continue_on_error:
                return 22
            continue

        # Prefer "value" if present; else allow "raw"
        has_value = "value" in entry
        has_raw = "raw" in entry
        if not has_value and not has_raw:
            print(f"ERROR: addr={addr} has neither 'value' nor 'raw' in YAML", file=sys.stderr)
            failures += 1
            if not args.continue_on_error:
                return 23
            continue

        try:
            if has_raw and not has_value:
                raw_value = int(entry["raw"])
            else:
                raw_value = coerce_yaml_value_to_raw(spec, entry["value"])
        except Exception as e:
            print(f"ERROR: addr={addr} ({spec.name}) bad value: {e}", file=sys.stderr)
            failures += 1
            if not args.continue_on_error:
                return 24
            continue

        eng_preview = spec.raw_to_eng(raw_value)

        if args.dry_run:
            print(f"DRY-RUN WRITE {addr} {spec.name}: raw={raw_value} ({eng_preview})")
            continue

        try:
            write_param(bus, arbid_cmd, arbid_resp, spec, raw_value, args.timeout, args.retries, args.retry_backoff)
            verify_param(bus, arbid_cmd, arbid_resp, spec, raw_value, args.timeout, args.retries, args.retry_backoff)
            print(f"WRITE {addr} {spec.name}: raw={raw_value} ({eng_preview}) -> success")
            print("  VERIFY OK")
        except Exception as e:
            print(f"ERROR: addr={addr} ({spec.name}) write/verify failed: {e}", file=sys.stderr)
            failures += 1
            if not args.continue_on_error:
                return 25

        # Small pacing delay to avoid flooding some controllers
        time.sleep(0.10)

    if failures:
        print(f"Flash completed with {failures} failures.", file=sys.stderr)
        return 26

    print("Flash completed successfully.")
    return 0


def parse_hex_int(s: str) -> int:
    s = s.strip().lower()
    return int(s, 16) if s.startswith("0x") else int(s, 10)


def main() -> int:
    ap = argparse.ArgumentParser(description="EEPROM param read/write tool (2-byte data, manual units, no DBC)")
    ap.add_argument("--channel", default="can0", help="CAN channel (e.g., can0)")
    ap.add_argument("--interface", default="socketcan", help="python-can interface (default: socketcan)")
    ap.add_argument("--bitrate", type=int, default=None, help="Optional bitrate (backend-dependent)")
    ap.add_argument("--timeout", type=float, default=0.6, help="Response timeout seconds")
    ap.add_argument("--retries", type=int, default=3,
                    help="Retries on timeout for each CAN transaction (default: 3)")
    ap.add_argument("--retry-backoff", type=float, default=0.05,
                    help="Seconds base backoff between retries (default: 0.05)")
    ap.add_argument(
        "--id-offset",
        type=parse_hex_int,
        default=0x0A0,
        help="Controller CAN ID Offset/base. Default 0x0A0 -> IDs 0x0C1/0x0C2. Accepts 0x.. or decimal.",
    )

    sub = ap.add_subparsers(dest="cmd", required=True)

    ls = sub.add_parser("list", help="List embedded parameters")
    ls.add_argument("--eeprom-only", action="store_true", help="Only show EEPROM range (100–499)")

    rd = sub.add_parser("read", help="Read a parameter (engineering units out)")
    g = rd.add_mutually_exclusive_group(required=True)
    g.add_argument("--addr", type=int, help="Parameter address")
    g.add_argument("--name", type=str, help="Parameter name (fuzzy match)")

    wr = sub.add_parser("write", help="Write a parameter (engineering units in)")
    wr.add_argument("--addr", type=int, required=True, help="Parameter address")
    wr.add_argument("--value", type=float, help="Engineering value (tool applies scaling)")
    wr.add_argument("--enum", type=str, help="Enum label (if supported)")

    dy = sub.add_parser("dump", help="Read many parameters and write a YAML snapshot")
    dy.add_argument("--out", required=True, help="Output YAML path")
    dy.add_argument("--eeprom-only", action="store_true", help="Only dump EEPROM range (100–499)")

    fy = sub.add_parser("flash", help="Write parameters from a YAML file")
    fy.add_argument("--in", dest="input", required=True, help="Input YAML path")
    fy.add_argument("--dry-run", action="store_true", help="Print actions without writing")
    fy.add_argument("--continue-on-error", action="store_true", help="Attempt remaining params after an error")
    fy.add_argument("--skip-unknown", action="store_true", help="Skip YAML params not in the embedded registry")

    args = ap.parse_args()

    arbid_cmd = args.id_offset + DELTA_PARAM_CMD
    arbid_resp = args.id_offset + DELTA_PARAM_RESP

    if args.cmd == "list":
        return cmd_list(args)

    bus_kwargs = dict(interface=args.interface, channel=args.channel)
    if args.bitrate is not None:
        bus_kwargs["bitrate"] = args.bitrate

    try:
        with can.Bus(**bus_kwargs) as bus:
            if args.cmd == "flash":
                # cmd_flash_yaml sets filter after reading YAML id_offset
                return cmd_flash_yaml(args, bus, arbid_cmd, arbid_resp)

            set_resp_filter(bus, arbid_resp)
            if args.cmd == "read":
                return cmd_read(args, bus, arbid_cmd, arbid_resp)
            if args.cmd == "write":
                return cmd_write(args, bus, arbid_cmd, arbid_resp)
            if args.cmd == "dump":
                return cmd_dump_yaml(args, bus, arbid_cmd, arbid_resp)
            return 1
    except can.CanError as e:
        print(f"CAN error: {e}", file=sys.stderr)
        return 10


if __name__ == "__main__":
    raise SystemExit(main())
