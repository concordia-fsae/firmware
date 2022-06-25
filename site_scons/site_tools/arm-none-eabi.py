from os.path import splitext
from SCons.Script import *

TOOL_PATH = Dir("/opt/toolchains/gcc-arm-none-eabi/bin/")


def _program(target, source, env, for_signature):
    return SCons.Action.Action(
        [
            "$CC -o $TARGET -T $LINKSCRIPT $LINKFLAGS $SOURCES",
            "$OBJCOPY -O binary $TARGET ${TARGET.target_from_source('', '.bin')}",
        ]
    )


def _bin_emitter(target, source, env):
    bin_file = "build/" + splitext(target[0].name)[0] + ".bin"
    map_file = "build/" + splitext(target[0].name)[0] + ".map"
    target.append([bin_file, map_file])
    return target, source


def _gdb(env, elf_file, *args):
    args = " ".join(args)
    fn = "phony.file" + str(len(args))
    env.Pseudo(fn)
    return env.gdb(source=elf_file, target=fn, args=args)


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


def exists():
    return True
