from SCons.Script import Action, Builder, Dir, File

OPENOCD_DIR = Dir("#embedded/openocd")
OPENOCD_BIN = File("/opt/openocd/bin/openocd")
MCU_DIR = OPENOCD_DIR.Dir("mcu")
IFACE_DIR = OPENOCD_DIR.Dir("interface")


def set_env(env, interface=None, mcu=None):
    if interface:
        interface = IFACE_DIR.File(interface + ".cfg")
    elif env.get("OPENOCD_INTERFACE", False):
        interface = IFACE_DIR.File(env["OPENOCD_INTERFACE"] + ".cfg")
    else:
        raise Exception(
            "OPENOCD_INTEFACE not specified in env, and `interface` parameter not given in function call"
        )

    if not interface.exists():
        raise Exception("Specified openocd interface file does not exist!")

    if mcu:
        mcu = MCU_DIR.File(mcu + ".cfg")
    elif env.get("OPENOCD_MCU", False):
        mcu = MCU_DIR.File(env["OPENOCD_MCU"] + ".cfg")
    else:
        raise Exception(
            "OPENOCD_MCU not specified in env, and `mcu` parameter not given in function call"
        )

    if not mcu.exists():
        raise Exception("Specified openocd MCU file does not exist!")

    return (interface, mcu)


def start_server(env, interface=None, mcu=None):
    vals = set_env(env, interface, mcu)
    return env.openocd(interface=vals[0], mcu=vals[1])


def get_openocd_cmd(env, interface=None, mcu=None):
    vals = set_env(env, interface, mcu)
    return f'{OPENOCD_BIN.abspath} -s {OPENOCD_DIR.abspath} -f {vals[0]} -f {vals[1]} -c "gdb_port pipe"'


def openocd_gdb(env, file, interface=None, mcu=None, args=list()):
    return env.launch_gdb(
        file,
        f"-ex 'target extended-remote | {get_openocd_cmd(env, interface, mcu)}'",
        '-ex "monitor reset"',
        *args,
    )


def generate(env):
    env.AddMethod(start_server, "OpenOcdServer")

    if env["__ENV_HAS_GDB"]:
        env.AddMethod(openocd_gdb)

    env["BUILDERS"]["openocd"] = Builder(
        action=Action(
            f"{OPENOCD_BIN.abspath} -s {OPENOCD_DIR.abspath} -f $interface -f $mcu",
            cmdstr="Launching openocd",
        )
    )


def exists():
    return True
