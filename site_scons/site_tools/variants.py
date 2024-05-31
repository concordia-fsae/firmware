from typing import List

from SCons.Node import FS
from oyaml import safe_load


def GenerateVariants(env, variants_file: FS.File, config_ids: List[int] = list()):
    if not variants_file.exists():
        raise Exception(f"File '{variants_file.abspath}' does not exist")

    with open(variants_file.abspath, "r") as fd:
        variants = safe_load(fd)

    valid_config_ids = set(variants["configs"].keys())
    config_ids_set = set(config_ids)

    if len(config_ids_set) != len(config_ids):
        raise Exception(f"Duplicate config IDs specified")

    if config_ids and not set(config_ids).issubset(set(valid_config_ids)):
        raise Exception(
            f"Some of the specified config IDs do not exist: {config_ids_set - valid_config_ids}"
        )

    return dict(filter(lambda item: item[0] in config_ids, variants["configs"].items()))


def generate(env):
    env.AddMethod(GenerateVariants)


def exists():
    return True
