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
import zlib

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

BUS_SCHEMA = Schema(
    {
        "name": str,
        "description": str,
        "defaultEndianness": str,
        Optional("interfaceType"): Or("physical", "virtual"),
        Optional("baudrate"): Or(1000000, 500000),
    }
)

NODE_SCHEMA = Schema(
    {
        "description": str,
        "onBuses": Or(str, list[str]),
        Optional("duplicateNode"): int,
        Optional("baseId"): int,
    }
)

SIGNAL_SCHEMA = Schema(
    {
        Optional("description"): str,
        Optional("unit"): str,
        Optional("nativeRepresentation"): dict,
        Optional("discreteValues"): str,
        Optional("continuous"): bool,
        Optional("validationRole"): str,
        Optional("template"): str,
    }
)

MESSAGE_SCHEMA = Schema(
    {
        Optional("description"): str,
        Optional("cycleTimeMs"): int,
        Optional("timeoutPeriodMs"): int,
        Optional("id"): int,
        Optional("idOffset"): int,
        Optional("lengthBytes"): int,
        Optional("messageType"): str,
        Optional("sourceBuses"): Or(str, list[str]),
        Optional("signals"): Or(dict, None),
        Optional("unscheduled"): bool,
        Optional("template"): str,
    }
)

RX_FILE_SCHEMA = Schema(
    {
        "messages": Or(dict, None),
        "signals": Or(dict, None),
        Optional("receiveAllMessagesOnBuses"): Or(str, list[str]),
        Optional("forwarding"): Or(list, None),
    }
)

RX_ITEM_SCHEMA = Schema(
    {
        Optional("sourceBuses"): Or(str, list[str]),
        Optional("unrecorded"): bool,
        Optional("node"): Or(int, list[int]),
    }
)

