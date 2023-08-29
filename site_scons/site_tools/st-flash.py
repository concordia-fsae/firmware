import SCons
from SCons.Script import *


def generate(env):
    if not GetOption("verbose"):
        env["FLASHCOMSTR"] = "Flashing $SOURCE to address ${hex(addr)}"
        env["ERASECOMSTR"] = "Erasing chip"

    env["BUILDERS"]["_erase"] = SCons.Builder.Builder(
        action=SCons.Action.Action(
            "st-flash erase",
            "$ERASECOMSTR",
        )
    )

    env["BUILDERS"]["_flash"] = SCons.Builder.Builder(
        action=SCons.Action.Action(
            "st-flash write $SOURCE $addr",
            "$FLASHCOMSTR",
        )
    )

    env.AddMethod(flash, "flash")


def flash(env, source, target=0x08000000):
    env._erase()
    return env._flash(source=source, addr=target)


def exists():
    return True
