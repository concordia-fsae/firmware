from argparse import ArgumentParser, Namespace
from importlib.abc import Loader
from os import makedirs, path
from pathlib import Path
from typing import Dict, Iterator, Tuple
import pickle
import copy
from schema import Schema, Or, Optional, And
import sys

from mako import template
from mako.lookup import TemplateLookup
import yaml

from classes.Can import CanBus, CanMessage, CanNode, CanSignal, DiscreteValues
from classes.Types import *

NODE_FILE = "{name}.yaml"
MESSAGE_FILE = "{name}-message.yaml"
RX_FILE = "{name}-rx.yaml"
SIG_FILE = "{name}-signals.yaml"

ACCEPTED_YAML_FILES = [
    NODE_FILE,
    MESSAGE_FILE,
    RX_FILE,
    SIG_FILE,
]

can_bus_defs = {}
can_nodes = {}
discrete_values = {}
templates = {}

BUS_SCHEMA = Schema({
    "name": str,
    "description": str,
    "defaultEndianness": str,
    "baudrate": Or(1000000, 500000),
})

NODE_SCHEMA = Schema({
    "description": str,
    "onBuses": Or(str, list[str]),
    Optional("duplicateNode"): int,
    Optional("baseId"): int,
})

SIGNAL_SCHEMA = Schema({
    Optional("description"): str,
    Optional("unit"): str,
    Optional("nativeRepresentation"): dict,
    Optional("discreteValues"): str,
    Optional("continuous"): bool,
    Optional("validationRole"): str,
    Optional("template"): str,
})

MESSAGE_SCHEMA = Schema({
    Optional("description"): str,
    Optional("cycleTimeMs"): int,
    Optional("id"): int,
    Optional("idOffset"): int,
    Optional("lengthBytes"): int,
    Optional("sourceBuses"): Or(str, list[str]),
    Optional("signals"): dict,
    Optional("unscheduled"): bool,
    Optional("template"): str,
})

RX_FILE_SCHEMA = Schema({
    "messages": Or(dict, None),
    "signals": Or(dict, None),
})

RX_ITEM_SCHEMA = Schema({
    Optional("sourceBuses"): Or(str, list[str]),
    Optional("unrecorded"): bool,
    Optional("node"): Or(int, list[int])
})

# if this gets set during the build, we will fail at the end
ERROR = False

class UniqueKeyLoader(yaml.SafeLoader):
    def construct_mapping(self, node, deep=False):
        mapping = set()
        for key_node, value_node in node.value:
            key = self.construct_object(key_node, deep=deep)
            if key in mapping:
                raise ValueError(f"Duplicate {key!r} key found in YAML.")
            mapping.add(key)
        return super().construct_mapping(node, deep)

def safe_load(file):
    return yaml.load(file, Loader=UniqueKeyLoader)

def generate_discrete_values(definition_dir: Path) -> None:
    """Load discrete values from discrete-values.yaml"""
    global ERROR
    global discrete_values
    discrete_values = DiscreteValues()
    error = False

    with open(definition_dir.joinpath("discrete_values.yaml"), "r") as fd:
        dvs: Dict[str, Dict[str, int]] = safe_load(fd)

    for name, val in dvs.items():
        try:
            discrete_value = DiscreteValue(name, val)
            for vals, _ in val.items():
                if any(c for c in vals if c.islower()):
                    print(f"Discrete Value '{name}' cannot contain lower case in enum '{vals}'")
                    error = True
                    continue
                if any(c for c in vals if not c.isalnum() and c != '_'):
                    print(f"Discrete Value '{name}' may only contain capital letters, numbers, or underscores in enum '{vals}'")
                    error = True
                    continue
            setattr(discrete_values, name, discrete_value)
        except ValueError as e:
            print(e)
            ERROR = True
    if error:
        ERROR = True
        raise Exception("Error generating discrete values, review previous errors...")

