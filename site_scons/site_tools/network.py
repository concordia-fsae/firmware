from os import walk
from typing import Dict, Optional

from SCons.Script import Action, Builder, Dir, Flatten, Glob

NETWORK_PATH = Dir("#/network")
NETWORK_TOOL_PATH = NETWORK_PATH.File("NetworkGen/NetworkGen.py")
NETWORK_DATA_DIR = NETWORK_PATH.Dir("definition")
NETWORK_OUTPUT_DIR = Dir("#/generated")
NETWORK_CACHE_DIR = NETWORK_PATH.Dir("cache")

def recursive_glob(dir, glob):
    ret = []
    for root, dirnames, _ in walk(dir.abspath):
        ret.extend([Glob(f"{dir}/{glob}") for dir in [root] + dirnames])
    return Flatten(ret)


def build_network(env):
    env._networkBuilder()
    return [env["NETWORK_OUTPUT_DIR"].File("dbcs/veh.dbc")]


def generate_nodes(env, nodes: Optional[Dict[str, Dir]] = None):
    codegen_args = ""
    if nodes:
        codegen_args = " ".join(
            [
                f"--node {node} --codegen-dir {dir.abspath}"
                for node, dir in nodes.items()
            ]
        )

    env._generateNodes(nodes=nodes, codegen_args=codegen_args)


def emitBuild(target, source, env):
    source.extend(recursive_glob(NETWORK_PATH, "*.py"))
    source.extend(recursive_glob(NETWORK_DATA_DIR, "*.yaml"))
    source.extend(recursive_glob(NETWORK_DATA_DIR, "*.mako"))
    target.append(env["NETWORK_OUTPUT_DIR"].File("dbcs/veh.dbc"))
    target.append(env["NETWORK_CACHE_DIR"].File("CachedNodes.pickle"))
    target.append(env["NETWORK_CACHE_DIR"].File("CachedBusDefs.pickle"))
    return (target, source)

def emitGen(target, source, env):
    source.append(env["NETWORK_CACHE_DIR"].File("CachedNodes.pickle"))
    source.append(env["NETWORK_CACHE_DIR"].File("CachedBusDefs.pickle"))
    for _, dir in env["nodes"].items():
        target.extend(
            [
                dir.File("MessagePack_generated.c"),
                dir.File("MessagePack_generated.h"),
                dir.File("SigTx.c"),
            ]
        )
    return (target, source)


def generate(env):
    env.Replace(
        NETWORK_TOOL_PATH=NETWORK_TOOL_PATH,
        NETWORK_DATA_DIR=NETWORK_DATA_DIR,
        NETWORK_OUTPUT_DIR=NETWORK_OUTPUT_DIR,
        NETWORK_CACHE_DIR=NETWORK_CACHE_DIR,
        NETWORK="python $NETWORK_TOOL_PATH --data-dir $NETWORK_DATA_DIR --output-dir $NETWORK_OUTPUT_DIR",
        NETWORKBUILDCOMSTR="Building Network with command '$NETWORKBUILD --build --cache-dir $NETWORK_CACHE_DIR'",
        NETWORKNODEGENCOMSTR="Generating Node with command '$NETWORKCODEGEN --cache-dir $NETWORK_CACHE_DIR $codegen_args'",
    )

    env["BUILDERS"]["_networkBuilder"] = Builder(
        action=Action(
            "$NETWORK --build --cache-dir $NETWORK_CACHE_DIR",
            cmdstr="$NETWORKBUILDCOMSTR",
        ),
        emitter=emitBuild,
    )
    env["BUILDERS"]["_generateNodes"] = Builder(
        action=Action(
            "$NETWORK --cache-dir $NETWORK_CACHE_DIR $codegen_args",
            cmdstr="$NETWORKNODEGENCOMSTR",
        ),
        emitter=emitGen,
    )

    env.AddMethod(build_network, "BuildNetwork")
    env.AddMethod(generate_nodes, "GenerateNodes")


def exists():
    return True
