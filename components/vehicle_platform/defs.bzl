load("@prelude//:rules.bzl", __rules__ = "rules")
load("//drive-stack/conUDS/defs.bzl", "conUDS_download")
load("//drive-stack/defs.bzl", "deployable_target")
load("//drive-stack/ota-agent/defs.bzl", "ota_agent")
load("//tools/feature-tree/defs.bzl", "generate_feature_tree")
load("//tools/yamcan/defs.bzl", "generate_c_library", "generate_resources")
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

def _platform_srcs_for_all(platform_variants, srcs):
    return {
        platform: srcs
        for platform, _variant in platform_variants
    }

def _generate_feature_trees_by_platform(
        platform_variants,
        app_name,
        srcs_by_platform,
        feature_overrides_by_platform = {},
        name_prefix = "feature-tree"):
    [
        generate_feature_tree(
            name = "{}-{}".format(name_prefix, platform_output_name(platform)),
            variant_id = variant_id,
            srcs = srcs,
            feature_overrides = {
                "app_variant_id": variant_id,
            } | feature_overrides_by_platform.get(platform, {}),
            **{app_name + "_variant_id": str(variant_id) + "U"}
        )
        for platform, variant_id in platform_variants
        for srcs in [srcs_by_platform[platform]]
    ]

def _add_platform_selected_target_alias(
        name,
        platform_variants,
        name_prefix = "feature-tree",
        visibility = None):
    native.alias(
        name = name,
        actual = platform_selected_target(platform_variants, name_prefix),
        visibility = visibility,
    )

def add_platform_target_aliases(
        platform_variants,
        targets,
        visibility = None):
    [
        native.configured_alias(
            name = "{}-{}".format(target, platform_output_name(platform)),
            actual = ":{}".format(target),
            platform = platform_target_label(platform),
            visibility = visibility,
        )
        for platform, _variant in platform_variants
        for target in targets
    ]

def add_platform_feature_tree_targets(
        platform_variants,
        app_name,
        srcs = None,
        srcs_by_platform = None,
        feature_overrides_by_platform = {},
        name_prefix = "feature-tree",
        selected_name = "sil-features",
        selected_visibility = None):
    if srcs == None and srcs_by_platform == None:
        fail("add_platform_feature_tree_targets requires srcs or srcs_by_platform")
    if srcs != None and srcs_by_platform != None:
        fail("add_platform_feature_tree_targets accepts either srcs or srcs_by_platform, not both")

    _generate_feature_trees_by_platform(
        platform_variants = platform_variants,
        app_name = app_name,
        srcs_by_platform = srcs_by_platform if srcs_by_platform != None else _platform_srcs_for_all(platform_variants, srcs),
        feature_overrides_by_platform = feature_overrides_by_platform,
        name_prefix = name_prefix,
    )
    _add_platform_selected_target_alias(
        name = selected_name,
        platform_variants = platform_variants,
        name_prefix = name_prefix,
        visibility = selected_visibility,
    )
    add_platform_target_aliases(
        platform_variants = platform_variants,
        targets = [selected_name],
        visibility = selected_visibility,
    )

def add_platform_sil_bundle_targets(
        platform_variants,
        app_name,
        feature_srcs = None,
        feature_srcs_by_platform = None,
        feature_overrides_by_platform = {},
        visibility = None,
        host_headers = ":sil-host-headers",
        yamcan_compiler_flags = [],
        yamcan_deps = [],
        app_srcs = [],
        app_compiler_flags = [],
        app_headers = {},
        app_deps = []):
    add_platform_feature_tree_targets(
        platform_variants = platform_variants,
        app_name = app_name,
        srcs = feature_srcs,
        srcs_by_platform = feature_srcs_by_platform,
        feature_overrides_by_platform = feature_overrides_by_platform,
        selected_visibility = visibility,
    )

    generate_resources(
        name = "sil-yamcan",
        network_dep = "//network:network",
        node = app_name,
        rust_wrapper = True,
        visibility = visibility,
    )

    generate_c_library(
        name = "sil-yamcan-c",
        codegen_target = ":sil-yamcan",
        compiler_flags = yamcan_compiler_flags,
        library_deps = [
            ":sil-features",
            host_headers,
        ] + yamcan_deps,
        preferred_linkage = "static",
        visibility = visibility,
    )

    __rules__["cxx_library"](
        name = "sil-application",
        srcs = app_srcs,
        compiler_flags = app_compiler_flags,
        header_namespace = "",
        headers = app_headers,
        deps = [
            ":sil-features",
            host_headers,
            ":sil-yamcan-c",
        ] + app_deps,
        preferred_linkage = "static",
        visibility = visibility,
    )

    add_platform_target_aliases(
        platform_variants = platform_variants,
        targets = [
            "sil-yamcan-c",
            "sil-application",
        ],
        visibility = visibility,
    )

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
            platform = platform_output_name(platform),
            visibility = ["PUBLIC"],
        )
        for platform, _variant in platform_variants
    ]
