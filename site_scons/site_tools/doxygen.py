##
# @file dox.py
# @brief Adds support for doxygen 
# @author Joshua Lafleur (josh.lafleur@outlook.com)
# @date 2023-11-07

from os.path import join
from SCons.Script import *

def generate(env):
    '''
    Generates the SCons tools in the environment
    @param Environment variable to be modified
    @retval None
    '''
    env["DOXYGEN"] = "/bin/doxygen"

    env["BUILDERS"]["doxygen"] = SCons.Builder.Builder(
        action="$DOXYGEN $DOXYGENCONF $args",
    )

def exists():
    '''
    SCons function showing module exists
    @note Included for forward compatibility
    @retval True
    '''
    return True
