##
# @file dox.py
# @brief  SCons python tool for Doxygen integration
# @author Joshua Lafleur (josh.lafleur@outlook.com)
# @version 0.1
# @date 2022-08-27

from SCons.Script import *


##
# @brief  Generates the helper actions in SCons
#
# @param env SCons environment to work on
#
# @retval   None
def generate(env):
    # @brief Adds the bash command as a builder to the environment
    env['BUILDERS']['dox'] = SCons.Builder.Builder(
        action = f"doxygen -g $SOURCE"
    )


##
# @brief  Shows that this module exists
#
# @note     Included for forward compatibility
#
# @retval   True
def exists():
    return True

