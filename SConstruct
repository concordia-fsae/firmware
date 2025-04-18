#!/usr/bin/python3

from os import environ
from re import compile, findall, search

from SCons.Node import NodeList
from SCons.Script import (
    AddOption,
    Default,
    Dir,
    Environment,
    Exit,
    GetOption,
    SConscript,
    File
)
from oyaml import safe_load

# create a global environment which all targets can start from
GlobalEnv = Environment(REPO_ROOT_DIR=Dir("#"), tools=[ "tar" ])
try:
    GlobalEnv["ENV"]["TERM"] = environ["TERM"]
except Exception:
    print("No terminal environment specified...")

# add option to choose target or platforam
AddOption("--targets", dest="targets", type="string", action="store")
AddOption("--platform", dest="platform", type="string", action="store")
# add option to make build verbose
AddOption("--verbose", dest="verbose", action="store_true")
# add option to build without debug info
AddOption("--release", dest="release", action="store_true")

components = None
platforms = None

with open("site_scons/components.yaml") as components_file:
    components = safe_load(components_file)
with open("site_scons/platforms.yaml") as platforms_file:
    platforms = safe_load(platforms_file)


COMPONENT_REGEX = compile(r"(?P<component>[A-Za-z]+)")
CONFIG_ID_REGEX = compile(r"(?:(\d+),?)")

network_dirs = {}

targets = GetOption("targets")
platform = GetOption("platform")
target_dict = {}
PlatformEnv = GlobalEnv.Clone()
NetworkEnv = PlatformEnv.Clone()
PlatformEnv["ARTIFACTS"] = {}
NetworkEnv["NETWORK_PATH"] = Dir("#/network")
NetworkEnv["NETWORK_DATA_DIR"] = NetworkEnv["NETWORK_PATH"].Dir("definition")
NetworkEnv["NETWORK_ARTIFACTS"] = GlobalEnv["REPO_ROOT_DIR"].Dir(f"platform-artifacts/network")

if platform:
    if platform in platforms["configs"]:
        PlatformEnv["PLATFORM_ID"] = platform
        PlatformEnv["PLATFORM_ARTIFACTS"] = GlobalEnv["REPO_ROOT_DIR"].Dir(f"platform-artifacts/{platform.upper()}")
        if targets:
            for target in targets.split("+"):
                component = search(COMPONENT_REGEX, target).group()
                config_ids = [int(id) for id in findall(CONFIG_ID_REGEX, target)]
                if component in platforms["configs"][platform]["ecu"]:
                    if len(config_ids) == 0:
                        target_dict[component] = platforms["configs"][platform]["ecu"][component]
                    else:
                        target_dict[component] = []
                        for config in platforms["configs"][platform]["ecu"][component].keys():
                            if config in config_ids:
                                target_dict[component].append(config)
                else:
                    print(
                            f"Platform string '{platform}' does not contain any ecu named '{component}'"
                    )
                    Exit(1)
        else:
            for part in platforms["configs"][platform]["ecu"]:
                target_dict[part] = platforms["configs"][platform]["ecu"][part]
    else:
        print(
                f"Platform string '{platform}' does not match any platform in 'site_scons/platforms.yaml'"
        )
        Exit(1)

elif targets:
    targets = targets.split('+')
    for target in targets:
        target_dict[target] = []

NetworkEnv.Tool("network")
dbc = NetworkEnv.BuildNetwork()
PlatformEnv["ARTIFACTS"]["DBC"] = dbc
Default(dbc)

multiple_targets = False

if len(target_dict) > 1:
    multiple_targets = True

for target in target_dict:
    component = search(COMPONENT_REGEX, target)
    if not component:
        print(
            f"Target string '{target}' does not match the required format of '<component name1>:[<config id1>[,<config id2>[,...]]]'"
        )
        Exit(1)

    if component["component"] not in components:
        print(f"Unknown target '{target}'. See site_scons/components.yaml")
        Exit(1)

    config_ids = []
    if platform:
        config_ids = target_dict[target]
    else:
        config_ids = [int(id) for id in findall(CONFIG_ID_REGEX, target)]

    target_sconscript = f"{components[component["component"]]['path']}SConscript"
    PlatformEnv["ARTIFACTS"][f"{target.upper()}"] = SConscript(target_sconscript, exports={"TARGET": target, "CONFIG_IDS": config_ids, "PLATFORM_ENV": PlatformEnv, "NETWORK_ENV": NetworkEnv })

