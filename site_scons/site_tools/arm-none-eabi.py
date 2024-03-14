from os.path import splitext
import SCons
from SCons.Script import *

TOOL_PATHS = [
    Dir("/opt/toolchains/gcc-arm-none-eabi/bin/"),
    Dir("#/embedded/toolchains/gcc-arm-none-eabi/bin/"),
]

TOOL_PATH = None
for path in TOOL_PATHS:
    if path.exists():
        TOOL_PATH = path
        break
if not TOOL_PATH:
    raise Exception("gcc-arm-none-eabi not found!")


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
    env["OBJDUMP"] = tool_path("objdump")
    env["GDB"] = tool_path("gdb")

    if not GetOption("verbose"):
        env["ASCOMSTR"] = "Assembling $SOURCE > $TARGET"
        env["ASPPCOMSTR"] = "Assembling $SOURCE > $TARGET"
        env["CCCOMSTR"] = "Compiling $SOURCE > $TARGET"
        env["LINKCOMSTR"] = "Linking $SOURCE > $TARGET"
        env["BINCOMSTR"] = "Generating binary file $TARGET"
        env["OBJDUMPSTR"] = "Disassembling $SOURCE > $TARGET"

    env["OBJPREFIX"] = ""
    env["OBJSUFFIX"] = ".obj"
    env["PROGSUFFIX"] = ".elf"

    env["BUILDERS"]["Program"] = SCons.Builder.Builder(
        generator=program,
        emitter=prog_emitter,
        src_builder="Object",
        src_suffix="$OBJSUFFIX",
        suffix="$PROGSUFFIX",
    )

    env["BUILDERS"]["Bin"] = SCons.Builder.Builder(
        generator=bin_generator,
        src_suffix=".elf",
        suffix=".bin",
    )

    env["BUILDERS"]["Asm"] = SCons.Builder.Builder(
        generator=asm_generator,
        src_suffix=[".obj", ".elf"],
        suffix=".asm",
    )

    env["BUILDERS"]["gdb"] = SCons.Builder.Builder(action="$GDB $SOURCE $args")
    env.AddMethod(_gdb, "launch_gdb")


def program(target, source, env, for_signature):
    return SCons.Action.Action(
        "$CC -o $TARGET -T $LINKSCRIPT $LINKFLAGS $SOURCES",
        "$LINKCOMSTR",
    )


def prog_emitter(target, source, env):
    map_file = "build/" + splitext(target[0].name)[0] + ".map"
    target.append([map_file])
    return target, source


def bin_generator(target, source, env, for_signature):
    return SCons.Action.Action(
        "$OBJCOPY -O binary $SOURCE $TARGET",
        "$BINCOMSTR",
    )


def asm_generator(target, source, env, for_signature):
    return SCons.Action.Action(
        "$OBJDUMP -D $SOURCE > $TARGET",
        "$OBJDUMPSTR",
    )


def _gdb(env, elf_file, *args):
    args = " ".join(args)
    fn = "phony.file" + str(len(args))
    env.Pseudo(fn)
    return env.gdb(source=elf_file, target=fn, args=args)


def exists():
    return True
