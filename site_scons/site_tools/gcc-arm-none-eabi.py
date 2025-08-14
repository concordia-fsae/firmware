from optparse import OptionConflictError
from re import compile, search
from time import time_ns
from typing import Optional

from SCons.Defaults import StaticObjectEmitter
from SCons.Script import (
    Action,
    AddOption,
    AlwaysBuild,
    Builder,
    Clean,
    Dir,
    GetOption,
    NoCache,
    Tool,
    Value,
)
from SCons import Node

MAP_REGEX = compile("-Wl,-Map=(.+)")

TOOL_PATHS = [
    Dir("/opt/toolchains/gcc-arm-none-eabi/bin/"),
    Dir("#/embedded/toolchains/gcc-arm-none-eabi/bin/"),
]


def find_tool_path() -> Dir:
    for path in TOOL_PATHS:
        if path.exists():
            return path
    raise Exception("gcc-arm-none-eabi not found!")


TOOL_PATH = find_tool_path()


def generate(env):
    # load the built-in tools to start with
    Tool("cc")(env)
    Tool("as")(env)

    tool_prefix = "arm-none-eabi"

    def tool_path(tool: str) -> str:
        return TOOL_PATH.File(f"{tool_prefix}-{tool}")

    env.Replace(
        AS=tool_path("gcc"),
        CC=tool_path("gcc"),
        AR=tool_path("gcc-ar"),
        CXX=tool_path("c++"),
        RANLIB=tool_path("gcc-ranlib"),
        OBJCOPY=tool_path("objcopy"),
        OBJDUMP=tool_path("objdump"),
        GDB=tool_path("gdb"),
        CPP=tool_path("cpp"),  # c pre-processor, not c++
        STRIP=tool_path("strip"),
    )

    env.Replace(
        BINCOMSTR="Generating binary file '$TARGET'",
        HEXCOMSTR="Generating hex file '$TARGET'",
        OBJDUMPSTR="Disassembling $SOURCE > $TARGET",
        PRELINKCOMSTR=f"$CPP -P -undef $LINKFLAGS $LINKSCRIPT | sed -e '/^#.\\+$/d' > $__LINKSCRIPT",
    )

    if not GetOption("verbose"):
        env.Replace(
            ASCOMSTR="Assembling $SOURCE > $TARGET",
            ASPPCOMSTR="Assembling $SOURCE > $TARGET",
            CCCOMSTR="Compiling $SOURCE > $TARGET",
            LINKCOMSTR="Linking into '$TARGET'",
            PRELINKCOMSTR="Running c preprocessor on provided linkscript",
        )

    # default values
    env.Replace(
        OBJPREFIX="",
        OBJSUFFIX=".obj",
        PROGSUFFIX=".elf",
        PROGEMITTER=prog_emitter,
    )

    # handle the --stack-usage cmdline flag
    try:
        AddOption("--stack-usage", dest="stack_usage", action="store_true")
    except OptionConflictError:
        pass

    if GetOption("stack_usage"):
        env.Append(
            CCFLAGS="-fstack-usage",
        )
        env["BUILDERS"]["Object"].add_emitter(".c", su_emitter)

    env["BUILDERS"]["Program"] = Builder(
        generator=prog_generator,
        emitter="$PROGEMITTER",
        src_suffix="$OBJSUFFIX",
        suffix="$PROGSUFFIX",
    )

    env["BUILDERS"]["Bin"] = Builder(
        action=Action(
            "$OBJCOPY -O binary $SOURCE $TARGET",
            cmdstr="$BINCOMSTR",
        ),
        src_suffix=".elf",
        suffix=".bin",
    )

    env["BUILDERS"]["Hex"] = Builder(
        action=Action(
            "$OBJCOPY -O ihex $SOURCE $TARGET",
            cmdstr="$HEXCOMSTR",
        ),
        src_suffix=".elf",
        suffix=".hex",
    )

    env["BUILDERS"]["Asm"] = Builder(
        action=Action(
            "$OBJDUMP -D $SOURCE > $TARGET",
            cmdstr="$OBJDUMPSTR",
        ),
        src_suffix=[".obj", ".elf"],
        suffix=".asm",
        single_source=True,
    )

    env["BUILDERS"]["Asms"] = Builder(
        generator=asms_generator,
        emitter=asms_emitter,
        src_suffix=[".obj", ".elf"],
        suffix=".asms",
        single_source=True,
    )

    env["__ENV_HAS_GDB"] = True
    env["BUILDERS"]["gdb"] = Builder(
        action=Action(
            "$GDB $SOURCE $args",
            cmdstr="Launching GDB with file '$SOURCE' and args '$args'",
        )
    )
    env.AddMethod(_gdb, "launch_gdb")