if PlatformEnv["ARTIFACTS"]:
    AddOption("--upload", dest="upload", action="store_true")
    AddOption("--debugger", dest="debugger", action="store_true")

    for component in PlatformEnv["ARTIFACTS"]:
        if component == "DBC":
            continue

        if any("FLASHABLE_ARTIFACT" in artifact for artifact in PlatformEnv["ARTIFACTS"][component]):
            if GetOption("upload"):
                if multiple_targets:
                    print(f"Cannot upload multiple targets...")
                    print(f"Exiting...")
                    Exit(1)

                if type(PlatformEnv["ARTIFACTS"][component]["FLASHABLE_ARTIFACT"]["artifact"]) is list:
                    if len(PlatformEnv["ARTIFACTS"][component]["FLASHABLE_ARTIFACT"]["artifact"]) > 1:
                        print(f"Cannot flash multiple configs...")
                        print(f"Exiting...")
                        Exit(1)

                artifact = PlatformEnv["ARTIFACTS"][component]["FLASHABLE_ARTIFACT"]["artifact"]
                start_addr = PlatformEnv["ARTIFACTS"][component]["FLASHABLE_ARTIFACT"]["addr"]
                tools = PlatformEnv["ARTIFACTS"][component]["FLASHABLE_ARTIFACT"]["tools"]
                flashing_env = GlobalEnv.Clone(tools=tools)

                upload = flashing_env.flash(source=artifact, start_addr=start_addr)
                Default(upload)

        if any("DEBUG_ARTIFACT" in artifact for artifact in PlatformEnv["ARTIFACTS"][component]):
            if GetOption("debugger"):
                if multiple_targets:
                    print(f"Cannot debug multiple targets...")
                    print(f"Exiting...")
                    Exit(1)

                if type(PlatformEnv["ARTIFACTS"][component]["DEBUG_ARTIFACT"]["artifact"]) is list:
                    if len(PlatformEnv["ARTIFACTS"][component]["DEBUG_ARTIFACT"]["artifact"]) > 1:
                        print(f"Cannot debug multiple configs...")
                        print(f"Exiting...")
                        Exit(1)

                artifact = PlatformEnv["ARTIFACTS"][component]["DEBUG_ARTIFACT"]["artifact"]
                args = PlatformEnv["ARTIFACTS"][component]["DEBUG_ARTIFACT"]["args"]
                tools = PlatformEnv["ARTIFACTS"][component]["DEBUG_ARTIFACT"]["tools"]
                env = PlatformEnv["ARTIFACTS"][component]["DEBUG_ARTIFACT"].get("env", {})
                debug_env = GlobalEnv.Clone(tools=tools, **env)

                openocd_gdb = debug_env.openocd_gdb(artifact, *args)
                Default(openocd_gdb)

PlatformEnv.Append(
    TARFLAGS = '-c -z',
)

def flatten(sequence: list) -> list:
    new_list = []
    for item in sequence:
        if type(item) != list and type(item) != NodeList:
            new_list.append(item)
        else:
            new_list.extend(flatten(item))
    return new_list

AddOption("--package", dest="package", action="store_true")

artifacts = []
for key, value in PlatformEnv["ARTIFACTS"].items():
    if type(value) is dict:
        for name, val in value.items():
            if "ARTIFACT" in name:
                artifacts.append(val["artifact"])
plat_name = GetOption("platform") or "artifacts"
package = PlatformEnv.Tar(f"platform-artifacts/{plat_name}.tgz", [ file.abspath for file in flatten(artifacts) ])
Depends(package, artifacts)

if GetOption("package"):
    Default(package)