def generate_templates(definition_dir: Path) -> None:
    """Load discrete values from signals.yaml"""
    global ERROR
    global templates
    error = False

    with open(definition_dir.joinpath("signals.yaml"), "r") as fd:
        templates["signals"] = safe_load(fd)["signals"]
        for sig, definition in templates["signals"].items():
            try:
                SIGNAL_SCHEMA.validate(definition)
            except Exception as e:
                print(f"Error generating template signal '{sig}': {e}")
                error = True
                continue
    with open(definition_dir.joinpath("messages.yaml"), "r") as fd:
        messages = safe_load(fd)["messages"]
        templates["messages"] = {}
        for message, definition in messages.items():
            try:
                MESSAGE_SCHEMA.validate(definition)
            except Exception as e:
                print(f"Error generating template message '{message}': {e}")
                error = True
                continue
            if "sourceBuses" in definition:
                ERROR = True
                print(f"Template message '{message}' cannot define the source buses of a message")
                break
            elif "id" in definition:
                ERROR = True
                print(f"Template message '{message}' cannot define the id of a message")
                break
            elif "signals" not in definition:
                ERROR = True
                print(f"Template message '{message}' must define the signals of a message")
                break
            else:
                templates["messages"][message] = definition
    if error:
        ERROR = True
        raise Exception("Error processing templates, see previous errors...")

def generate_can_buses(definition_dir: Path) -> None:
    """Generate CAN buses based on yaml files"""
    global ERROR
    error = False
    can_buses = definition_dir.joinpath("buses").glob("*.yaml")
    for bus_file_path in can_buses:
        with open(bus_file_path, "r") as bus_file:
            can_bus_def = safe_load(bus_file)
        try:
            BUS_SCHEMA.validate(can_bus_def)
            try:
                Endianess[can_bus_def["defaultEndianness"]]
            except:
                raise Exception(f"Endianness '{can_bus_def["defaultEndianness"]}' is not valid. Endianness can be { [ e.name for e in Endianess ] }.")
        except Exception as e:
            print(f"CANbus configuration file '{bus_file_path}' is invalid.")
            print(f"CAN Bus Schema Error: {e}")
            error = True
            continue

        try:
            can_bus_defs[can_bus_def["name"]] = CanBus(can_bus_def)
        except Exception as e:
            # FIXME: except the specific exception we're expecting here
            raise e

    if error:
        ERROR = True
        raise Exception("Error generating CAN bus', review previous errors...")


def generate_can_nodes(definition_dir: Path) -> None:
    """
    Generate CAN nodes based on yaml files
    """
    global ERROR
    error = False

    nodes = [
        dir for dir in definition_dir.joinpath("data/components").iterdir() if dir.is_dir()
    ]

    for node in nodes:
        if node.name in can_nodes:
            print(
                f"Duplicate node definition detected! Node '{node.name}' already defined"
            )
            ERROR = True
            continue

        node_dict = {"name": node.name, "def_files": {}}
        node_files = node.glob("*.yaml")

        for file in node_files:
            if not file.name in [
                fn.format(name=node.name) for fn in ACCEPTED_YAML_FILES
            ]:
                print(
                    f"Warning: Unexpected file in node dir '{node.name}': {file.name}"
                )
            else:
                node_dict["def_files"][file.name] = file

        if not f"{node.name}.yaml" in node_dict["def_files"]:
            print(f"Missing node config file {node.name}.yaml")
            node_dict["skip"] = True
            # ERROR = True

        message_file = f"{node.name}-message.yaml" in node_dict["def_files"]
        signals_file = f"{node.name}-signals.yaml" in node_dict["def_files"]

        if (message_file and not signals_file) or (not message_file and signals_file):
            print(
                f"You must either provide both a '{node.name}-message.yaml' and '{node.name}-signals.yaml file, or neither"
            )
            ERROR = True

        if "skip" in node_dict:
            continue

        with open(node_dict["def_files"][f"{node.name}.yaml"], "r") as fd:
            node_def = safe_load(fd)
            node_dict.update(node_def)

        try:
            NODE_SCHEMA.validate(node_def)
            if type(node_def["onBuses"]) is list:
                for bus in node_def["onBuses"]:
                    if bus not in can_bus_defs:
                        raise Exception(f"Node '{node.name}' is on bus '{bus}' but that bus is not defined.")
            else:
                if node_def["onBuses"] not in can_bus_defs:
                    raise Exception(f"Node '{node.name}' is on bus '{node_def["onBuses"]}' but that bus is not defined.")
        except Exception as e:
            print(f"CAN node configuration file for node '{node.name}' is invalid.")
            print(f"CAN Node Error: {e}")
            error = True
            continue

        if "duplicateNode" in node_dict:
            for i in range(0, node_def["duplicateNode"]):
                # create one object and add it to all buses so that each bus will have the same object
                # that way if we modify it in one place it will apply to all buses
                can_node = CanNode(node.name + "_" + str(i), node_dict)
                can_node.on_buses = node_def["onBuses"]
                can_nodes[node.name + "_" + str(i)] = can_node
        else:
            # create one object and add it to all buses so that each bus will have the same object
            # that way if we modify it in one place it will apply to all buses
            can_node = CanNode(node.name, node_dict)
            can_node.on_buses = node_def["onBuses"]
            can_nodes[node.name] = can_node

    if error:
        ERROR = True
        raise Exception("Error generating CAN nodes, review previous errors...")


