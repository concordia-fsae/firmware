from os import walk
from typing import Dict, Optional

from SCons.Script import Action, Builder, Dir, Flatten, Glob

NETWORK_PATH = Dir("#/network")
NETWORK_TOOL_PATH = NETWORK_PATH.File("NetworkGen/NetworkGen.py")
NETWORK_DATA_DIR = NETWORK_PATH.Dir("definition")
NETWORK_OUTPUT_DIR = Dir("#/generated")


def recursive_glob(dir, glob):
    ret = []
    for root, dirnames, _ in walk(dir.abspath):
        ret.extend([Glob(f"{dir}/{glob}") for dir in [root] + dirnames])
    return Flatten(ret)


def build_network(env, nodes: Optional[Dict[str, Dir]] = None):
    codegen_args = ""
    if nodes:
        codegen_args = " ".join(
            [
                f"--node {node} --codegen-dir {dir.abspath}"
                for node, dir in nodes.items()
            ]
        )
    env._networkBuilder(nodes=nodes, codegen_args=codegen_args)


def emit(target, source, env):
    source.extend(recursive_glob(NETWORK_PATH, "*.py"))
    source.extend(recursive_glob(NETWORK_DATA_DIR, "*.yaml"))
    source.extend(recursive_glob(NETWORK_DATA_DIR, "*.mako"))
    target.append(env["NETWORK_OUTPUT_DIR"].File("dbcs/veh.dbc"))
    for _, dir in env["nodes"].items():
        target.extend(
            [dir.File("MessagePack_generated.c"), dir.File("MessagePack_generated.h")]
        )
    return (target, source)


def generate(env):
    env.Replace(
        NETWORK_TOOL_PATH=NETWORK_TOOL_PATH,
        NETWORK_DATA_DIR=NETWORK_DATA_DIR,
        NETWORK_OUTPUT_DIR=NETWORK_OUTPUT_DIR,
        NETWORK="python $NETWORK_TOOL_PATH $NETWORK_DATA_DIR $NETWORK_OUTPUT_DIR",
        NETWORKCOMSTR="Building Network with command '$NETWORK $codegen_args'",
    )

    env["BUILDERS"]["_networkBuilder"] = Builder(
        action=Action(
            "$NETWORK $codegen_args",
            cmdstr="$NETWORKCOMSTR",
        ),
        emitter=emit,
    )

    env.AddMethod(build_network, "BuildNetwork")


def exists():
    return True