def handle_mapfile(env) -> bool:
    mapflag = None
    mapfile = None

    # check if the flag to generate a map file was provided in the linkflags
    for flag in env.get("LINKFLAGS", []):
        if m := search(MAP_REGEX, flag):
            # store the data from the flag
            mapflag = m.group(0)
            mapfile = m.group(1)
            # remove the flag, we'll handle it ourselves
            env["LINKFLAGS"].remove(flag)
            break

    # check if the map file path was passed in in the env
    e_mapfile = env.get("MAPFILE", None)
    if mapfile and e_mapfile:
        # can't have both
        raise Exception(
            "Both the MAPFLAG env var and a link flag to generate a map file were provided. Please use one or the other"
        )

    # if we got here, e_mapfile is only valid if mapfile is not
    if e_mapfile:
        mapfile = e_mapfile

    # handle the different formats that the map file path can be in
    if isinstance(mapfile, str):
        env["MAPFILE"] = env["REPO_ROOT_DIR"].File(mapfile)
    elif isinstance(mapfile, Node.FS.File):
        env["MAPFILE"] = mapfile
    else:
        raise Exception(
            f"Expected MAPFILE env var to be str or File, got {type(mapfile)}"
        )

    # this effectively checks if we got the map file path from the env or from the flags
    if (not mapflag) and mapfile:
        mapflag = f"-Wl,-Map={env['MAPFILE'].path}"

    if mapflag:
        env["__MAPFLAG"] = mapflag

    if mapflag:
        return True
    return False


def handle_ldscript(target, env):
    if not "LINKSCRIPT" in env:
        raise Exception("Link script has not been provided!")
    ldfile = env["LINKSCRIPT"]

    if isinstance(ldfile, Node.FS.File):
        ldname = ldfile.name
    elif isinstance(ldfile, str):
        ldname = ldfile.split("/")[-1]
    else:
        raise Exception("LINKSCRIPT env var was neither a File object nor a str")
    env["__LINKSCRIPT"] = target[0].File(ldname)

    Clean(target[0], env["__LINKSCRIPT"])


def asms_generator(target, source, env, for_signature):
    ext = source[0].abspath.split(".")[-1]
    env["STRIPPED_TARGET"] = source[0].target_from_source(prefix="", suffix=f"_stripped.{ext}")

    strip_object = Action(
        f"$STRIP -o $STRIPPED_TARGET $SOURCE",
        cmdstr="Stripping object $SOURCE into $STRIPPED_TARGET",
    )

    generate_asm = Action(
        f"$OBJDUMP -D $STRIPPED_TARGET > $TARGET",
        cmdstr="Disassembling $STRIPPED_TARGET > $TARGET",
    )

    return [strip_object, generate_asm]


def asms_emitter(target, source, env):
    target.append(source[0].target_from_source(prefix="", suffix=".asms"))
    return target, source


def prog_generator(target, source, env, for_signature):
    handle_ldscript(target, env)

    prep_ldfile = Action(
        f"$CPP -P -undef $LINKFLAGS $LINKSCRIPT | sed -e '/^#.\\+$/d' > $__LINKSCRIPT",
        cmdstr="$PRELINKCOMSTR",
    )

    link = Action(
        f"$CC -o $TARGET -T $__LINKSCRIPT $LINKFLAGS $__MAPFLAG $SOURCES",
        cmdstr="$LINKCOMSTR",
    )
    return [prep_ldfile, link]


def prog_emitter(target, source, env):
    map_exists = handle_mapfile(env)
    if map_exists and not env["MAPFILE"] in target:
        target.append(env["MAPFILE"])

    return target, source


def su_emitter(target, source, env):
    """
    don't actually add the .su file to the targets,
    just tell scons about it so that it knows to clean it
    """

    # still call the default emitter
    target, source = StaticObjectEmitter(target, source, env)

    Clean(
        target[0],
        target[0]
        .disambiguate(must_exist=True)
        .target_from_source(prefix="", suffix=".su"),
    )
    return target, source


def _gdb(env, elf_file, *args):
    # need to make a unique target whenever this is called
    # and tell scons that it should always be rebuilt, even if inputs haven't changed
    phony = Value(time_ns())
    AlwaysBuild(phony)
    NoCache(phony)

    args = " ".join(args)
    return env.gdb(target=phony, source=elf_file, args=args)


def exists():
    return True