def process_node(node: CanNode):
    """Process the signals and messages associated with a given CAN node"""
    global ERROR
    global templates
    error = False

    sig_file = SIG_FILE.format(name=node.name)
    msg_file = MESSAGE_FILE.format(name=node.name)

    if sig_file not in node.def_files:
        return

    signals_dict = {}
    with open(node.def_files[sig_file], "r") as signals_file:
        try:
            signals_dict = safe_load(signals_file)["signals"] or {}
            for key in signals_dict.keys():
                if not key.isalnum():
                    raise Exception(f"Signal name '{key}' can contain only alpha-numeric characters.")
        except Exception as e:
            raise Exception(f"Error in signal file {sig_file}: {e}")

    if signals_dict is not None:
        for sig, definition in signals_dict.items():
            try:
                SIGNAL_SCHEMA.validate(definition)
                if "nativeRepresentation" not in definition and "discreteValues" not in definition:
                    raise Exception(f"Signal '{sig}' in '{node.name}' has neither a discreteValues or nativeRepresentation.")
                if "unit" in definition:
                    try:
                        Units(definition["unit"])
                    except:
                        raise Exception(f"Unit '{definition["unit"]}' is not an accepted unit.")
                if "validationRole" in definition:
                    try:
                        ValidationRole(definition["validationRole"])
                    except:
                        raise Exception(f"Validation role '{definition["validationRole"]}' is not valid.")
            except Exception as e:
                print(f"CAN signal definition for '{sig}' in node '{node.name}' is invalid.")
                print(f"CAN Signal Schema Error: {e}")
                error = True
                continue
        if error:
            ERROR = True
            raise Exception("Error processing node signals, see previous errors...")

    signals = {}

    if not signals_dict:
        print(f"Warning: No signals found in signal file for node '{node.name}'")

    if signals_dict is not None:
        for signal in signals_dict:
            if node.duplicateNode:
                sig_name = f"{node.name.upper()}{node.offset}_{signal}"
            else:
                sig_name = f"{node.name.upper()}_{signal}"
            try:
                sig_obj = CanSignal(sig_name, signals_dict[signal])
                signals[sig_obj.name] = sig_obj
            except Exception as e:
                print(e)
                error = True
                continue
    if error:
        ERROR = True
        raise Exception(f"Error processing node '{node.name}' signals, see previous errors...")

    with open(node.def_files[msg_file], "r") as messages_file:
        try:
            messages_dict = safe_load(messages_file).get("messages", {})
            for key in messages_dict.keys():
                if not key.isalnum():
                    raise Exception(f"Message name '{key}' can contain only alpha-numeric characters.")
        except Exception as e:
            raise Exception(f"Error in message file {msg_file}: {e}")

    if not messages_dict:
        print(f"No messages found in message file for node '{node.name}'")
        ERROR = True
        return


    for name, definition in messages_dict.items():
        if node.duplicateNode:
            msg_name = f"{node.name.upper()}{node.offset}_{name}"
        else:
            msg_name = f"{node.name.upper()}_{name}"

        if "template" in definition:
            if definition["template"] not in templates["messages"]:
                print(f"Message '{name}' has template message '{definition["template"]}' which can not be found in the template messages")
                ERROR = True
                break
            definition.update(templates["messages"][definition["template"]])
        try:
            MESSAGE_SCHEMA.validate(definition)
            if "lengthBytes" in definition and (definition["lengthBytes"] < 1 or definition["lengthBytes"] > 8):
                raise Exception("Message length must be greater than 0 and less than or equal to 8")
            if "id" not in definition and "idOffset" not in definition:
                raise Exception("Message must have a specified 'id' or 'idOffset'")
            elif "id" in definition and "idOffset" in definition:
                raise Exception("Message cannot have an 'id' and 'idOffset' at the same time")
        except Exception as e:
            print(f"CAN message definition for '{name}' in node '{node.name}' is invalid.")
            print(f"Message Schema Error: {e}")
            error = True
            continue

        if "idOffset" in definition:
            if node.baseId is None:
                print(f"Message '{name}' has a 'idOffset' specified and {node.name} does not have a 'baseId' specified")
                ERROR = True
                break
            else:
                definition["id"] = definition["idOffset"] + node.baseId + node.offset
        else:
            definition["id"] = definition["id"] + node.offset

        for existing_node in can_nodes:
            for msg in can_nodes[existing_node].messages:
                if definition["id"] == can_nodes[existing_node].messages[msg].id:
                    print(f"Message {msg_name} has the same ID as {msg}")
                    ERROR = True
                    break
        msg_obj = CanMessage(node, msg_name, definition)

        if msg_obj.signals is None:
            print(f"No signals were defined for message {msg_obj.name}")
            ERROR = True
            continue

        msg_obj.node_ref = node
        for msg_signal in msg_obj.signals:
            sig = msg_signal.split('_')[1]
            if definition['signals'][sig] is not None and "template" in definition["signals"][sig]:
                if "unit" in definition["signals"][sig]:
                    ERROR = True
                    print(f"Templated signal '{msg_signal}' cannot override template unit")
                    break
                elif "nativeRepresentation" in definition["signals"][sig]:
                    ERROR = True
                    print(f"Templated signal '{msg_signal}' cannot override template native representation")
                    break
                elif "validationRole" in definition["signals"][sig]:
                    ERROR = True
                    print(f"Templated signal '{msg_signal}' cannot override template validation role")
                    break
                elif "discreteValues" in definition["signals"][sig]:
                    ERROR = True
                    print(f"Templated signal '{msg_signal}' cannot override template discrete value")
                    break
                elif definition['signals'][sig]['template'] not in templates["signals"]:
                    ERROR = True
                    print(f"Templated signal '{definition['signals'][sig]['template']}' in message '{msg_signal}' cannot be found")
                    break

                new_sig = copy.deepcopy(templates["signals"][definition['signals'][sig]['template']])
                new_sig.update(definition['signals'][sig])
                try:
                    SIGNAL_SCHEMA.validate(new_sig)
                    if "nativeRepresentation" not in new_sig and "discreteValues" not in new_sig:
                        raise Exception(f"Signal '{msg_signal}' in '{node.name}' has neither a discreteValues or nativeRepresentation.")
                    if "unit" in definition:
                        try:
                            Units(definition["unit"])
                        except:
                            raise Exception(f"Unit '{new_sig["unit"]}' is not an accepted unit.")
                    if "validationRole" in new_sig:
                        try:
                            ValidationRole(new_sig["validationRole"])
                        except:
                            raise Exception(f"Validation role '{new_sig["validationRole"]}' is not valid.")
                except Exception as e:
                    print(f"CAN signal definition for '{msg_signal}' in node '{node.name}' is invalid.")
                    print(f"CAN Signal Schema Error: {e}")
                    error = True
                    continue
                try:
                    sig_obj = CanSignal(msg_signal, new_sig)
                except:
                    error = True
                    continue
                signals[msg_signal] = sig_obj
            if msg_signal in signals:
                if not signals[msg_signal].is_valid:
                    print(
                        f"Signal '{signals[msg_signal].name}' is invalid. See previous errors"
                    )
                    ERROR = True
                signals[msg_signal].message_ref = msg_obj
                msg_obj.add_signal(msg_obj.signals[msg_signal], signals[msg_signal])
            else:
                print(
                    f"A signal was defined in {node.name}-message.yaml "
                    "but the signal definition was not found in the signals file: "
                    f"{msg_signal}"
                )
                ERROR = True
        if error:
            ERROR = True
            raise Exception(f"Error processing node '{node.name}' signals, see previous errors...")
        msg_obj.validate_msg()
        if not msg_obj.is_valid:
            ERROR = True
        node.add_message(msg_obj)
    if error:
        ERROR = True
        raise Exception(f"Error processing node '{node.name}' messages, see previous errors...")

    for signal in signals.keys():
        if not signal in node.signals.keys():
            print(
                f"Signal '{signal}' was defined in {node.name}-signals.yaml but was "
                "not placed in a message"
            )
            ERROR = True

    for bus in node.on_buses:
        can_bus_defs[bus].add_node(node)


