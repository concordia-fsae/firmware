from enum import Enum


class Continuous(Enum):
    discrete = 0
    continuous = 1


class DiscreteValue:
    """DiscreteValue type used to create all the discrete values from discrete-values.yaml"""
    def __init__(self, name: str, values: dict):
        self.name = name
        self.values = values
        self.bit_width = 0
        self.max_val = 0

    def __repr__(self):
        return (
            f"Name: {self.name}\n"
            f"Values: {self.values}"
        )

    def repr_for_dbc(self):
        return " ".join([f"{val} \"{name}\"" for name, val in self.values.items()])


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
    value = 2


class Units(Enum):
    none = ""
    m = "m"
    cm = "cm"
    s = "s"
    ms = "ms"
    RPM = "RPM"
