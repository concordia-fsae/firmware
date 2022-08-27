##
# @file openocd.py
# @brief  Adds OpenOCD support to SCons
# @author Ricky Lopez
# @version 0.1
# @date 2022-08-27


from SCons.Script import *

OPENOCD_DIR = Dir("#embedded/openocd")


##
# @brief Starts the OpenOCD server for interface and board 
#
# @note Not to be called directly
#
# @param env Environment to be worked on
# @param interface Interface to use
# @param board Board to use
#
# @retval   Returns execution of server
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


##
# @brief  Creates piped bash command to get server
#
# @note Not to be called directly
#
# @param env Environment to be worked on
# @param interface Interface to use
# @param board Board to use
#
# @retval   Command to be executed
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


##
# @brief  Generates SCons support for OpenOCD
#
# @param env Environment to be worked on
#
# @retval   None
def generate(env):
    env.AddMethod(_start_server, "openocd_srv")
    env.AddMethod(_get_openocd_cmd, "openocd_cmd")

    env["BUILDERS"]["openocd"] = SCons.Builder.Builder(
        action=f"openocd -s {OPENOCD_DIR.path} -f $interface -f $board"
    )


##
# @brief  Shows module exists
#
# @retval   True
def exists():
    return True