def process_receivers(bus: CanBus, node: CanNode):
    """Process signals received by a node"""
    global ERROR
    error = False

    rx_file = RX_FILE.format(name=node.name)
    if rx_file not in node.def_files:
        print(f"Note: rx file not found for node '{node.name}'")
        return
    with open(node.def_files[rx_file], "r") as fd:
        rx_sig_file = safe_load(fd)
        try:
            RX_FILE_SCHEMA.validate(rx_sig_file)
        except Exception as e:
            print(f"CAN rx definition for node '{node.name}' is invalid.")
            print(f"File Schema Error: {e}")
            raise e

    with open(node.def_files[rx_file], "r") as fd:
        # making this a dictionary because eventually we'll
        # probably want to be able to gateway messages from
        # one bus to another, and this would be the place to
        # define the gateway node
        rx_sig_dict = rx_sig_file["signals"] if rx_sig_file["signals"] is not None else {}

    for sig, definition in rx_sig_dict.items():
        if definition is not None:
            try:
                RX_ITEM_SCHEMA.validate(definition)
                if "sourceBuses" in definition and definition["sourceBuses"] not in can_bus_defs:
                    raise Exception(f"Source bus {definition["sourceBuses"]} is not defined in the network.")
            except Exception as e:
                print(f"CAN signal reception definition for '{sig}' in node '{node.name}' is invalid.")
                print(f"CAN RX Signal Error: {e}")
                error = True
                continue
    if error:
        ERROR = True
        raise Exception("Error processing node receivers, see previous errors...")


    with open(node.def_files[rx_file], "r") as fd:
        rx_msg_dict = rx_sig_file["messages"] if rx_sig_file["messages"] is not None else {}

    for msg, definition in rx_msg_dict.items():
        if definition is not None:
            try:
                RX_ITEM_SCHEMA.validate(definition)
                if "sourceBuses" in definition and definition["sourceBuses"] not in can_bus_defs:
                    raise Exception(f"Source bus {definition["sourceBuses"]} is not defined in the network.")
            except Exception as e:
                print(f"CAN message reception definition for '{msg}' in node '{node.name}' is invalid.")
                print(f"CAN RX Message Error: {e}")
                error = True
                continue
    if error:
        ERROR = True
        raise Exception("Error processing node receivers, see previous errors...")

    for sig_name in rx_sig_dict.keys():
        if sig_name not in bus.signals:
            print(
                f"Node '{node.alias}' attempted to RX signal '{sig_name}' "
                f"on bus {bus.name}, but that signal is not present on that bus"
            )
            ERROR = True
            continue

        rxed_sig = bus.signals[sig_name]
        rxed_msg = rxed_sig.message_ref

        node.received_sigs[sig_name] = rxed_sig
        node.received_msgs[rxed_msg.name] = rxed_msg
        rxed_msg.add_receiver(node, rxed_sig.name)

    for msg_name in rx_msg_dict.keys():
        if rx_msg_dict[msg_name] is not None and "sourceBuses" in rx_msg_dict[msg_name]:
            if bus.name not in rx_msg_dict[msg_name]["sourceBuses"]:
                continue
        if bus.name not in node.on_buses:
            print(
                f"Node '{node.alias}' attempted to RX message '{msg_name}' "
                f"on bus {bus.name}, but '{node.alias}' is not present on that bus"
            )
            ERROR = True
            continue

        if msg_name not in bus.messages.keys() or bus.name not in bus.messages[msg_name].source_buses:
            print(
                f"Node '{node.alias}' attempted to RX message '{msg_name}' "
                f"on bus {bus.name}, but that message is not present on that bus"
            )
            ERROR = True
            continue

        if rx_msg_dict[msg_name] is not None:
            if "node" in rx_msg_dict[msg_name]:
                if node.duplicateNode is False:
                    Exception(f"{msg_name} in file {rx_file} is for node {rx_msg_dict[msg_name]["node"]}, however {node.alias} is not a duplicate node.")
                if rx_msg_dict[msg_name]["node"] is not node.offset:
                    continue

        if msg_name not in node.received_msgs:
            node.received_msgs[msg_name] = bus.messages[msg_name]

        if rx_msg_dict[msg_name] is not None:
            if "unrecorded" in rx_msg_dict[msg_name]:
                continue

        for sig_name in bus.messages[msg_name].signals:
            rxed_sig = bus.signals[sig_name]

            if rxed_sig not in node.received_sigs:
                node.received_sigs[sig_name] = rxed_sig
                bus.messages[msg_name].add_receiver(node, rxed_sig.name)


