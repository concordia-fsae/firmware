from SCons.Script import *


def generate(env):
    env['BUILDERS']['dox'] = SCons.Builder.Builder(
        action = f"doxygen -g $SOURCE"
    )


def exists():
    return True

