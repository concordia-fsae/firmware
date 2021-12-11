from math import ceil, log

from .Types import *


def get_if_exists(src: dict, key: str, conversion_type: type, default, **kwargs):
    """
    Get a key from the source and convert to the desired type
    if the key exists in the source, otherwise return the default
    """
    if "extra_params" in kwargs:
        return (
            conversion_type(src[key], kwargs["extra_params"]) if key in src else default
        )
    return conversion_type(src[key]) if key in src else default


class CanObject:
    """
    CAN Object base class
    Contains common methods and data to be inherited by other CAN classes
    """

    def __init__(self):
        pass


class DiscreteValues:
    """
    Empty class in which all the discrete value tables defined in
    discrete_values.yaml will be stored
    """

    def __repr__(self):
        return str(dict(self))

    def __iter__(self):
        for attr, value in self.__dict__.items():
            yield attr, value

    def update(self, other):
        if other is not None:
            if isinstance(other, DiscreteValue):
                if other.name not in self.__dict__.keys():
                    setattr(self, other.name, other)
            elif isinstance(other, DiscreteValues):
                for attr, value in other:
                    if attr not in self.__dict__.keys():
                        setattr(self, attr, value)


discrete_values = DiscreteValues()


class NativeRepresentation:
    """
    Takes in a dictionary parsed from YAML
    """

    def __init__(self, signal_def=None):
        signal_def = {} if signal_def is None else signal_def
        self.bit_width = get_if_exists(signal_def, "bitWidth", int, None)
        self.range = get_if_exists(signal_def, "range", Range, None)
        self.signedness = get_if_exists(
            signal_def, "signedness", Signedness, Signedness.unsigned
        )
        self.endianness = get_if_exists(
            signal_def, "endianness", Endianess, Endianess.little
        )
        self.resolution = get_if_exists(signal_def, "resolution", float, 1.0)


class SnaParams:
    """Parameters describing SNA behavior for a signal"""

    def __init__(self, sna_params=None, signal_name=""):
        sna_params = {} if sna_params is None else sna_params
        if sna_params:
            self.signal_name = signal_name
            self.sna_type = get_if_exists(sna_params, "type", SnaType, SnaType.auto)
            self.value = (
                get_if_exists(sna_params, "value", int, None)
                if self.sna_type == SnaType.auto
                else None
            )

            if self.sna_type == SnaType.auto and "value" in sna_params:
                print(
                    f"Signal {self.signal_name} has an SNA value defined "
                    "when the SNA type is 'auto'"
                )
        else:
            self.sna_type = SnaType.auto
            self.value = None


