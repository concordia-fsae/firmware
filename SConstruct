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

# add option to choose target
AddOption("--target", dest="target", type="string", action="store")
# add option to not generate the compilation database
AddOption("--nocompiledb", dest="nocompiledb", action="store_true")
# add option to make build verbose
AddOption("--verbose", dest="verbose", action="store_true")
# add option to log output to file
AddOption("--savelog", dest="savelog", action="store_true")
# add option for clean build
AddOption("--cleanbuild", dest="cleanbuild", action="store_true")
# add option for platformio commandline args
AddOption("--pio_args", dest="pio_args", type="string", action="store", default="")
# add option to build without debug info
AddOption("--release", dest="release", action="store_true")
# add option to document the code
AddOption("--doc", dest="doc", action="store_true")

def pio_build_cmd(source, **_):
    pio_dir = dirname(str(source[0]))
    if GetOption("cleanbuild"):
        Execute(f"cd {pio_dir} && pio run -t clean")

    cmd = f"cd {pio_dir} && pio run"

    if GetOption("upload"):
        cmd += " -t upload"

    if GetOption("savelog"):
        cmd += " -v | tee build.log"
    elif GetOption("verbose"):
        cmd += " -v"

    cmd += get_pio_args()

    Execute(cmd)

    # TODO: we shouldn't try to compiledb if the build above fails
    if not GetOption("nocompiledb") and not GetOption("cleanbuild"):
        Execute(f"cd {pio_dir} && pio run -t compiledb")

def get_pio_args():
    pio_args = GetOption("pio_args")
    if pio_args:
        return pio_args.split(",")
    else:
        return ""

pio_build = Builder(action=pio_build_cmd)
env.Append(BUILDERS={"pio_build": pio_build})

Export("env")

with open("site_scons/components.yaml") as components_file:
    components = load(components_file, Loader=Loader)

target = GetOption("target")
if not target:
    doc = GetOption("doc")
    if doc:
        Execute(f"doxygen site_scons/site_tools/doxygen.conf")
    else:
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
