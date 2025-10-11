from argparse import ArgumentParser, ArgumentError, Action
from mako.template import Template
import os
from oyaml import safe_load
from typing import List

class MergeKeyValuePairs(Action):
   """
   argparse action to split a KEY=VALUE argument and append the pairs to a dictionary.
   """
   def __call__(self, parser, args, values, option_string=None):
      previous = getattr(args, self.dest, None) or dict()
      try:
         added = dict(map(lambda x: x.split('='), values))
      except ValueError:
         raise ArgumentError(self, f"Could not parse argument \"{values}\" as k1=v1 k2=v2 ... format")
      merged = {**previous, **added}
      setattr(args, self.dest, merged)


def render_generated_file(options: dict[str, str], template: str, output_dir: str):
    renderer = Template(filename=template)
    try:
        rendered = renderer.render(**dict(sorted(options.items())))
    except Exception as e:
        raise Exception(f"Template render error on '{template}': {e}")
    if not isinstance(rendered, str):
        raise Exception("Template render error on '{template}': "+
                        "Mako rendering didn't produce a str to write to the file")

    with open(output_dir + os.sep + template.rstrip(".mako"), "w") as out_fd:
        out_fd.write(rendered)


def GenerateFeatures(selection_files: List[str], features_dict: List[dict] = list()):
    features_files = []
    for selection_file in selection_files:
        with open(selection_file, "r") as fd:
            loaded_selections = safe_load(fd)
            try:
                if type(loaded_selections["featureDefs"]) is str:
                    # TODO: Fix this once we remove scons support
                    features_files += [ open(os.path.basename(loaded_selections["featureDefs"])) ]
                elif type(loaded_selections["featureDefs"]) is list:
                    # TODO: Fix this once we remove scons support
                    features_files += [ open(os.path.basename(file)) for file in loaded_selections["featureDefs"] ]
            except KeyError:
                raise Exception(f"FeatureDefs: Definition file in {selection_file} not specified in 'featureDefs'.")

    features = {}
    discreteValues = {}

    for fd in features_files:
            loaded_features = safe_load(fd)
            if loaded_features["defs"] is None:
                continue
            if "config" not in loaded_features.keys():
                raise Exception(f"Feature: File {fd.name} doesn't contain a configuration.")
            if "prefix" not in loaded_features["config"] or "description" not in  loaded_features["config"]:
                raise Exception(f"Feature: File {fd.name} doesn't contain a prefix or description.")
            defs_copy = loaded_features["defs"].copy()
            for loaded_feature in defs_copy:
                new_loaded_feature = loaded_features["config"]["prefix"] + "_" + loaded_feature
                loaded_features["defs"][new_loaded_feature] = loaded_features["defs"].pop(loaded_feature)
                if new_loaded_feature in features.keys():
                    raise Exception(f"Feature: {new_loaded_feature} in file {fd.name} already exists.")
            features.update(loaded_features["defs"])
            if "discreteValues" in loaded_features:
                for value in loaded_features["discreteValues"]:
                    if value in discreteValues.keys():
                        raise Exception(f"FeatureDef: {value} in file {fd.name} already exists in the feature tree.")
                discreteValues.update(loaded_features["discreteValues"])

    feature_values = {}
    for selection_file in selection_files:
        with open(selection_file, "r") as fd:
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
                            raise Exception(f"FeatureDefs: Requested value {loaded_selections['features'][feature]} not defined.")
                feature_values.update(loaded_selections["features"])
            except TypeError: # Nonetype
                pass
            except KeyError:
                raise Exception(f"FeatureDefs: No features specified in {selection_file}")

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
                if "restricts" in features[feature] and feature_values[feature] is not False:
                    for requirement in features[feature]["restricts"]:
                        if requirement not in features.keys():
                            raise Exception(f"FeatureDefs: Restricted feature {requirement} is not specified.")
                        if requirement in feature_values.keys() and feature_values[requirement] is not False:
                            raise Exception(f"FeatureDefs: Restricted feature {requirement} is enabled.")

    features["defs"] = feature_values
    features["discreteValues"] = discreteValues

    return features

def LoadVariants(variants_file: str):
    if not os.path.exists(variants_file):
        raise Exception(f"File '{variants_file}' does not exist")
    with open(variants_file, "r") as fd:
        variants = safe_load(fd)

    return variants["configs"]


def GetOptions(config_id: int, variants, kv: dict[str, str]):
    try:
        if config_id not in variants:
            raise Exception(f"Invalid config id. Supported IDs are {list(variants.keys())}")
        override_options = variants[config_id]["options"] if variants[config_id]["options"] else {}
        kv.update(override_options)
        return kv
    except Exception as e:
        raise Exception(f"Unknown error \"{type(e)} {e}\", unable to get variant options")


def GetFeatures(config_id: int, variants):
    return variants[config_id]["features"]


if __name__ == "__main__":
    parser = ArgumentParser("feature-tree")

    parser.add_argument(
        "--config-id",
        required=True,
        type=int,
        help="Config ID in use.",
        metavar="CONFIG_ID",
    )
    parser.add_argument(
        "--sources",
        required=True,
        nargs='+',
        help="Path to variants.yaml, *_FeatureDefs.yaml, and *_FeatureSels.yaml." +
            "Note: The order of files is from lowest to highest priority",
        metavar="SOURCES",
    )
    parser.add_argument(
        "--set",
        nargs='*',
        default={},
        action=MergeKeyValuePairs,
        metavar="KEY=VALUE",
        help='Add key-value mappings to export into BuildDefines_generated.h.'
            ' If a value contains spaces, please wrap it in double quotes: key="value with spaces".'
    )

    parser.add_argument(
        "--output",
        required=True,
        help="Output folder destination.",
        metavar="OUTPUT_FOLDER",
    )

    args = parser.parse_args()

    variants_file = ""
    selections = []
    definitions = []

    for file in args.sources:
        if not os.path.exists(file):
            raise Exception(f"File '{variants_file}' does not exist")
        if ".mako" in file:
            continue
        elif "variants.yaml" in file:
            if variants_file != "":
                print("Error: Multiple 'variants.yaml' files in the sources!")
                exit(1)
            variants_file = file
            continue
        elif "FeatureSels.yaml" in file:
            selections.append(file)
        elif "FeatureDefs.yaml" in file:
            definitions.append(file)
        else:
            print(f"Warning: Unknown '{file}' file in the sources!")

    if variants_file == "":
        print("Error: No 'variants.yaml' file in the sources!")
        exit(1)
    elif len(selections) == 0:
        print("Error: Atleast one '*FeatureSels.yaml' file must be in the sources!")
        exit(1)
    elif len(definitions) == 0:
        print("Error: Atleast one '*FeatureDefs.yaml' file must be in the sources!")
        exit(1)
    elif not os.path.exists(args.output):
        raise Exception(f"Output directory '{args.output}' does not exist")


    try:
        options = {}
        variants = LoadVariants(variants_file)
        options = GetOptions(args.config_id, variants, args.set)
        features = GetFeatures(args.config_id, variants)
        render_generated_file({ "configs": options }, "BuildDefines_generated.h.mako", args.output)
        features = GenerateFeatures(selections, features["overrides"])
        render_generated_file({ "features": features }, "FeatureDefines_generated.h.mako", args.output)
    except Exception as e:
        print(f"Error: {e}")
        exit(1)
