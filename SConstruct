#!/usr/bin/python3

from SCons.Script import (
    AddOption,
    Default,
    Dir,
    Environment,
    Export,
    Exit,
    GetOption,
    SConscript,
)

from os.path import dirname
from os import environ
from oyaml import safe_load

# create a global environment which all targets can start from
GlobalEnv = Environment(REPO_ROOT_DIR=Dir("#"), tools=[])
GlobalEnv["ENV"]["TERM"] = environ["TERM"]
Export("GlobalEnv")

# add option to choose target
AddOption("--target", dest="target", type="string", action="store")
# add option to make build verbose
AddOption("--verbose", dest="verbose", action="store_true")
# add option to build without debug info
AddOption("--release", dest="release", action="store_true")


with open("site_scons/components.yaml") as components_file:
    components = safe_load(components_file)

target = GetOption("target")
if target:
    if not target in components:
        print(f"Unknown target '{target}'. See site_scons/components.yaml")
        Exit(1)

    target = f"{components[target]['path']}/SConscript"
    artifacts = SConscript(target)

    if artifacts:
        if artifacts.get("FLASHABLE_ARTIFACT", None):
            AddOption("--upload", dest="upload", action="store_true")
            if GetOption("upload"):
                artifact = artifacts["FLASHABLE_ARTIFACT"]["artifact"]
                start_addr = artifacts["FLASHABLE_ARTIFACT"]["addr"]
                tools = artifacts["FLASHABLE_ARTIFACT"]["tools"]
                flashing_env = GlobalEnv.Clone(tools=tools)

                upload = flashing_env.flash(source=artifact, start_addr=start_addr)
                Default(upload)

        if artifacts.get("DEBUG_ARTIFACT", None):
            AddOption("--debugger", dest="debugger", action="store_true")
            if GetOption("debugger"):
                artifact = artifacts["DEBUG_ARTIFACT"]["artifact"]
                args = artifacts["DEBUG_ARTIFACT"]["args"]
                tools = artifacts["DEBUG_ARTIFACT"]["tools"]
                env = artifacts["DEBUG_ARTIFACT"].get("env", {})
                debug_env = GlobalEnv.Clone(tools=tools, **env)

                openocd_gdb = debug_env.openocd_gdb(artifact, *args)
                Default(openocd_gdb)
