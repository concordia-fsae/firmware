from SCons.Script import *

OPENOCD_DIR = Dir("#embedded/openocd")


def _start_server(env, interface, board):

    interface = OPENOCD_DIR.Dir("interface").File(interface + ".cfg")
    board = OPENOCD_DIR.Dir("board").File(board + ".cfg")

    if interface.exists() and board.exists():
        env["interface"] = interface
        env["board"] = board
        ret = env.openocd(target=interface)
    else:
        print("Interface or Board file does not exist!")
        ret = 1

    return ret


def _get_openocd_cmd(env, interface, board):

    interface = OPENOCD_DIR.Dir("interface").File(interface + ".cfg")
    board = OPENOCD_DIR.Dir("board").File(board + ".cfg")

    if interface.exists() and board.exists():
        env["interface"] = interface
        env["board"] = board
        ret = (
            f'openocd -s {OPENOCD_DIR.path} -f $interface -f $board -c "gdb_port pipe"'
        )
    else:
        print("Interface or Board file does not exist")
        ret = ""

    return ret


def generate(env):
    env.AddMethod(_start_server, "openocd_srv")
    env.AddMethod(_get_openocd_cmd, "openocd_cmd")

    env["BUILDERS"]["openocd"] = SCons.Builder.Builder(
        action=f"openocd -s {OPENOCD_DIR.path} -f $interface -f $board"
    )


def exists():
    return True
