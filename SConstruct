#!/usr/bin/python3

from SCons.Script import (
    AddOption,
    Builder,
    COMMAND_LINE_TARGETS,
    Environment,
    Execute,
    Export,
    GetOption,
    SConscript,
)
from os.path import dirname
from yaml import load, Loader

env = Environment()

# don't generate the compilation database
AddOption("--nocompiledb", dest="compiledb", action="store_true")


def pio_build_cmd(source, **kwargs):
    pio_dir = dirname(str(source[0]))
    cmd = f"cd {pio_dir} && pio run"
    if not GetOption("compiledb"):
        cmd += "  &&  pio run -t compiledb"
    Execute(cmd)


pio_build = Builder(action=pio_build_cmd)
env.Append(BUILDERS={"pio_build": pio_build})

Export("env")

AddOption("--target", dest="target", type="string", action="store")

with open("site_scons/components.yaml") as components_file:
    components = load(components_file, Loader=Loader)
print(components)


target = GetOption("target")
if not target:
    print("No targets specified!")
else:
    targets_arr = target.split(",")
    targets = [
        f"{components[target]['path']}/SConscript"
        for target in targets_arr
        if target in components
    ]
    if len(targets) != len(targets_arr):
        print("Warning: Some of the specified targets do not exist")
    SConscript(targets)
