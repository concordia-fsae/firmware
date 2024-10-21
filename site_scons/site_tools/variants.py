from typing import List

from SCons.Node import FS
from SCons.Script import File
from oyaml import safe_load

def GenerateFeatures(env, selection_files: List[FS.File], features_dict: List[dict] = list()):
    features_files = []
    for selection_file in selection_files:
        with open(selection_file.abspath, "r") as fd:
            loaded_selections = safe_load(fd)
            try:
                features_files += [ File(loaded_selections["featureDefs"]) ]
            except KeyError:
                raise Exception(f"FeatureDefs: Definition file in {selection_file.abspath} not specified in 'featureDefs'.")

    for feature_file in features_files:
        if not feature_file.exists():
            raise Exception(f"File '{feature_file.abspath}' does not exist")

    features = {}
    discreteValues = {}

    for feature_file in features_files:
        with open(feature_file.abspath, "r") as fd:
            loaded_features = safe_load(fd)
            if loaded_features["defs"] is None:
                continue
            if "config" not in loaded_features.keys():
                raise Exception(f"Feature: File {feature_file.abspath} doesn't contain a configuration.")
            if "prefix" not in loaded_features["config"] or "description" not in  loaded_features["config"]:
                raise Exception(f"Feature: File {feature_file.abspath} doesn't contain a prefix or description.")
            defs_copy = loaded_features["defs"].copy()
            for loaded_feature in defs_copy:
                new_loaded_feature = loaded_features["config"]["prefix"] + "_" + loaded_feature
                loaded_features["defs"][new_loaded_feature] = loaded_features["defs"].pop(loaded_feature)
                if new_loaded_feature in features.keys():
                    raise Exception(f"Feature: {new_loaded_feature} in file {feature_file.abspath} already exists.")
            features.update(loaded_features["defs"])
            if "discreteValues" in loaded_features:
                for value in loaded_features["discreteValues"]:
                    if value in discreteValues.keys():
                        raise Exception(f"FeatureDef: {value} in file {feature_file.abspath} already exists in the feature tree.")
                discreteValues.update(loaded_features["discreteValues"])

    feature_values = {}
    for selection_file in selection_files:
        with open(selection_file.abspath, "r") as fd:
            loaded_selections = safe_load(fd)
            try:
                for feature in loaded_selections["features"]:
                    if feature not in features:
                        raise Exception(f"FeatureDefs: Feature {feature} in {selection_file} not defined anywhere in Feature Tree.")
                    if loaded_selections["features"][feature] is None:
                        loaded_selections["features"][feature] = True
                    elif type(loaded_selections["features"][feature]) is str:
                        valid = False
                        for values in discreteValues:
                            if loaded_selections["features"][feature] in discreteValues[values]:
                                valid = True
                                loaded_selections["features"][feature] = values + "_" + loaded_selections["features"][feature]
                        if not valid:
                            raise Exception(f"FeatureDefs: Requested value {loaded_selections["features"][feature]} not defined.")
                feature_values.update(loaded_selections["features"])
            except TypeError: # Nonetype
                pass
            except KeyError:
                raise Exception(f"FeatureDefs: No features specified in {selection_file.abspath}")

    if features_dict is not None:
        for requested_feature in features_dict:
            if requested_feature not in features.keys():
                raise Exception(f"Feature: {requested_feature} does not exist.")
            elif isinstance(features_dict[requested_feature], int):
                feature_values[requested_feature] = features_dict[requested_feature]
            elif type(features_dict[requested_feature]) is bool:
                if features_dict[requested_feature]:
                    feature_values[requested_feature] = True
                else:
                    feature_values[requested_feature] = False
            elif type(features_dict[requested_feature]) is str:
 #               requested_enum_valid = False
                for values in discreteValues:
                    if features_dict[requested_feature] in discreteValues[values]:
#                        requested_enum_valid = True
                        features_dict[requested_feature] = values + "_" + features_dict[requested_feature]
#                if not requested_enum_valid:
#                    raise Exception(f"FeatureDefs: Requested value {features_dict[requested_feature]} not defined.")
                feature_values[requested_feature] = features_dict[requested_feature]
            else:
                feature_values[requested_feature] = True

    if features is not None:
        for feature in features:
            if feature not in feature_values:
                feature_values[feature] = False
            if type(features[feature]) is dict:
                if "requires" in features[feature]:
                    for requirement in features[feature]["requires"]:
                        if requirement not in feature_values.keys():
                            raise Exception(f"FeatureDefs: Required feature {requirement} is not specified.")
                        if feature_values[feature] is not None and feature_values[feature] is not False and feature_values[feature] != -1:
                            if feature_values[requirement] is None or feature_values[requirement] == -1 or feature_values[requirement] is False:
                                raise Exception(f"FeatureDefs: Required feature {requirement} is disabled.")

    features["defs"] = feature_values
    features["discreteValues"] = discreteValues

    return features

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
