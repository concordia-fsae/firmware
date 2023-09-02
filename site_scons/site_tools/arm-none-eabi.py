##
# @file arm-none-eabi.py
# @brief  Adds support for ARM Toolchain
# @author Ricky Lopez
# @version 0.1
# @date 2022-08-27

from os.path import splitext
from SCons.Script import *

TOOL_PATH = Dir("/opt/toolchains/gcc-arm-none-eabi/bin/") # @brief Absolute path of binaries when inside container


##
# @brief  Compiles the targets with signature
#
# @note Not to be called directly
#
# @param target Target output file
# @param source Sources to be used
# @param env Environment to be worked on
# @param for_signature Signature given to Target
#
# @retval   Returns the SCons action
def _program(target, source, env, for_signature):
    return SCons.Action.Action(
        [
            "$CC -o $TARGET -T $LINKSCRIPT $LINKFLAGS $SOURCES",
            "$OBJCOPY -O binary $TARGET ${TARGET.target_from_source('', '.bin')}",
        ]
    )


##
# @brief Appends the Binary and Map file to the target and returns with source
#
# @note Not to be called directly

# @param target Target to work on
# @param source Source to work from
# @param env Environment being worked on
#
# @retval   [target, source]
def _bin_emitter(target, source, env):
    bin_file = "build/" + splitext(target[0].name)[0] + ".bin"
    map_file = "build/" + splitext(target[0].name)[0] + ".map"
    target.append([bin_file, map_file])
    return target, source


##
# @brief Adds ARM GDB support 
#
# @note Not to be called directly
#
# @param env Environment to work on
# @param elf_file ELF file
# @param args args to be passed to GDB
#
# @retval   Return execution of SCons GDB
def _gdb(env, elf_file, *args):
    args = " ".join(args)
    fn = "phony.file" + str(len(args))
    env.Pseudo(fn)
    return env.gdb(source=elf_file, target=fn, args=args)


##
# @brief  Generates the SCons tools in the environment
#
# @param env Environment to be worked on
#
# @retval   None
def generate(env):
    SCons.Tool.Tool("cc")(env)
    SCons.Tool.Tool("ar")(env)
    SCons.Tool.Tool("as")(env)

    tool_prefix = "arm-none-eabi"

    def tool_path(tool: str) -> str:
        return TOOL_PATH.File(tool_prefix + "-" + tool)

    env["AS"] = tool_path("gcc")
    env["CC"] = tool_path("gcc")
    env["AR"] = tool_path("gcc-ar")
    env["CXX"] = tool_path("c++")
    env["RANLIB"] = tool_path("gcc-ranlib")
    env["OBJCOPY"] = tool_path("objcopy")
    env["GDB"] = tool_path("gdb")

    env["OBJPREFIX"] = ""
    env["OBJSUFFIX"] = ".obj"
    env["PROGSUFFIX"] = ".elf"

    env["BUILDERS"]["Program"] = SCons.Builder.Builder(
        generator=_program,
        emitter=_bin_emitter,
        src_builder="Object",
        src_suffix="$OBJSUFFIX",
        suffix="$PROGSUFFIX",
    )

    env["BUILDERS"]["gdb"] = SCons.Builder.Builder(
        action="$GDB $SOURCE $args"
    )
    env.AddMethod(_gdb, "launch_gdb")


##
# @brief  Shows this module exists
#
# @note Included for forward compatibility
#
# @retval True 
def exists():
    return True