class CanSignal(CanObject):
    """
    CanSignal class
    Defines a CAN signal to be placed into a CAN message
    """

    def __init__(
        self,
        name: str,
        signal_def: dict,
    ):
        self.name = name
        self.continuous = Continuous(
            get_if_exists(signal_def, "continuous", bool, Continuous.discrete)
        )
        self.cycle_time_ms = get_if_exists(signal_def, "cycleTimeMs", int, None)
        self.description = get_if_exists(signal_def, "description", str, None)
        self.discrete_values = getattr(discrete_values, get_if_exists(signal_def, "discreteValues", str, ""), None)
        self.native_representation = get_if_exists(
            signal_def,
            "nativeRepresentation",
            NativeRepresentation,
            NativeRepresentation(),
        )
        self.sna_params = get_if_exists(
            signal_def, "sna", SnaParams, SnaParams(), extra_params=self.name
        )
        try:
            self.unit = get_if_exists(signal_def, "unit", Units, Units.none)
        except Exception as e:
            raise Exception(
                f"Exception {e}: Unknown unit type for signal {self.name}"
            ) from e

        # these will be set when building the message
        self.message_ref = None
        self.start_bit = 0
        self.offset = 0
        self.scale = 1
        self.receivers = []

        # check validity
        self.is_valid = True
        self._check_valid()

        if self.is_valid:
            self.calc_length_bits()

    def __repr__(self):
        return (
            f"\nCAN Signal: {self.name}, "
            f"Description: {self.description}, "
            f"cycleTimeMs: {self.cycle_time_ms}, "
            f"bitWidth: {self.native_representation.bit_width}, "
            f"offset: {self.offset}, "
            f"scale: {self.scale}, "
            f"startBit: {self.start_bit}, "
            f"unit: {self.unit.value}"
        )

    def _check_valid(self):
        nat_rep = self.native_representation
        name = self.name

        if nat_rep is None and self.discrete_values is None:
            print(
                f"Signal {name} has neither a NativeRepresentation nor a "
                "discreteValues defined. One of these must be defined"
            )
            self.is_valid = False
        elif self.discrete_values == "":
            print(f"Signal {name} has a mistake in its units")
            self.is_valid = False
        elif not nat_rep is None:
            if self.continuous == Continuous.continuous:
                if self.unit is None:
                    print(f"Signal {name} is continuous but has no unit defined")
                    self.is_valid = False
                if nat_rep.range is None:
                    print(f"Signal {name} is continuous but has no range defined")
                    self.is_valid = False
                else:
                    if not nat_rep.range.is_valid:
                        # error for this printed from Range class
                        self.is_valid = False
            else:
                self.unit = Units.none

            nat_rep_invalid = nat_rep.bit_width is None and (
                nat_rep.range is None or not nat_rep.range.is_valid
            )

            if nat_rep_invalid and self.discrete_values is None:
                print(
                    f"Signal '{name}' does not have a range or "
                    "discreteValues defined. Please add one of these"
                )
                self.is_valid = False

            if nat_rep.bit_width and self.discrete_values:
                print(
                    f"Signal '{name}' has both bitWidth and discreteValues defined. "
                    "These are mutually exclusive"
                )
                self.is_valid = False

            if self.discrete_values and nat_rep.range:
                print(
                    f"Signal '{name}' has both a range and discreteValues defined. "
                    "These are mutually exclusive"
                )
                self.is_valid = False

    def calc_length_bits(self):
        """calculate the bit length of the signal from the native representation"""
        nat_rep = self.native_representation
        if (
            self.discrete_values is None
            and not nat_rep.range is None
            and nat_rep.range.is_valid
        ):
            sig_range = 0
            if nat_rep.range.min >= 0:
                self.offset = nat_rep.range.min
                nat_rep.signedness = Signedness.unsigned
            elif nat_rep.range.min < 0:
                self.offset = -nat_rep.range.min
                nat_rep.signedness = Signedness.signed
            sig_range = nat_rep.range.max - nat_rep.range.min

            if not nat_rep.resolution is None:
                self.scale = 1 / nat_rep.resolution
                nat_rep.bit_width = ceil(log(sig_range / nat_rep.resolution, 2))
            elif nat_rep.bit_width:
                self.scale = sig_range / (2 ** nat_rep.bit_width)
            else:
                print(f"Error when calculating scale for signal {self.name}")
        elif not self.discrete_values is None:
            nat_rep.bit_width = self.discrete_values.bit_width
            nat_rep.range = Range({"min": 0, "max": self.discrete_values.max_val})
        else:
            print(f"Signal {self.name} has an error with discreteValues or range")
            self.is_valid = False


