from enum import Enum
from typing import Dict, Literal, Union


class Continuous(Enum):
    discrete = 0
    continuous = 1


class DiscreteValue:
    """DiscreteValue type used to create all the discrete values from discrete-values.yaml"""

    def __init__(self, name: str, values: Dict[str, int]):
        if not isinstance(name, str):
            raise ValueError(f"Discrete value name {name} is not acceptable")

        if (
            not isinstance(values, dict)
            or any(not isinstance(n, str) for n in values.keys())
            or any(not isinstance(v, int) for v in values.values())
        ):
            raise ValueError(f"Values for discrete value {name} must all be ints")

        self.name = name
        self.values = values
        self.min_val = min(values.values())
        self.max_val = max(values.values())
        self.bit_width = self.max_val.bit_length()

    def __repr__(self):
        return f"Name: {self.name}\n" f"Values: {self.values}"

    def repr_for_dbc(self):
        return " ".join([f'{val} "{name}"' for name, val in self.values.items()])


class Endianess(Enum):
    big = 0
    little = 1


class Range:
    def __init__(self, vals):
        if (not "min" in vals) or (not "max" in vals):
            self.min = 0
            self.max = 0
            self.delta = 0
            self.is_valid = False
        else:
            self.min = vals["min"]
            self.max = vals["max"]
            self.is_valid = True

            self._check_valid()

            self.delta = self.max - self.min

    def __repr__(self):
        return f"Range object with Min: {self.min} and Max: {self.max}"

    def _check_valid(self):
        if not (
            isinstance(self.min, (float, int))
            and isinstance(self.max, (float, int))
            and (self.min < self.max)
        ):
            print("min or max not int/float or min not less than max")
            self.is_valid = False


class Signedness(Enum):
    unsigned = "+"
    signed = "-"


class SnaType(Enum):
    auto = 0
    max = 1
    min = 2
    custom = 3


# TODO: move this to a yaml
class Units(Enum):
    none = ""
    m = "m"
    cm = "cm"
    s = "s"
    ms = "ms"
    volts = "V"
    degC = "degC"
    amps = "A"
    amp_hours = "Ah"
    rpm = "RPM"
    pct = "%"
    mm = "mm"
    Nm = "Nm"
    N = "N"
    deg = "deg"
    Hz = "Hz"
    Wb = "Wb"
    psi = "PSI"
    mps2 = "m/s2"
    mps = "m/s"
    bytes = "B"
    Whr = "Whr"
    kW = "kW"
    km = "km"
    dps = "deg/s"
    kOhm = "kOhm"


class ValidationRole(Enum):
    none = "none"
    counter = "counter"
    checksum = "checksum"


class CType(Enum):
    _bool = "bool"
    uint8_t = "uint8_t"
    int8_t = "int8_t"
    uint16_t = "uint16_t"
    int16_t = "int16_t"
    uint32_t = "uint32_t"
    int32_t = "int32_t"
    uint64_t = "uint64_t"
    int64_t = "int64_t"
    float32_t = "float32_t"
    float64_t = "float64_t"

    @classmethod
    def from_val(cls, bit_width: int, signed: bool, flt: bool):
        if bit_width == 1:
            return CType._bool

        req_width_bytes = (bit_width + 7) // 8
        req_width_bytes = 4 if req_width_bytes == 3 else 8 if req_width_bytes > 4 else req_width_bytes

        if req_width_bytes not in _DATA_SIZE_MAPPING[flt][signed]:
            raise KeyError("Error in calculation of required C type")

        return _DATA_SIZE_MAPPING[flt][signed][req_width_bytes]


_DATA_SIZE_MAPPING = {
    False: {
        False: {
            1: CType.uint8_t,
            2: CType.uint16_t,
            4: CType.uint32_t,
            8: CType.uint64_t,
        },
        True: {
            1: CType.int8_t,
            2: CType.int16_t,
            4: CType.int32_t,
            8: CType.int64_t,
        },
    },
    True: {
        True: {
            1: CType.float32_t,
            2: CType.float32_t,
            4: CType.float32_t,
            8: CType.float64_t,
        },
        False: {
            1: CType.float32_t,
            2: CType.float32_t,
            4: CType.float32_t,
            8: CType.float64_t,
        },
    },
}