def generate_dbcs(mako_lookup: TemplateLookup, bus: CanBus, output_dir: Path):
    dbc_dir = output_dir

    dbc_template = mako_lookup.get_template("template.dbc.mako")

    dbc = dbc_template.render(bus=bus, messages=bus.messages.values())
    if not isinstance(dbc, str):
        print("Error when trying to render DBCs: Result of rendering was not a string")
        return

    makedirs(dbc_dir, exist_ok=True)
    with open(dbc_dir.joinpath(f"{bus.name}.dbc"), "w") as dbc_handle:
        dbc_handle.write(dbc)


def codegen(mako_lookup: TemplateLookup, nodes: Iterator[Tuple[str, Path]]):
    global ERROR
    for node, output_dir in nodes:
        if node not in can_nodes:
            raise Exception(f"Error: Node not defined for node '{node}'")

        makos = [
            ["MessagePack_generated.c.mako", {"nodes": [can_nodes[node]]}],
            ["MessagePack_generated.h.mako", {"nodes": [can_nodes[node]]}],
            ["MessageUnpack_generated.c.mako", {"nodes": [can_nodes[node]]}],
            ["MessageUnpack_generated.h.mako", {"nodes": [can_nodes[node]]}],
            ["MessageUnpack_generated.h.mako", {"nodes": [can_nodes[node]]}],
            ["NetworkDefines_generated.h.mako", {"nodes": [can_nodes[node]], "buses": can_bus_defs}],
            ["CANTypes_generated.h.mako", {"nodes": [can_nodes[node]]}],
            ["SigTx.c.mako", {"nodes": [can_nodes[node]]}],
            ["SigRx.h.mako", {"nodes": [can_nodes[node]]}],
            ["TemporaryStubbing.h.mako", {"nodes": [can_nodes[node]]}],
        ]
        for template in makos:
            rendered = mako_lookup.get_template(template[0]).render(**template[1])
            if not isinstance(rendered, str):
                print(
                    f"Error when trying to render '{template[0]}': Result of rendering was not a string"
                )
                return

            makedirs(output_dir, exist_ok=True)
            with open(output_dir.joinpath(template[0].replace(".mako", "")), "w") as fd:
                fd.write(rendered)


