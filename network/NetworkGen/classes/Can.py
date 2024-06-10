from math import ceil, log
from typing import List, Optional

from .Types import *


def get_if_exists(src: dict, key: str, conversion_type: type, default, **kwargs):
    """
    Get a key from the source and convert to the desired type
    if the key exists in the source, otherwise return the default
    """
    if key not in src:
        return default

    if "extra_params" in kwargs:
        return conversion_type(src[key], kwargs["extra_params"])
    return conversion_type(src[key])


class CanObject:
    """
    CAN Object base class
    Contains common methods and data to be inherited by other CAN classes
    """

    def __init__(self):
        pass


class DiscreteValues:
    """
    Singleton class in which all the discrete value tables defined in
    discrete_values.yaml will be stored
    """

    _instance = None

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = super(DiscreteValues, cls).__new__(cls, *args, **kwargs)
        return cls._instance

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


class NativeRepresentation:
    """
    Takes in a dictionary parsed from YAML
    """

    def __init__(self, signal_def=dict()):
        self.bit_width = get_if_exists(signal_def, "bitWidth", int, None)
        self.range = get_if_exists(signal_def, "range", Range, None)
        self.signedness = get_if_exists(
            signal_def, "signedness", Signedness, Signedness.unsigned
        )
        self.endianness = get_if_exists(
            signal_def, "endianness", Endianess, Endianess.little
        )
        self.resolution = get_if_exists(signal_def, "resolution", float, None)


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

    DISC = DiscreteValues()

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
        self.discrete_values = getattr(
            self.DISC, get_if_exists(signal_def, "discreteValues", str, ""), None
        )
        self.native_representation = get_if_exists(
            signal_def,
            "nativeRepresentation",
            NativeRepresentation,
            None,
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

        self.validation_role = get_if_exists(
            signal_def, "validationRole", ValidationRole, ValidationRole.none
        )

        # these will be set when building the message
        self.message_ref = None
        self.start_bit = 0
        self.offset = 0
        self.scale = 1
        self.receivers = []

        # check validity
        self.is_valid = False
        self._check_valid()

        if self.is_valid:
            self.calc_length_bits()

        self._check_val_roles()

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
        valid = True
        nat_rep = self.native_representation
        dv = self.discrete_values
        name = self.name

        if not nat_rep and not dv:
            print(
                f"Signal {name} has neither a nativeRepresentation key nor a "
                "discreteValues key defined. One of these must be defined"
            )
            valid = False
        elif dv == "":
            print(f"Signal {name} has a mistake in its discreteValues")
            valid = False
        elif nat_rep:
            if self.continuous == Continuous.continuous:
                if self.unit is None:
                    print(f"Signal {name} is continuous but has no unit defined")
                    valid = False
                if nat_rep.range is None:
                    print(f"Signal {name} is continuous but has no range defined")
                    valid = False
                elif not nat_rep.range.is_valid:
                    # error for this printed from Range class
                    valid = False
            elif dv:
                self.unit = Units.none

            nat_rep_invalid = not nat_rep.bit_width and (
                not nat_rep.range or not nat_rep.range.is_valid
            )

            if nat_rep_invalid and not dv:
                print(
                    f"Signal '{name}' does not have a range or "
                    "discreteValues defined. Please add one of these"
                )
                valid = False

            if nat_rep.bit_width and dv:
                print(
                    f"Signal '{name}' has both bitWidth and discreteValues defined. "
                    "These are mutually exclusive"
                )
                valid = False

            if dv and nat_rep.range:
                print(
                    f"Signal '{name}' has both a range and discreteValues defined. "
                    "These are mutually exclusive"
                )
                valid = False

        self.is_valid = valid

    def _check_val_roles(self):
        if self.validation_role == ValidationRole.counter:
            assert (
                self.native_representation
            ), "Expected signal to have a nativeRepresentation by now, but it is still None"
            assert (
                self.native_representation.bit_width
            ), "Expected bit width to have been calculated by this point, but it is still None"
            if self.native_representation.bit_width > 8:
                print(
                    f"Signal '{self.name}' is marked as a counter signal, but its bit width exceeds 8. We currently only support 8 bit wide counter signals."
                )
                self.is_valid = False

    def calc_length_bits(self):
        """calculate the bit length of the signal from the native representation"""
        if self.native_representation and self.discrete_values:
            print(
                f"Signal '{self.name}' has both discreteValues and nativeRepresentation defined, when only one of the two should be used at a time."
            )
            self.is_valid = False
            return

        # handle case where discreteValues is provided
        if dv := self.discrete_values:
            self.native_representation = NativeRepresentation()
            nat_rep = self.native_representation
            nat_rep.bit_width = dv.bit_width
            nat_rep.range = Range({"min": dv.min_val, "max": dv.max_val})
            if not nat_rep.range.is_valid:
                print(
                    f"Signal '{self.name}' uses discrete value table '{dv}' which has an invalid range"
                )
                self.is_valid = False
            return

        # handle case where nativeRepresentation is provided
        if nat_rep := self.native_representation:
            if nat_rep.range:
                if not nat_rep.range.is_valid:
                    print(f"Signal '{self.name}' has an invalid range")
                    self.is_valid = False
                    return

                if nat_rep.range.min >= 0:
                    self.offset = nat_rep.range.min
                    nat_rep.signedness = Signedness.unsigned
                elif nat_rep.range.min < 0:
                    self.offset = -nat_rep.range.min
                    nat_rep.signedness = Signedness.signed
                sig_range = nat_rep.range.max - nat_rep.range.min

                if nat_rep.resolution:
                    self.scale = nat_rep.resolution
                else:
                    if nat_rep.bit_width:
                        self.scale = sig_range / (2**nat_rep.bit_width)
                    else:
                        assert sig_range, "Signal range was not defined somehow"
                        assert nat_rep.resolution, "Resolution was not defined somehow"
                        nat_rep.bit_width = ceil(log(sig_range / nat_rep.resolution, 2))
            elif nat_rep.bit_width:
                nat_rep.resolution = nat_rep.resolution or 1.0
                nat_rep.range = Range(
                    {"min": 0, "max": 2**nat_rep.bit_width * nat_rep.resolution}
                )
        else:
            nat_rep = NativeRepresentation()
            nat_rep.bit_width = 1
            nat_rep.resolution = 1
            nat_rep.range = Range({"min": 0, "max": 1})

    def get_name_nodeless(self):
        return "_".join(self.name.split("_")[1:])


class CanMessage(CanObject):
    """CAN Message Class"""

    DISC = DiscreteValues()

    def __init__(self, node: "CanNode", name: str, msg_def: dict):
        self.name = name
        self.cycle_time_ms = 0
        self.description = get_if_exists(msg_def, "description", str, "")
        self.node_name = self.name.split("_")[0]
        self.id = get_if_exists(msg_def, "id", int, None)
        self.length_bytes = get_if_exists(msg_def, "lengthBytes", int, None)
        self.signals = {
            f"{self.node_name}_{sig_name}": sig
            for sig_name, sig in msg_def["signals"].items()
        }
        self.signal_objs = {}
        self.counter_sig: Optional[CanSignal] = None
        self.checksum_sig: Optional[CanSignal] = None
        self.receivers = []
        self.source_buses: List[str]
        if source_buses := msg_def.get("sourceBuses"):
            if isinstance(source_buses, list):
                self.source_buses = source_buses
            elif isinstance(source_buses, str):
                self.source_buses = [source_buses]
            else:
                raise Exception(
                    f"source_buses should be a str or list of strs, got '{type(source_buses)}'"
                )
        else:
            self.source_buses = node.on_buses

        self.is_valid = False

    def __repr__(self):
        return f"id: {self.id}, Signals: {list(self.signal_objs.values())}"

    def add_signal(self, msg_signal: dict, signal_obj: CanSignal):
        """Add the given signal to this message"""
        signal_obj.start_bit = msg_signal["startBit"] if msg_signal else 0
        if signal_obj.validation_role == ValidationRole.checksum:
            self.checksum_sig = signal_obj
        if signal_obj.validation_role == ValidationRole.counter:
            self.counter_sig = signal_obj
        self.signal_objs[signal_obj.name] = signal_obj
        self.DISC.update(signal_obj.discrete_values)

    def validate_msg(self):
        """Validate that this message meets all requirements"""
        valid = True
        if self.id is None:
            print(f"Message '{self.name}' is missing an id")
            valid = False

        # this works because dictionaries are ordered now
        first_signal_loc = list(self.signal_objs.values())[0].start_bit
        bit_count = first_signal_loc
        self.cycle_time_ms = list(self.signal_objs.values())[0].cycle_time_ms

        sig_objs = self.signal_objs.values()
        first = True
        for signal in sig_objs:
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

        # validate validationRoles of signals
        roles = [ValidationRole.counter, ValidationRole.checksum]
        for role in roles:
            if sum([1 for sig in sig_objs if sig.validation_role == role]) > 1:
                print(
                    f"Message {self.name} contains multiple signals marked as having the validationRole '{role}'"
                )
                valid = False

        if self.checksum_sig and self.counter_sig:
            if self.checksum_sig == self.counter_sig:
                print(
                    f"Signal '{self.checksum_sig}' is defined as both a counter and checksum signal"
                )
                valid = False

        if bit_count > 64:
            print(f"Message {self.name} has length greater than 64 bits!")
            valid = False
            return

        if self.length_bytes is None:
            print(f"Message {self.name} is missing a lengthBytes")
            valid = False
        else:
            if ceil(bit_count / 8) > self.length_bytes:
                print(
                    f"Message {self.name} has lengthBytes greater than the sum "
                    "of the bitWidths of each of its signals"
                )
                valid = False

        self.is_valid = valid

    def add_receiver(self, node, signal):
        if node not in self.receivers:
            self.receivers.append(node)

        if signal not in self.signal_objs:
            print(
                f"Tried to add a receiver for signal '{signal}' in message '{self.name}', but couldn't find the signal to add it"
            )
            return

        if node in self.signal_objs[signal].receivers:
            print(
                f"Tried to add a receiver for signal '{signal}' in message '{self.name}' but it was already marked as a receiver"
            )
            return

        self.signal_objs[signal].receivers.append(node)

    def get_non_val_sigs(self) -> List[CanSignal]:
        return [
            sig
            for sig in self.signal_objs.values()
            if sig.validation_role == ValidationRole.none
        ]


class CanNode(CanObject):
    """Class defining a CAN node (i.e. ecu)"""

    def __init__(self, name, node_def):
        self.name: str = name
        self.def_files = node_def["def_files"]
        self.description: str = get_if_exists(node_def, "description", str, "")
        self.messages: Dict[str, CanMessage] = {}
        self.signals: Dict[str, CanSignal] = {}
        self.processed = False
        self.on_buses: List[str] = []
        self.received_msgs: Dict[str, CanMessage] = {}
        self.received_sigs: Dict[str, CanSignal] = {}

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

    def messages_by_cycle_time(self) -> Dict[int, List[CanMessage]]:
        ret = {}
        for msg in self.messages.values():
            if msg.cycle_time_ms in ret:
                ret[msg.cycle_time_ms].append(msg)
                continue
            ret.update({msg.cycle_time_ms: [msg]})
        for _, msgs in ret.items():
            msgs.sort(key=lambda entry: entry.id)
        return ret


class CanBus(CanObject):
    """Class defining a physical CAN Bus"""

    DISC = DiscreteValues()

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

        self.is_valid = False
        self._check_valid()

    def __repr__(self):
        return f"CAN Bus Definition:\n\
                Name: {self.name}\n\
                Description: {self.description}\n\
                Nodes: {self.nodes}\n"

    def _check_valid(self):
        valid = True
        if self.name == "":
            print("A bus is missing a name! Check bus definition files")
            valid = False

        if self.description == "":
            print(f"Warning: Bus '{self.name}' is missing a description")

        self.is_valid = valid

    def add_node(self, node: CanNode):
        """Add a node to this CAN bus"""
        if node in self.nodes:
            print(
                f"Tried to add node '{node.name}' to bus '{self.name}', a node with that name was already present"
            )
            return

        self.nodes[node.name] = node
        self.messages.update(node.messages)
        self.signals.update(node.signals)
