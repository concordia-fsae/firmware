from os import walk
from typing import Dict, Optional

from SCons.Script import Action, Builder, Dir, Flatten, Glob

def recursive_glob(dir, glob):
    ret = []
    for root, dirnames, _ in walk(dir.abspath):
        ret.extend([Glob(f"{dir}/{glob}") for dir in [root] + dirnames])
    return Flatten(ret)


def build_network(env):
    env._networkBuilder()
    return [env["NETWORK_OUTPUT_DIR"].File(f"{file.name.split('.')[0]}.dbc") for file in recursive_glob(env["NETWORK_DATA_DIR"].Dir("buses"), "*.yaml")]


def generate_nodes(env, nodes: Optional[Dict[str, Dir]], ):
    codegen_args = ""
    codegen_args = " ".join(
        [
            f"--node {node} --codegen-dir {dir.abspath}"
            for node, dir in nodes.items()
        ]
    )

    env._generateNodes(nodes=nodes, codegen_args=codegen_args)
    return ["MessageUnpack_generated.c"]


def emitBuild(target, source, env):
    buses = [file.name.split('.')[0] for file in recursive_glob(env["NETWORK_DATA_DIR"].Dir("buses"), "*.yaml")]
    source.extend(recursive_glob(env["NETWORK_PATH"], "*.py"))
    source.extend(recursive_glob(env["NETWORK_DATA_DIR"], "*.yaml"))
    target.append([ env["NETWORK_OUTPUT_DIR"].File(f"{bus}.dbc") for bus in buses ])
    target.append([ env["NETWORK_OUTPUT_DIR"].File(f"{bus}-stats.txt") for bus in buses ])
    target.append(env["NETWORK_CACHE_DIR"].File("CachedNodes.pickle"))
    target.append(env["NETWORK_CACHE_DIR"].File("CachedBusDefs.pickle"))
    target.append(env["NETWORK_CACHE_DIR"].File("CachedDiscreteValues.pickle"))
    target.append(env["NETWORK_CACHE_DIR"].File("CachedTemplates.pickle"))
    return (target, source)

def emitGen(target, source, env):
    source.extend(recursive_glob(env["NETWORK_PATH"], "*.mako"))
    source.append(env["NETWORK_CACHE_DIR"].File("CachedNodes.pickle"))
    source.append(env["NETWORK_CACHE_DIR"].File("CachedBusDefs.pickle"))
    source.append(env["NETWORK_CACHE_DIR"].File("CachedDiscreteValues.pickle"))
    source.append(env["NETWORK_CACHE_DIR"].File("CachedTemplates.pickle"))
    for _, dir in env["nodes"].items():
        target.extend(
            [
                dir.File("MessagePack_generated.c"),
                dir.File("MessagePack_generated.h"),
                dir.File("MessageUnpack_generated.c"),
                dir.File("MessageUnpack_generated.h"),
                dir.File("NetworkDefines_generated.h"),
                dir.File("CANTypes_generated.h"),
                dir.File("SigTx.c"),
                dir.File("SigRx.h"),
            ]
        )
    return (target, source)


def generate(env):
    env.Replace(
        NETWORK_TOOL_PATH=env["NETWORK_PATH"].File("NetworkGen/NetworkGen.py"),
        NETWORK_DATA_DIR=env["NETWORK_DATA_DIR"],
        NETWORK_OUTPUT_DIR=env["NETWORK_ARTIFACTS"],
        NETWORK_CACHE_DIR=env["NETWORK_DATA_DIR"].Dir("cache"),
        NETWORK="python $NETWORK_TOOL_PATH --data-dir $NETWORK_DATA_DIR --output-dir $NETWORK_OUTPUT_DIR",
        NETWORKBUILDCOMSTR="Building Network with command '$NETWORK --build --cache-dir $NETWORK_CACHE_DIR'",
        NETWORKNODEGENCOMSTR="Generating Node with command '$NETWORK --cache-dir $NETWORK_CACHE_DIR $codegen_args'",
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
