from argparse import ArgumentParser, Namespace
from os import makedirs
from pathlib import Path
from typing import Dict, Iterator, Tuple
import pickle

from mako.lookup import TemplateLookup
from oyaml import safe_load

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

# if this gets set during the build, we will fail at the end
ERROR = False


def generate_discrete_values(data_dir: Path) -> None:
    """Load discrete values from discrete-values.yaml"""
    global ERROR
    global discrete_values
    discrete_values = DiscreteValues()

    with open(data_dir.joinpath("discrete_values.yaml"), "r") as fd:
        dvs: Dict[str, Dict[str, int]] = safe_load(fd)

    for name, val in dvs.items():
        try:
            discrete_value = DiscreteValue(name, val)
            setattr(discrete_values, name, discrete_value)
        except ValueError as e:
            print(e)
            ERROR = True


def generate_can_buses(data_dir: Path) -> None:
    """Generate CAN buses based on yaml files"""
    can_buses = data_dir.joinpath("buses").glob("*.yaml")
    for bus_file_path in can_buses:
        with open(bus_file_path, "r") as bus_file:
            can_bus_def = safe_load(bus_file)
        try:
            can_bus_defs[can_bus_def["name"]] = CanBus(can_bus_def)
        except Exception as e:
            # FIXME: except the specific exception we're expecting here
            raise e


def generate_can_nodes(data_dir: Path) -> None:
    """
    Generate CAN nodes based on yaml files
    """
    global ERROR

    nodes = [
        dir for dir in data_dir.joinpath("data/components").iterdir() if dir.is_dir()
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



def process_node(node: CanNode):
    """Process the signals and messages associated with a given CAN node"""
    global ERROR

    sig_file = SIG_FILE.format(name=node.name)
    msg_file = MESSAGE_FILE.format(name=node.name)

    if sig_file not in node.def_files:
        return

    with open(node.def_files[sig_file], "r") as signals_file:
        signals_dict = safe_load(signals_file)["signals"]

    signals = {}

    if not signals_dict:
        print(f"No signals found in signal file for node '{node.name}'")
        ERROR = True
        return

    for signal in signals_dict:
        if node.duplicateNode:
            sig_name = f"{node.name.upper()}{node.offset}_{signal}"
        else:
            sig_name = f"{node.name.upper()}_{signal}"
        sig_obj = CanSignal(sig_name, signals_dict[signal])
        signals[sig_obj.name] = sig_obj

    with open(node.def_files[msg_file], "r") as messages_file:
        messages_dict = safe_load(messages_file).get("messages", None)

    if not messages_dict:
        print(f"No messages found in message file for node '{node.name}'")
        ERROR = True
        return


    for name, definition in messages_dict.items():
        if node.duplicateNode:
            msg_name = f"{node.name.upper()}{node.offset}_{name}"
        else:
            msg_name = f"{node.name.upper()}_{name}"

        definition["id"] = definition["id"] + node.offset;
        
        for existing_node in can_nodes:
            for msg in can_nodes[existing_node].messages:
                if definition["id"] == can_nodes[existing_node].messages[msg].id:
                    print(f"Message {msg_name} has the same ID as {msg}")
                    ERROR = True
        msg_obj = CanMessage(node, msg_name, definition)

        if msg_obj.signals is None:
            print(f"No signals were defined for message {msg_obj.name}")
            ERROR = True
            continue

        for msg_signal in msg_obj.signals:
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
        msg_obj.validate_msg()
        if not msg_obj.is_valid:
            ERROR = True
        node.add_message(msg_obj)

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

    rx_file = RX_FILE.format(name=node.name)
    if rx_file not in node.def_files:
        print(f"Note: rx file not found for node '{node.name}'")
        return

    with open(node.def_files[rx_file], "r") as fd:
        # making this a dictionary because eventually we'll
        # probably want to be able to gateway messages from
        # one bus to another, and this would be the place to
        # define the gateway node
        rx_sig_dict = safe_load(fd)["signals"] or {}

    with open(node.def_files[rx_file], "r") as fd:
        rx_msg_dict = safe_load(fd)["messages"] or {}

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
        if msg_name not in bus.messages.keys():
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
            ["NetworkDefines_generated.h.mako", {"nodes": [can_nodes[node]]}],
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
        dest="data_dir",
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
    generate_discrete_values(args.data_dir)
    generate_can_buses(args.data_dir)
    generate_can_nodes(args.data_dir)

    for node in can_nodes.values():
        process_node(node)

    for bus in can_bus_defs.values():
        for node in bus.nodes.values():
            if not node.processed:
                process_receivers(bus, node)

    generate_dbcs(lookup, bus, args.output_dir)


def main():
    """Main function"""
    global ERROR
    global can_nodes
    global can_bus_defs
    global discrete_values

    # parse arguments
    args = parse_args()
    lookup = TemplateLookup(directories=[args.data_dir.joinpath("templates")])

    if args.build:
        print(f"Parsing network...")
        parseNetwork(args, lookup)

        for bus in can_bus_defs.values():
            generate_dbcs(lookup, bus, args.output_dir)
        if args.cache_dir:
            pickle.dump(can_nodes, open(args.cache_dir.joinpath("CachedNodes.pickle"), "wb"))
            pickle.dump(can_bus_defs, open(args.cache_dir.joinpath("CachedBusDefs.pickle"), "wb"))
            pickle.dump(discrete_values, open(args.cache_dir.joinpath("CachedDiscreteValues.pickle"), "wb"))
    elif args.cache_dir:
        try:
            can_nodes = pickle.load(open(args.cache_dir.joinpath("CachedNodes.pickle"), "rb"))
            can_bus_defs = pickle.load(open(args.cache_dir.joinpath("CachedBusDefs.pickle"), "rb"))
            discrete_values = pickle.load(open(args.cache_dir.joinpath("CachedDiscreteValues.pickle"), "rb"))
        except Exception as e:
            print(f"Could not retreive cache files. Try building the network again...")
            ERROR = True

    if ERROR:
        raise Exception("Build failed. See previous errors in output")

    if args.node:
        codegen(lookup, zip(args.node, args.codegen_dir))


if __name__ == "__main__":
    main()