class CanMessage(CanObject):
    """CAN Message Class"""

    def __init__(self, name: str, msg_def: dict):
        self.name = name
        self.cycle_time_ms = 0
        self.description = get_if_exists(msg_def, "description", str, "")
        self.node_name = self.name.split("_")[0]
        self.id = get_if_exists(msg_def, "id", int, None)
        self.length_bytes = get_if_exists(msg_def, "lengthBytes", int, None)
        self.signals = {
            f"{self.node_name}_{sig_name}": sig
            for sig_name, sig in get_if_exists(msg_def, "signals", dict, {}).items()
        }
        self.signal_objs = {}
        self.receivers = []
        self.discrete_values = DiscreteValues()

        self.is_valid = True

    def __repr__(self):
        return f"id: {self.id}, Signals: {list(self.signal_objs.values())}"

    def add_signal(self, msg_signal: dict, signal_obj: CanSignal):
        """Add the given signal to this message"""
        signal_obj.start_bit = msg_signal["startBit"] if not msg_signal is None else 0
        self.signal_objs[signal_obj.name] = signal_obj
        self.discrete_values.update(signal_obj.discrete_values)

    def validate_msg(self):
        """Validate that this message meets all requirements"""
        if self.id is None:
            print(f"Message '{self.name}' is missing an id")
            self.is_valid = False

        # this works because dictionaries are ordered now
        first_signal_loc = list(self.signal_objs.values())[0].start_bit
        bit_count = first_signal_loc
        self.cycle_time_ms = list(self.signal_objs.values())[0].cycle_time_ms

        first = True
        for signal in self.signal_objs.values():
            self.cycle_time_ms = min(self.cycle_time_ms, signal.cycle_time_ms)
            if first:
                signal.start_bit = bit_count
                bit_count += signal.native_representation.bit_width
                first = False
            else:
                signal.start_bit = (
                    bit_count if signal.start_bit == 0 else signal.start_bit
                )
                bit_count = signal.start_bit + signal.native_representation.bit_width

        if bit_count >= 64:
            print(f"Message {self.name} has length greater than 64 bits!")
            self.is_valid = False
            return

        if self.length_bytes is None:
            print(f"Message {self.name} is missing a lengthBytes")
            self.is_valid = False
        else:
            if ceil(bit_count / 8) > self.length_bytes:
                print(
                    f"Message {self.name} has lengthBytes greater than the sum "
                    "of the bitWidths of each of its signals"
                )

    def add_receiver(self, node, signal):
        self.receivers.append(node)
        self.signal_objs[signal].receivers.append(node)


class CanNode(CanObject):
    """Class defining a CAN node (i.e. ecu)"""

    def __init__(self, name, node_def):
        self.name = name
        self.def_files = node_def["def_files"]
        self.description = get_if_exists(node_def, "description", str, "")
        self.messages = {}
        self.signals = {}
        self.processed = False
        self.on_buses = {}
        self.received_msgs = {}
        self.received_sigs = {}
        self.discrete_values = DiscreteValues()

        self.is_valid = True

    def __repr__(self):
        return f"CAN Node Definition:\n\
                Name: {self.name}\n\
                Messages: {self.messages}\n"

    def add_message(self, message: CanMessage):
        """Add a message to this node"""
        self.messages[message.name] = message
        if message.signals is None:
            print(
                f"Tried to add a message with no signals ({message.name})"
                f"to node {self.name}"
            )
            self.is_valid = False
        else:
            self.signals.update(message.signal_objs)
            self.discrete_values.update(message.discrete_values)


class CanBus(CanObject):
    """Class defining a physical CAN Bus"""

    def __init__(self, bus_def):
        self.name = get_if_exists(bus_def, "name", str, "")
        self.baudrate = get_if_exists(bus_def, "baudrate", int, 500000)
        self.description = get_if_exists(bus_def, "description", str, "")
        self.default_endianess = Endianess[
            get_if_exists(bus_def, "defaultEndianess", str, "str")
        ]
        self.nodes = {}
        self.messages = {}
        self.signals = {}
        self.discrete_values = DiscreteValues()

        self.is_valid = True
        self._check_valid()

    def __repr__(self):
        return f"CAN Bus Definition:\n\
                Name: {self.name}\n\
                Description: {self.description}\n\
                Nodes: {self.nodes}\n"

    def _check_valid(self):
        if self.name == "":
            print("A bus is missing a name! Check bus definition files")
            self.is_valid = False

        if self.description == "":
            if self.is_valid:
                print(f"Bus '{self.name}' is missing a description")
            else:
                print("A bus is missing a name! Check bus definition files")

    def add_node(self, node: CanNode):
        """Add a node to this CAN bus"""
        self.nodes[node.name] = node
        self.messages.update(node.messages)
        self.discrete_values.update(node.discrete_values)
        self.signals.update(node.signals)
