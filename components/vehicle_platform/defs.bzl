load("//drive-stack/conUDS/defs.bzl", "conUDS_download")
load("//drive-stack/defs.bzl", "deployable_target")
load("//drive-stack/ota-agent/defs.bzl", "ota_agent")
load("//tools/feature-tree/defs.bzl", "generate_feature_tree")
load(
    "//components/vehicle_platform:platforms.bzl",
    "platform_constraint_label",
    "platform_output_name",
    "platform_target_label",
)

def _platform_select_map(platform_variants, label_fmt):
    mapping = {
        platform_constraint_label(platform): label_fmt.format(
            platform = platform,
            platform_output = platform_output_name(platform),
        )
        for platform, _variant in platform_variants
    }
    mapping["DEFAULT"] = label_fmt.format(
        platform = platform_variants[-1][0],
        platform_output = platform_output_name(platform_variants[-1][0]),
    )
    return mapping

def platform_selected_target(platform_variants, name_prefix = "feature-tree"):
    return select(_platform_select_map(
        platform_variants,
        ":{}-".format(name_prefix) + "{platform_output}",
    ))

def platform_selected_targets(platform_variants, name_prefix = "feature-tree"):
    return select({
        key: [value]
        for key, value in _platform_select_map(
            platform_variants,
            ":{}-".format(name_prefix) + "{platform_output}",
        ).items()
    })

def platform_selected_codegen_srcs(platform_variants, static_srcs, name_prefix = "feature-tree"):
    return select({
        key: static_srcs | {
            "BuildDefines_generated.h": value + "-codegen[BuildDefines_generated.h]",
            "FeatureDefines_generated.h": value + "-codegen[FeatureDefines_generated.h]",
        }
        for key, value in _platform_select_map(
            platform_variants,
            ":{}-".format(name_prefix) + "{platform_output}",
        ).items()
    })

def generate_feature_trees_by_platform(
        platform_variants,
        app_name,
        srcs_by_platform,
        feature_overrides_by_platform = {},
        name_prefix = "feature-tree"):
    [
        generate_feature_tree(
            name = "{}-{}".format(name_prefix, platform_output_name(platform)),
            config_id = variant_id,
            srcs = srcs,
            feature_overrides = {
                "app_pcba_id": variant_id,
            } | feature_overrides_by_platform.get(platform, {}),
            **{app_name + "_config_id": str(variant_id) + "U"}
        )
        for platform, variant_id in platform_variants
        for srcs in [srcs_by_platform[platform]]
    ]

def add_platform_deploy_targets(platform_variants, app_name):
    [
        native.configured_alias(
            name = "crc-{}".format(platform_output_name(platform)),
            actual = ":bin_crc",
            platform = platform_target_label(platform),
            visibility = ["PUBLIC"],
        )
        for platform, _variant in platform_variants
    ]
    [
        conUDS_download(
            name = "download-{}".format(platform_output_name(platform)),
            binary = ":crc-{}".format(platform_output_name(platform)),
            manifest = "//network:manifest-uds",
            node = app_name,
        )
        for platform, _variant in platform_variants
    ]
    [
        deployable_target(
            name = "deploy-{}".format(platform_output_name(platform)),
            src = ":crc-{}".format(platform_output_name(platform)),
            target_node = app_name,
            visibility = ["PUBLIC"],
        )
        for platform, _variant in platform_variants
    ]
    [
        ota_agent(
            name = "ota-{}".format(platform_output_name(platform)),
            src = ":deploy-{}".format(platform_output_name(platform)),
            visibility = ["PUBLIC"],
        )
        for platform, _variant in platform_variants
    ]