def parse_args() -> Namespace:
    parser = ArgumentParser()

    parser.add_argument(
        "--data-dir",
        dest="definition_dir",
        type=Path,
        help="Path to the network definition directory",
    )

    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        type=Path,
        help="Directory where generated DBC files will be placed",
    )

    parser.add_argument(
        "--node",
        "-n",
        dest="node",
        action="append",
        type=str,
        help="Node to generate code for. Must be accompanied by a `--codegen-dir` argument",
    )

    parser.add_argument(
        "--codegen-dir",
        "-c",
        dest="codegen_dir",
        action="append",
        type=Path,
        help="Directory in which to store generated files for the preceding `--node` argument",
    )

    parser.add_argument(
        "--build",
        "-b",
        dest="build",
        action="store_true",
        help="Process YAMCAN files and create network data. Use with a '--cache-dir' argument to cache network for later usage with code generation.",
    )

    parser.add_argument(
        "--cache-dir",
        type=Path,
        dest="cache_dir",
        action="store",
        help="Directory where generated network files will be placed",
    )

    args = parser.parse_args()

    if (args.codegen_dir and not args.node) or (not args.codegen_dir and args.node):
        raise Exception(
            "You must either provide both a node and codegen directory, or neither"
        )
    elif (args.codegen_dir and args.node) and (len(args.codegen_dir) != len(args.node)):
        raise Exception(
            "You must provide the same number of nodes and codegen directories"
        )

    return args


