from SCons.Script import *


# def _flash_chip(env, bin_file):
#     env["bin_file"] = bin_file
#     return env.flash_chip(bin_file)


def generate(env):
    # env.AddMethod(_flash_chip, "flash")

    env['BUILDERS']['flash'] = SCons.Builder.Builder(
        action = SCons.Action.Action(
            "st-flash write $SOURCE 0x08000000"
        )
    )
    env['BUILDERS']['flash_dbg'] = SCons.Builder.Builder(
        action = SCons.Action.Action(
            "st-flash --hot-plug write $SOURCE 0x08000000"
        )
    )


def exists():
    return True
