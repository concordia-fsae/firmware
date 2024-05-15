from SCons.Script import Action, Builder


def generate(env):
    env.Replace(
        FLASHCOMSTR="Flashing $SOURCE to address ${hex(addr)}",
        ERASECOMSTR="Erasing chip",
    )

    env["BUILDERS"]["_erase"] = Builder(
        action=Action(
            "st-flash erase",
            cmdstr="$ERASECOMSTR",
        )
    )

    env["BUILDERS"]["_flash"] = Builder(
        action=Action(
            "st-flash write $SOURCE $addr",
            cmdstr="$FLASHCOMSTR",
        )
    )

    env.AddMethod(flash, "flash")


def flash(env, source, start_addr):
    return [env._erase(), env._flash(source=source, addr=start_addr)]


def exists():
    return True