def parseNetwork(args, lookup):
    generate_discrete_values(args.definition_dir)
    generate_templates(args.definition_dir.joinpath("data/templates"))
    generate_can_buses(args.definition_dir)
    generate_can_nodes(args.definition_dir)

    for node in can_nodes.values():
        process_node(node)

    for bus in can_bus_defs.values():
        for node in bus.nodes.values():
            if not node.processed:
                process_receivers(bus, node)

def calculate_bus_load(bus, output_dir):
    total_bits_per_s = 0
    total_messages_per_s = 0
    total_unscheduled_bits = 0
    total_unscheduled_messages = 0
    total_messages = 0

    output = [ open(str(output_dir) + f"/{bus.name}-stats.txt", 'w'), sys.stdout ]

    for _, message in bus.messages.items():
        # SOF(1), RTR(1), IDE(1), DLC(4), CRC(15)
        # DEL(1), ACK(1), DEL(1), EOF(7), ITM(11)
        if bus.name not in message.source_buses:
            continue

        bit_length = 43
        bit_length += message.length_bytes * 8
        bit_length += 11 if message.id <= 0x7ff else 29

        if message.unscheduled:
            total_unscheduled_bits += bit_length
            total_unscheduled_messages += 1
            continue
        total_messages += 1
        messages_per_s = 1000 / message.cycle_time_ms

        total_bits_per_s += bit_length * messages_per_s
        total_messages_per_s += messages_per_s

    usage = total_bits_per_s / bus.baudrate
    for out in output:
        print(f"CAN Bus Statistics for '{bus.name.upper()}'; Baudrate: {bus.baudrate}; Usage: {usage * 100}%", file=out)
        print(f"Total Bits Transmitted/s: {int(total_bits_per_s)}; Total Messages Transmitted/s: {int(total_messages_per_s)}", file=out)
        if total_unscheduled_messages > 0:
            print(f"Total Bits Unscheduled: {int(total_unscheduled_bits)}; Total Messages Unscheduled: {int(total_unscheduled_messages)}", file=out)
        print(f"Total Nodes: {len(bus.nodes)}; Count Messages: {total_messages}", file=out)

    if usage > 0.8:
        print(f"Warning: CANBus '{bus.name.upper()}' has high bus load")
    if usage > 1:
        raise Exception(f"CAN Bus '{bus.name.upper()}' has total bus usage of {usage}")

def main():
    """Main function"""
    global ERROR
    global can_nodes
    global can_bus_defs
    global discrete_values
    global templates

    # parse arguments
    args = parse_args()
    lookup = TemplateLookup(directories=[ path.dirname(__file__) + "/templates" ])

    if args.build:
        print(f"Parsing network...")
        parseNetwork(args, lookup)
        if not ERROR:
            for bus in can_bus_defs.values():
                calculate_bus_load(bus, args.output_dir)
                generate_dbcs(lookup, bus, args.output_dir)
        if args.cache_dir:
            pickle.dump(can_nodes, open(args.cache_dir.joinpath("CachedNodes.pickle"), "wb"))
            pickle.dump(can_bus_defs, open(args.cache_dir.joinpath("CachedBusDefs.pickle"), "wb"))
            pickle.dump(discrete_values, open(args.cache_dir.joinpath("CachedDiscreteValues.pickle"), "wb"))
            pickle.dump(templates, open(args.cache_dir.joinpath("CachedTemplates.pickle"), "wb"))
    elif args.cache_dir:
        try:
            can_nodes = pickle.load(open(args.cache_dir.joinpath("CachedNodes.pickle"), "rb"))
            can_bus_defs = pickle.load(open(args.cache_dir.joinpath("CachedBusDefs.pickle"), "rb"))
            discrete_values = pickle.load(open(args.cache_dir.joinpath("CachedDiscreteValues.pickle"), "rb"))
            templates = pickle.load(open(args.cache_dir.joinpath("CachedTemplates.pickle"), "rb"))
        except Exception as e:
            print(f"Could not retreive cache files. Try building the network again...")
            ERROR = True

    if ERROR:
        raise Exception("Build failed. See previous errors in output")

    if args.node:
        codegen(lookup, zip(args.node, args.codegen_dir))


if __name__ == "__main__":
    main()
