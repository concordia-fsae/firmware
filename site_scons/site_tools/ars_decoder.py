import importlib

from SCons.Script import *

#scons = importlib.find_loader('SCons.Script')
#scons_found = scons is not None
#
#if scons_found:
#    from scons import *

def _decode():
    print("Hello")

def generate(env):
    env.AddMethod(_decode, "decode")

def exists():
    return True
