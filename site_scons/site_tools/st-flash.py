##
# @file st-flash.py
# @brief  Adds ST-Flash support to Scons
# @author Ricky Lopez
# @version 0.1
# @date 2022-08-27


from SCons.Script import *


# def _flash_chip(env, bin_file):
#     env["bin_file"] = bin_file
#     return env.flash_chip(bin_file)


##
# @brief  Generates SCons support for ST-Flash
#
# @param env Environment to be worked on
#
# @retval   None
def generate(env):
    # env.AddMethod(_flash_chip, "flash")

    env['BUILDERS']['flash'] = SCons.Builder.Builder(
        action = SCons.Action.Action(
            "st-flash write $SOURCE 0x08000000"
        )
    )


##
# @brief  Shows module exists
#
# @retval   True
def exists():
    return True
