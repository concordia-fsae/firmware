##
# @file dox.py
# @brief Adds support for doxygen 
# @author Joshua Lafleur (josh.lafleur@outlook.com)
# @date 2023-11-07

from SCons.Script import *

def _get_doxygen_cmd(config_file):
    env["doxyconf"] = config_file
    return f"doxygen $config_file"


def generate(env):
    '''
    Generates the SCons tools in the environment
    @param Environment variable to be modified
    @retval None
    '''

    env.AddMethod(_get_doxygen_cmd, "doxygen")
    env["DOXYGEN"] = "/bin/doxygen"

    env["BUILDERS"]["doxygen"] = SCons.Builder.Builder(
        #action="$DOXYGEN $SOURCE $args",
        action="echo 'Building documentation...'"
    )

def exists():
    '''
    SCons function showing module exists
    @note Included for forward compatibility
    @retval True
    '''
    return True
