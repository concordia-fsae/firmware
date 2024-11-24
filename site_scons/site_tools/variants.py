from typing import List

from SCons.Node import FS
from oyaml import safe_load

def GenerateFeatures(env, features_files: List[FS.File], features_dict: List[dict] = list()):
    for feature_file in features_files:
        if not feature_file.exists():
            raise Exception(f"File '{feature_file.abspath}' does not exist")

    features = {}

    for feature_file in features_files:
        with open(feature_file.abspath, "r") as fd:
            loaded_features = safe_load(fd)
            if loaded_features["features"] is None:
                continue
            for loaded_feature in loaded_features["features"]:
                if loaded_feature in features.keys():
                    raise Exception(f"Feature: {loaded_feature} in file {feature_file.abspath} already exists.")
            features.update(loaded_features["features"])

    feature_values = {}

    if features is not None:
        for feature in features:
            if type(features[feature]) is dict:
                if "enabled" in features[feature]:
                    if features[feature]["enabled"]:
                        if "requires" in features[feature]:
                            for requires in features[feature]["requires"]:
                                if requires not in features.keys():
                                    raise Exception(f"Required feature: {requires} does not exist.")
                                feature_values[requires] = 1
                        feature_values[feature] = 1
                    else:
                        feature_values[feature] = 0
                else:
                    feature_values[feature] = 0
            elif type(features[feature]) is bool:
                if features[feature]:
                    feature_values[feature] = 1
                else:
                    feature_values[feature] = 0
            else: 
                feature_values[feature] = 0

    if features_dict is not None:
        for requested_feature in features_dict:
            if requested_feature not in feature_values.keys():
                raise Exception(f"Feature: {requested_feature} does not exist.")
            if type(features_dict[requested_feature]) is bool:
                if features_dict[requested_feature]:
                    feature_values[requested_feature] = 1
                else:
                    feature_values[requested_feature] = 0
            else:
                feature_values[requested_feature] = 1

    return feature_values

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

    if config_ids:
        return dict(filter(lambda item: item[0] in config_ids, variants["configs"].items()))

    return variants["configs"]


def generate(env):
    env.AddMethod(GenerateVariants)
    env.AddMethod(GenerateFeatures)


def exists():
    return True