FORWARDING_ITEM_SCHEMA = Schema(
    {
        "sourceBus": str,
        "destBus": str,
        Optional("policy"): Or("all", "bridged"),
        Optional("bridgedMessages"): Or(list[str], None),
    }
)

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
                    print(
                        f"Discrete Value '{name}' cannot contain lower case in enum '{vals}'"
                    )
                    error = True
                    continue
                if any(c for c in vals if not c.isalnum() and c != "_"):
                    print(
                        f"Discrete Value '{name}' may only contain capital letters, numbers, or underscores in enum '{vals}'"
                    )
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
                print(
                    f"Template message '{message}' cannot define the source buses of a message"
                )
                break
            elif "id" in definition:
                ERROR = True
                print(f"Template message '{message}' cannot define the id of a message")
                break
            elif "signals" not in definition:
                ERROR = True
                print(
                    f"Template message '{message}' must define the signals of a message"
                )
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
            interface_type = can_bus_def.get("interfaceType", "physical")
            if interface_type == "virtual":
                if "baudrate" in can_bus_def:
                    raise Exception("Virtual buses must not define a baudrate.")
            elif "baudrate" not in can_bus_def:
                raise Exception("Physical buses must define a baudrate.")
            try:
                Endianess[can_bus_def["defaultEndianness"]]
            except:
                raise Exception(
                    f"Endianness '{can_bus_def['defaultEndianness']}' is not valid. Endianness can be { [ e.name for e in Endianess ] }."
                )
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
        dir
        for dir in definition_dir.joinpath("data/components").iterdir()
        if dir.is_dir()
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

        if not message_file or not signals_file:
            print(
                f"Missing node transmit definition files for '{node.name}'. "
                f"Expected both '{node.name}-message.yaml' and '{node.name}-signals.yaml'."
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
                        raise Exception(
                            f"Node '{node.name}' is on bus '{bus}' but that bus is not defined."
                        )
            else:
                if node_def["onBuses"] not in can_bus_defs:
                    raise Exception(
                        f"Node '{node.name}' is on bus '{node_def['onBuses']}' but that bus is not defined."
                    )
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
            signals_root = safe_load(signals_file) or {}
            signals_dict = signals_root.get("signals", {}) or {}
            for key in signals_dict.keys():
                if not key.isalnum():
                    raise Exception(
                        f"Signal name '{key}' can contain only alpha-numeric characters."
                    )
        except Exception as e:
            raise Exception(f"Error in signal file {sig_file}: {e}")

    if signals_dict is not None:
        for sig, definition in signals_dict.items():
            try:
                SIGNAL_SCHEMA.validate(definition)
                if "template" in definition:
                    if definition["template"] not in templates["signals"]:
                        raise Exception(
                            f"Signal '{sig}' has template signal '{definition['template']}' which can not be found in the template signals"
                        )
                    definition.update(templates["signals"][definition["template"]])
                if (
                    "nativeRepresentation" not in definition
                    and "discreteValues" not in definition
                ):
                    raise Exception(
                        f"Signal '{sig}' in '{node.name}' has neither a discreteValues or nativeRepresentation."
                    )
                if "unit" in definition:
                    try:
                        Units(definition["unit"])
                    except:
                        raise Exception(
                            f"Unit '{definition['unit']}' is not an accepted unit."
                        )
                if "validationRole" in definition:
                    try:
                        ValidationRole(definition["validationRole"])
                    except:
                        raise Exception(
                            f"Validation role '{definition['validationRole']}' is not valid."
                        )
            except Exception as e:
                print(
                    f"CAN signal definition for '{sig}' in node '{node.name}' is invalid."
                )
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
        raise Exception(
            f"Error processing node '{node.name}' signals, see previous errors..."
        )

    with open(node.def_files[msg_file], "r") as messages_file:
        try:
            messages_root = safe_load(messages_file) or {}
            messages_dict = messages_root.get("messages", {}) or {}
            for key in messages_dict.keys():
                if not key.isalnum():
                    raise Exception(
                        f"Message name '{key}' can contain only alpha-numeric characters."
                    )
        except Exception as e:
            raise Exception(f"Error in message file {msg_file}: {e}")

    if not messages_dict:
        print(f"No messages found in message file for node '{node.name}'")

    for name, definition in messages_dict.items():
        if node.duplicateNode:
            msg_name = f"{node.name.upper()}{node.offset}_{name}"
        else:
            msg_name = f"{node.name.upper()}_{name}"

        if "template" in definition:
            if definition["template"] not in templates["messages"]:
                print(
                    f"Message '{name}' has template message '{definition['template']}' which can not be found in the template messages"
                )
                ERROR = True
                break
            definition.update(templates["messages"][definition["template"]])
        try:
            MESSAGE_SCHEMA.validate(definition)
            if "lengthBytes" in definition and (
                definition["lengthBytes"] < 1 or definition["lengthBytes"] > 8
            ):
                raise Exception(
                    "Message length must be greater than 0 and less than or equal to 8"
                )
            if "id" not in definition and "idOffset" not in definition:
                raise Exception("Message must have a specified 'id' or 'idOffset'")
            elif "id" in definition and "idOffset" in definition:
                raise Exception(
                    "Message cannot have an 'id' and 'idOffset' at the same time"
                )
        except Exception as e:
            print(
                f"CAN message definition for '{name}' in node '{node.name}' is invalid."
            )
            print(f"Message Schema Error: {e}")
            error = True
            continue

        if "idOffset" in definition:
            if node.baseId is None:
                print(
                    f"Message '{name}' has a 'idOffset' specified and {node.name} does not have a 'baseId' specified"
                )
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
            sig = msg_signal.split("_")[1]
            if (
                definition["signals"][sig] is not None
                and "template" in definition["signals"][sig]
            ):
                if "unit" in definition["signals"][sig]:
                    ERROR = True
                    print(
                        f"Templated signal '{msg_signal}' cannot override template unit"
                    )
                    break
                elif "nativeRepresentation" in definition["signals"][sig]:
                    ERROR = True
                    print(
                        f"Templated signal '{msg_signal}' cannot override template native representation"
                    )
                    break
                elif "validationRole" in definition["signals"][sig]:
                    ERROR = True
                    print(
                        f"Templated signal '{msg_signal}' cannot override template validation role"
                    )
                    break
                elif "discreteValues" in definition["signals"][sig]:
                    ERROR = True
                    print(
                        f"Templated signal '{msg_signal}' cannot override template discrete value"
                    )
                    break
                elif definition["signals"][sig]["template"] not in templates["signals"]:
                    ERROR = True
                    print(
                        f"Templated signal '{definition['signals'][sig]['template']}' in message '{msg_signal}' cannot be found"
                    )
                    break

                new_sig = copy.deepcopy(
                    templates["signals"][definition["signals"][sig]["template"]]
                )
                new_sig.update(definition["signals"][sig])
                try:
                    SIGNAL_SCHEMA.validate(new_sig)
                    if (
                        "nativeRepresentation" not in new_sig
                        and "discreteValues" not in new_sig
                    ):
                        raise Exception(
                            f"Signal '{msg_signal}' in '{node.name}' has neither a discreteValues or nativeRepresentation."
                        )
                    if "unit" in definition:
                        try:
                            Units(definition["unit"])
                        except:
                            raise Exception(
                                f"Unit '{new_sig['unit']}' is not an accepted unit."
                            )
                    if "validationRole" in new_sig:
                        try:
                            ValidationRole(new_sig["validationRole"])
                        except:
                            raise Exception(
                                f"Validation role '{new_sig['validationRole']}' is not valid."
                            )
                except Exception as e:
                    print(
                        f"CAN signal definition for '{msg_signal}' in node '{node.name}' is invalid."
                    )
                    print(f"CAN Signal Schema Error: {e}")
                    error = True
                    continue
                try:
                    sig_obj = CanSignal(msg_signal, new_sig)
                except Exception as e:
                    print(f"Error parsing signal '{msg_signal}': {e}")
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
            raise Exception(
                f"Error processing node '{node.name}' signals, see previous errors..."
            )
        msg_obj.validate_msg()
        if not msg_obj.is_valid:
            ERROR = True
        node.add_message(msg_obj)
    if error:
        ERROR = True
        raise Exception(
            f"Error processing node '{node.name}' messages, see previous errors..."
        )

    for signal in signals.keys():
        if not signal in node.signals.keys():
            print(
                f"Signal '{signal}' was defined in {node.name}-signals.yaml but was "
                "not placed in a message"
            )
            ERROR = True

    for bus in node.on_buses:
        can_bus_defs[bus].add_node(node)


def process_bridges(node: CanNode):
    global ERROR
    global can_bus_defs
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

    for route in node.forwarding_routes:
        if route["policy"] == "all":
            message_names = [
                message.name
                for message in can_bus_defs[route["source_bus"]].messages.values()
                if route["source_bus"] in message.source_buses
            ]
        else:
            message_names = route.get("bridged_messages", [])

        for msg_name in message_names:
            bridge_message_to_bus(
                node, route["source_bus"], route["dest_bus"], msg_name
            )


def bridge_message_to_bus(node: CanNode, source_bus: str, dest_bus: str, msg_name: str):
    source_message = can_bus_defs[source_bus].messages[msg_name]
    node.bridged_rx_messages.add((source_bus, msg_name))

    if msg_name in can_bus_defs[dest_bus].messages:
        return

    message = copy.deepcopy(source_message)
    message.from_bridge = True
    message.source_buses = [dest_bus]
    message.origin_bus = source_bus
    node.add_message(message)
    can_bus_defs[dest_bus].messages[message.name] = message
    sigs = {}
    for sig, defn in message.signal_objs.items():
        sigs[sig] = defn
    can_bus_defs[dest_bus].signals.update(sigs)


def process_forwarding(node: CanNode):
    global ERROR
    error = False

    rx_file = RX_FILE.format(name=node.name)
    if rx_file not in node.def_files:
        return

    with open(node.def_files[rx_file], "r") as fd:
        rx_sig_file = safe_load(fd) or {"messages": {}, "signals": {}}
        try:
            RX_FILE_SCHEMA.validate(rx_sig_file)
        except Exception as e:
            print(f"CAN rx definition for node '{node.name}' is invalid.")
            print(f"File Schema Error: {e}")
            raise e

    node.forwarding_routes = []
    forwarding_defs = rx_sig_file.get("forwarding") or []
    seen_routes = set()

    def upsert_route(source_bus: str, dest_bus: str, policy: str):
        for route in node.forwarding_routes:
            if route["source_bus"] == source_bus and route["dest_bus"] == dest_bus:
                if route["policy"] == "all" or policy == route["policy"]:
                    return
                if policy == "all":
                    route["policy"] = "all"
                    route["bridged_messages"] = []
                return
        node.forwarding_routes.append(
            {
                "source_bus": source_bus,
                "dest_bus": dest_bus,
                "policy": policy,
                "bridged_messages": [],
            }
        )
        seen_routes.add((source_bus, dest_bus))

    for definition in forwarding_defs:
        try:
            FORWARDING_ITEM_SCHEMA.validate(definition)

            source_bus = definition["sourceBus"]
            dest_bus = definition["destBus"]
            policy = definition.get("policy", "all")
            bridged_messages = definition.get("bridgedMessages") or []

            if source_bus not in can_bus_defs:
                raise Exception(
                    f"Source bus '{source_bus}' is not defined in the network."
                )
            if dest_bus not in can_bus_defs:
                raise Exception(
                    f"Destination bus '{dest_bus}' is not defined in the network."
                )

            if source_bus not in node.on_buses:
                raise Exception(
                    f"Node '{node.alias}' cannot define forwarding for bus '{source_bus}' because it is not present on that bus."
                )
            if dest_bus not in node.on_buses:
                raise Exception(
                    f"Node '{node.alias}' cannot define forwarding to bus '{dest_bus}' because it is not present on that bus."
                )
            if source_bus == dest_bus:
                raise Exception(
                    f"Node '{node.alias}' cannot forward bus '{source_bus}' to itself."
                )
            if policy == "all" and bridged_messages:
                raise Exception(
                    f"Node '{node.alias}' defines bridgedMessages for '{source_bus}' -> '{dest_bus}' with policy 'all'."
                )
            if policy == "bridged" and not bridged_messages:
                raise Exception(
                    f"Node '{node.alias}' must define bridgedMessages for '{source_bus}' -> '{dest_bus}' when policy is 'bridged'."
                )
            route_key = (source_bus, dest_bus)
            if route_key in seen_routes:
                raise Exception(
                    f"Node '{node.alias}' defines multiple forwarding routes for '{source_bus}' -> '{dest_bus}'."
                )

            upsert_route(source_bus, dest_bus, policy)
            route = next(
                route
                for route in node.forwarding_routes
                if route["source_bus"] == source_bus and route["dest_bus"] == dest_bus
            )
            if policy == "bridged":
                route["bridged_messages"] = bridged_messages
        except Exception as e:
            print(f"CAN forwarding definition for node '{node.name}' is invalid.")
            print(f"CAN Forwarding Error: {e}")
            error = True

    for route in node.forwarding_routes:
        if route["policy"] != "bridged":
            continue
        for msg_name in route["bridged_messages"]:
            if msg_name not in can_bus_defs[route["source_bus"]].messages:
                print(
                    f"Node '{node.alias}' forwards bridged message '{msg_name}' from "
                    f"'{route['source_bus']}' to '{route['dest_bus']}', but that message "
                    "does not exist on the source bus."
                )
                error = True

    if error:
        ERROR = True
        raise Exception("Error processing node forwarding, see previous errors...")


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

    rx_all = rx_sig_file.get("receiveAllMessagesOnBuses")
    rx_all_buses: list[str] = []
    if rx_all is not None:
        rx_all_buses = rx_all if type(rx_all) is list else [rx_all]
        for b in rx_all_buses:
            if b not in can_bus_defs:
                print(
                    f"Node '{node.alias}' attempted to RX all messages on bus '{b}', but that bus is not defined."
                )
                error = True

    with open(node.def_files[rx_file], "r") as fd:
        # making this a dictionary because eventually we'll
        # probably want to be able to gateway messages from
        # one bus to another, and this would be the place to
        # define the gateway node
        rx_sig_dict = (
            rx_sig_file["signals"] if rx_sig_file["signals"] is not None else {}
        )

    for sig, definition in rx_sig_dict.items():
        if definition is not None:
            try:
                RX_ITEM_SCHEMA.validate(definition)
                if (
                    "sourceBuses" in definition
                    and definition["sourceBuses"] not in can_bus_defs
                ):
                    raise Exception(
                        f"Source bus {definition['sourceBuses']} is not defined in the network."
                    )
                if "bridge" in definition:
                    raise Exception(f"Cannot bridge signals.")
            except Exception as e:
                print(
                    f"CAN signal reception definition for '{sig}' in node '{node.name}' is invalid."
                )
                print(f"CAN RX Signal Error: {e}")
                error = True
                continue
    if error:
        ERROR = True
        raise Exception("Error processing node receivers, see previous errors...")

    def add_message_and_signals(rxed_msg: CanMessage):
        if rxed_msg.name not in node.received_msgs:
            node.received_msgs[rxed_msg.name] = rxed_msg
        for sig_name, sig_obj in rxed_msg.signal_objs.items():
            if sig_name not in node.received_sigs:
                node.received_sigs[sig_name] = sig_obj
            rxed_msg.add_receiver(node, sig_name)

        if (
            rxed_msg.checksum_sig
            and rxed_msg.checksum_sig.name not in node.received_sigs
        ):
            node.received_sigs[rxed_msg.checksum_sig.name] = rxed_msg.checksum_sig
            rxed_msg.add_receiver(node, rxed_msg.checksum_sig.name)
        if rxed_msg.counter_sig and rxed_msg.counter_sig.name not in node.received_sigs:
            node.received_sigs[rxed_msg.counter_sig.name] = rxed_msg.counter_sig
            rxed_msg.add_receiver(node, rxed_msg.counter_sig.name)

    with open(node.def_files[rx_file], "r") as fd:
        rx_msg_dict = (
            rx_sig_file["messages"] if rx_sig_file["messages"] is not None else {}
        )

    for msg, definition in rx_msg_dict.items():
        if definition is not None:
            try:
                RX_ITEM_SCHEMA.validate(definition)
                if "sourceBuses" not in definition:
                    continue
                if type(definition["sourceBuses"]) is list:
                    if bus not in definition["sourceBuses"]:
                        return

                else:
                    if definition["sourceBuses"] not in can_bus_defs:
                        raise Exception(
                            f"Source bus {definition['sourceBuses']} is not defined in the network."
                        )
            except Exception as e:
                print(
                    f"CAN message fuck reception definition for '{msg}' in node '{node.name}' is invalid."
                )
                print(f"CAN RX Message Error: {e}")
                error = True
                continue
    if error:
        ERROR = True
        raise Exception("Error processing node receivers, see previous errors...")

    for sig_name in rx_sig_dict.keys():
        if rx_sig_dict[sig_name] and (
            "sourceBuses" in rx_sig_dict[sig_name]
            and rx_sig_dict[sig_name]["sourceBuses"] != bus.name
        ):
            continue
        if sig_name not in bus.signals:
            print(
                f"Node '{node.alias}' attempted to RX signal '{sig_name}' "
                f"on bus {bus.name}, but that signal is not present on that bus"
            )
            ERROR = True
            continue

        rxed_sig = bus.signals[sig_name]
        rxed_msg = rxed_sig.message_ref

        if (
            rxed_msg.checksum_sig
            and not rxed_msg.checksum_sig.name in rx_sig_dict.keys()
            and not rxed_msg.checksum_sig.name in node.received_sigs
        ):
            node.received_sigs[rxed_msg.checksum_sig.name] = rxed_msg.checksum_sig
            rxed_msg.add_receiver(node, rxed_msg.checksum_sig.name)

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

        if (
            msg_name not in bus.messages.keys()
            or bus.name not in bus.messages[msg_name].source_buses
        ):
            print(
                f"Node '{node.alias}' attempted to RX message '{msg_name}' "
                f"on bus {bus.name}, but that message is not present on that bus"
            )
            ERROR = True
            continue

        if rx_msg_dict[msg_name] is not None:
            if "node" in rx_msg_dict[msg_name]:
                if node.duplicateNode is False:
                    Exception(
                        f"{msg_name} in file {rx_file} is for node {rx_msg_dict[msg_name]['node']}, however {node.alias} is not a duplicate node."
                    )
                if rx_msg_dict[msg_name]["node"] is not node.offset:
                    continue

        if msg_name not in node.received_msgs:
            node.received_msgs[msg_name] = bus.messages[msg_name]

            if rx_msg_dict[msg_name] is not None:
                if "unrecorded" in rx_msg_dict[msg_name]:
                    continue

        if rx_msg_dict[msg_name] is not None and "unrecorded" in rx_msg_dict[msg_name]:
            continue

        for sig_name in bus.messages[msg_name].signals:
            rxed_sig = bus.signals[sig_name]

            if (
                rxed_sig not in node.received_sigs
                and not rxed_sig.message_ref.fault_message
            ):
                node.received_sigs[sig_name] = rxed_sig
                bus.messages[msg_name].add_receiver(node, rxed_sig.name)

    if bus.name in rx_all_buses:
        if bus.name not in node.on_buses:
            print(
                f"Node '{node.alias}' attempted to RX all messages on bus '{bus.name}', but '{node.alias}' is not present on that bus"
            )
            ERROR = True
            return
        for msg in bus.messages.values():
            if bus.name not in msg.source_buses:
                continue
            add_message_and_signals(msg)


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


def codegen(
    mako_lookup: TemplateLookup,
    nodes: Iterator[Tuple[str, Path]],
    rust_codegen: bool = False,
):
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
            ["YamcanShared.h.mako", {"nodes": [can_nodes[node]]}],
            [
                "NetworkDefines_generated.h.mako",
                {"nodes": [can_nodes[node]], "buses": can_bus_defs, "network": {}},
            ],
            ["CANTypes_generated.h.mako", {"nodes": [can_nodes[node]]}],
            ["SigTx.h.mako", {"nodes": [can_nodes[node]]}],
            ["SigRx.h.mako", {"nodes": [can_nodes[node]]}],
            ["TemporaryStubbing.h.mako", {"nodes": [can_nodes[node]]}],
        ]
        if rust_codegen:
            makos += [
                ["rust_model_generated.rs.mako", {"nodes": [can_nodes[node]]}],
                [
                    "rust_decode_generated.rs.mako",
                    {"nodes": [can_nodes[node]], "can_bus_defs": can_bus_defs},
                ],
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
        "--gen-dbc",
        dest="dbc",
        action="store_true",
        help="Generate DBC files.",
    )

    parser.add_argument(
        "--gen-stats",
        dest="stats",
        action="store_true",
        help="Generate stats files.",
    )

    parser.add_argument(
        "--cache-dir",
        type=Path,
        dest="cache_dir",
        action="store",
        help="Directory where generated network files will be placed",
    )

    parser.add_argument(
        "--rust-codegen",
        dest="rust_codegen",
        action="store_true",
        help="Generate additional code for can-bridge integration.",
    )

    parser.add_argument(
        "--manifest-output",
        dest="manifest",
        type=str,
        action="store",
        help="Generate network manifest.",
    )

    parser.add_argument(
        "--manifest-bus",
        dest="manifest_bus",
        type=str,
        action="store",
        help="Bus to generate the network manifest for.",
    )

    parser.add_argument(
        "--manifest-filter",
        dest="filters",
        action="append",
        type=str,
        help="Name of a message (or substring of the name) to include in the node manifest.",
    )
    parser.add_argument(
        "--ignore-node",
        dest="ignore_nodes",
        action="append",
        type=str,
        help="Name of the node to ignore from the manifest file.",
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

    for node in can_nodes.values():
        process_forwarding(node)

    for node in can_nodes.values():
        process_bridges(node)

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

    makedirs(output_dir, exist_ok=True)
    output = [open(str(output_dir) + f"/{bus.name}-stats.txt", "w"), sys.stdout]

    for _, message in bus.messages.items():
        # SOF(1), RTR(1), IDE(1), DLC(4), CRC(15)
        # DEL(1), ACK(1), DEL(1), EOF(7), ITM(11)
        if bus.name not in message.source_buses:
            continue

        bit_length = 43
        bit_length += message.length_bytes * 8
        bit_length += 11 if message.id <= 0x7FF else 29

        if message.unscheduled:
            total_unscheduled_bits += bit_length
            total_unscheduled_messages += 1
            continue
        total_messages += 1
        messages_per_s = 1000 / message.cycle_time_ms

        total_bits_per_s += bit_length * messages_per_s
        total_messages_per_s += messages_per_s

    usage = 0 if bus.interface_type == "virtual" else total_bits_per_s / bus.baudrate
    for out in output:
        if bus.interface_type == "virtual":
            print(
                f"CAN Bus Statistics for '{bus.name.upper()}'; Interface Type: virtual; Usage: n/a",
                file=out,
            )
        else:
            print(
                f"CAN Bus Statistics for '{bus.name.upper()}'; Baudrate: {bus.baudrate}; Usage: {usage * 100}%",
                file=out,
            )
        print(
            f"Total Bits Transmitted/s: {int(total_bits_per_s)}; Total Messages Transmitted/s: {int(total_messages_per_s)}",
            file=out,
        )
        if total_unscheduled_messages > 0:
            print(
                f"Total Bits Unscheduled: {int(total_unscheduled_bits)}; Total Messages Unscheduled: {int(total_unscheduled_messages)}",
                file=out,
            )
        print(
            f"Total Nodes: {len(bus.nodes)}; Count Messages: {total_messages}", file=out
        )

    if bus.interface_type != "virtual" and usage > 0.8:
        print(f"Warning: CANBus '{bus.name.upper()}' has high bus load")
    if bus.interface_type != "virtual" and usage > 1:
        raise Exception(f"CAN Bus '{bus.name.upper()}' has total bus usage of {usage}")


def generate_manifest(
    bus: str, manifest: str, filters: list[str], ignore_nodes: list[str]
):
    nodes: dict[str, dict[str, int]] = {}

    # ---- preprocess filters ----
    # specific: "node:filter" or "node:filter=alt"
    # general:  "filter" or "filter=alt"
    specific: dict[str, list[tuple[str, str | None]]] = {}
    general: list[tuple[str, str | None]] = []

    def parse(tok: str) -> tuple[str | None, str, str | None]:
        node = None
        filt = tok
        if ":" in tok:
            node, filt = tok.split(":", 1)
        alt = None
        if "=" in filt:
            filt, alt = filt.split("=", 1)
        return node, filt, alt

    for tok in filters:
        node, filt, alt = parse(tok)
        if node is None:
            general.append((filt, alt))
        else:
            specific.setdefault(node, []).append((filt, alt))

    def add_mapping(node_alias: str, filt: str, out_name: str, msg_id: int):
        if node_alias not in nodes:
            nodes[node_alias] = {}

        # Ignore exact duplicate inserts (can happen when same filter is applied twice)
        if out_name in nodes[node_alias]:
            if nodes[node_alias][out_name] == msg_id:
                return

            print(
                f"Signal filter '{filt}' has multiple matches in node '{node_alias}'."
            )
            print("Consider using a 'node:filter=alt' format.")
            exit(1)

        nodes[node_alias][out_name] = msg_id

    def apply_filter_list(
        node_alias: str,
        messages: dict[str, object],
        flist: list[tuple[str, str | None]],
        *,
        fatal_on_ambiguous: bool,
    ):
        # Apply per-filter so ambiguity can be resolved correctly
        for filt, alt in flist:
            matches = [(msg, defs) for msg, defs in messages.items() if filt in msg]
            if not matches:
                continue

            if len(matches) == 1:
                chosen_msg, chosen_defs = matches[0]
            else:
                # Prefer exact match when multiple substring matches exist
                exact = [(msg, defs) for msg, defs in matches if msg == filt]
                if len(exact) == 1:
                    chosen_msg, chosen_defs = exact[0]
                else:
                    if fatal_on_ambiguous:
                        print(
                            f"Signal filter '{filt}' has multiple matches in node '{node_alias}'."
                        )
                        print("Consider using a 'node:filter=alt' format.")
                        exit(1)
                    # Generic filter is ambiguous for this node: skip it.
                    continue

            out_name = alt or filt
            add_mapping(node_alias, filt, out_name, chosen_defs.id)

    # ---- build manifest ----
    for can_node_name, node_def in can_nodes.items():
        if can_node_name in ignore_nodes:
            continue

        node_alias = node_def.alias
        messages = {**node_def.messages, **node_def.received_msgs}

        # Collect node-specific filters (avoid double-adding when name == alias)
        spec_list: list[tuple[str, str | None]] = []
        spec_list.extend(specific.get(can_node_name, []))
        if node_alias != can_node_name:
            spec_list.extend(specific.get(node_alias, []))

        # Apply node-specific filters first (fatal if ambiguous)
        if spec_list:
            apply_filter_list(node_alias, messages, spec_list, fatal_on_ambiguous=True)

        # Apply general filters (non-fatal if ambiguous)
        apply_filter_list(node_alias, messages, general, fatal_on_ambiguous=False)

    with open(manifest, "w") as fd:
        yaml.dump({"nodes": nodes}, fd, default_flow_style=False)


def main():
    """Main function"""
    global ERROR
    global can_nodes
    global can_bus_defs
    global discrete_values
    global templates

    # parse arguments
    args = parse_args()
    lookup = TemplateLookup(directories=[path.dirname(__file__) + "/templates"])

    if args.build:
        if args.definition_dir:
            print(f"Parsing network...")
            parseNetwork(args, lookup)
            data = {}
            data["can_nodes"] = can_nodes
            data["can_bus_defs"] = can_bus_defs
            data["discrete_values"] = discrete_values
            data["templates"] = templates
            for node in can_nodes.values():
                if not node.offset:
                    for message in node.messages.values():
                        message.crc = message.crc32()
                else:
                    for name, message in node.messages.items():
                        base_message = (
                            name.split("_")[0].rstrip(str(node.offset))
                            + str(0)
                            + "_{}".format(name.split("_")[1])
                        )
                        message.crc = (
                            can_nodes[node.name + "_" + str(0)]
                            .messages[base_message]
                            .crc
                        )
            if args.cache_dir:
                makedirs(args.cache_dir, exist_ok=True)
                pickle.dump(
                    data, open(args.cache_dir.joinpath("CachedNetwork.pickle"), "wb")
                )
    elif args.cache_dir:
        try:
            data = pickle.load(
                open(args.cache_dir.joinpath("CachedNetwork.pickle"), "rb")
            )
            can_nodes = data["can_nodes"]
            can_bus_defs = data["can_bus_defs"]
            discrete_values = data["discrete_values"]
            templates = data["templates"]
        except Exception as e:
            print(f"Could not retreive cache files. Try building the network again...")
            ERROR = True

    if args.manifest:
        if len(args.filters) == 0:
            print("No manifest filters specified!")
            exit(1)
        generate_manifest(
            args.manifest_bus, args.manifest, args.filters, args.ignore_nodes
        )

    if args.output_dir:
        for bus in can_bus_defs.values():
            if (not args.dbc and not args.stats) or args.stats:
                calculate_bus_load(bus, args.output_dir)
            if (not args.dbc and not args.stats) or args.dbc:
                generate_dbcs(lookup, bus, args.output_dir)

    if ERROR:
        raise Exception("Build failed. See previous errors in output")

    if args.node:
        codegen(lookup, zip(args.node, args.codegen_dir), args.rust_codegen)


if __name__ == "__main__":
    main()
