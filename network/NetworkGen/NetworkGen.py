from pathlib import Path
from pprint import pprint
from os import makedirs
from yaml import load, Loader
from mako.template import Template


from classes.Can import CanBus, CanNode, CanSignal, CanMessage, discrete_values
from classes.Types import *

REPO_ROOT_DIR = Path("../../")
NETWORK_DIR = REPO_ROOT_DIR.joinpath("network/definition")
BUS_DEF_DIR = NETWORK_DIR.joinpath("buses")
NODE_DEF_DIR = NETWORK_DIR.joinpath("data/components")
NODE_DIRS = [dir for dir in NODE_DEF_DIR.iterdir() if dir.is_dir()]
OUTPUT_DIR = REPO_ROOT_DIR.joinpath("generated/dbc")

ACCEPTED_YAML_FILES = ["message.yaml", "rx.yaml", "signals.yaml"]

can_buses = BUS_DEF_DIR.glob("*.yaml")
can_bus_defs = {}
can_nodes = {}


# if this gets set during the build, we will fail at the end
ERROR = False


def generate_discrete_values() -> None:
    """Load discrete values from discrete-values.yaml"""
    with open(
        NETWORK_DIR.joinpath("discrete_values.yaml"), "r"
    ) as discrete_values_file:
        discrete_values_dict = load(discrete_values_file, Loader)

    for discrete_value_name, discrete_value in discrete_values_dict.items():
        max_val = max(discrete_value.values())
        bit_width = max_val.bit_length()
        discrete_value_obj = DiscreteValue(discrete_value_name, discrete_value)
        discrete_value_obj.max_val = max_val
        discrete_value_obj.bit_width = bit_width
        setattr(discrete_values, discrete_value_name, discrete_value_obj)


def generate_can_buses() -> None:
    """Generate CAN buses based on yaml files"""
    for bus_file_path in can_buses:
        with open(bus_file_path, "r") as bus_file:
            can_bus_def = load(bus_file, Loader)
            try:
                can_bus_defs[can_bus_def["name"]] = CanBus(can_bus_def)
            except Exception as e:
                # FIXME: except the specific exception we're expecting here
                raise e


def generate_can_nodes() -> None:
    """
    Generate CAN nodes based on yaml files
    """
    global ERROR
    for node in NODE_DIRS:
        node_obj = {"name": node.name, "def_files": {}}
        node_files = node.glob("*.yaml")

        for node_file in node_files:
            if not node_file.name in [
                f"{node.name}-{suffix}" for suffix in ACCEPTED_YAML_FILES
            ] + [f"{node.name}.yaml"]:
                print(f"Unexpected file in {node.name} node dir: {node_file.name}")
                ERROR = True
            else:
                node_obj["def_files"][node_file.name] = node_file

        if not f"{node.name}.yaml" in node_obj["def_files"]:
            print(f"Missing node config file {node.name}.yaml")
            node_obj["skip"] = True
            # ERROR = True

        message_file = f"{node.name}-message.yaml" in node_obj["def_files"]
        signals_file = f"{node.name}-signals.yaml" in node_obj["def_files"]

        if (message_file and not signals_file) or (not message_file and signals_file):
            print(
                "You must either provide both a message and signals file, "
                f"or neither for node {node.name}"
            )
            ERROR = True

        if "skip" in node_obj:
            continue

        with open(node_obj["def_files"][f"{node.name}.yaml"], "r") as node_file:
            node_def = load(node_file, Loader)
            node_def.update(node_obj)
        # create one object and add it to all buses so that each bus will have the same object
        # that way if we modify it in one place it will apply to all buses

        can_node = CanNode(node.name, node_def)

        can_node.on_buses = node_def["onBuses"]
        can_nodes[node.name] = can_node


def process_node(node: CanNode):
    """Process the signals and messages associated with a given CAN node"""
    global ERROR
    if f"{node.name}-signals.yaml" in node.def_files:
        with open(node.def_files[f"{node.name}-signals.yaml"], "r") as signals_file:
            signals_dict = load(signals_file, Loader)["signals"]

        signals = {}

        if not signals_dict is None:
            for signal in signals_dict:
                sig_name = f"{node.name.upper()}_{signal}"
                sig_obj = CanSignal(sig_name, signals_dict[signal])
                signals[sig_obj.name] = sig_obj

        with open(node.def_files[f"{node.name}-message.yaml"], "r") as messages_file:
            messages_dict = load(messages_file, Loader)["messages"]

        if messages_dict is None:
            messages_dict = {}

        for message, msg in messages_dict.items():
            msg_name = f"{node.name.upper()}_{message}"
            msg_obj = CanMessage(msg_name, msg)
            if msg_obj.signals is None:
                print(f"No signals were defined for message {msg_obj.name}")
                continue
            for msg_signal in msg_obj.signals:
                if msg_signal in signals:
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
            node.add_message(msg_obj)

        for signal, _ in signals.items():
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
    rx_file_name = f"{node.name}-rx.yaml"
    if rx_file_name in node.def_files:
        with open(node.def_files[rx_file_name], "r") as rx_file:
            # making this a dictionary because eventually we'll
            # probably want to be able to gateway messages from
            # one bus to another, and this would be the place to
            # define the gateway node
            rx_dict = load(rx_file, Loader)["signals"]
            rx_dict = {} if rx_dict is None else rx_dict

        for sig_name, _ in rx_dict.items():
            if not sig_name in bus.signals:
                print(
                    f"Node '{node.name}' attempted to RX signal '{sig_name}' "
                    f"on bus {bus.name}, but that signal is not present on that bus"
                )
            else:
                rxed_sig = bus.signals[sig_name]
                rxed_msg = rxed_sig.message_ref

                node.received_sigs[sig_name] = rxed_sig
                node.received_msgs[rxed_msg.name] = rxed_msg

                rxed_msg.add_receiver(node, rxed_sig.name)


def generate_dbcs(bus: CanBus):
    dbc = Template(filename="dbc_template/template.dbc.mako").render(
        bus=bus, messages=bus.messages.values()
    )
    makedirs(OUTPUT_DIR, exist_ok=True)
    with open(OUTPUT_DIR.joinpath(f"{bus.name}.dbc"), "w") as dbc_handle:
        dbc_handle.write(dbc)
    print(dbc)


def main():
    """Main function"""
    generate_discrete_values()
    generate_can_buses()
    generate_can_nodes()

    for node in can_nodes.values():
        process_node(node)

    for bus in can_bus_defs.values():
        for node in [node for node in bus.nodes.values() if not node.processed]:
            process_receivers(bus, node)

    # print(can_bus_defs)

    if ERROR:
        raise Exception("Build failed. See previous errors in output")

    # print(can_bus_defs["veh"].nodes)
    for bus in can_bus_defs.values():
        generate_dbcs(bus)


if __name__ == "__main__":
    main()
