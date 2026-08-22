load(
    "//components/vehicle_platform:platforms.bzl",
    "platform_constraint_label",
    "platform_output_name",
    "platform_target_name",
)

# Every BMS worker on the vehicle has its own firmware image and CAN node.
# Keep this topology in the component definition so embedded and SIL builds
# cannot silently diverge.
BMSW_NODE_COUNT_BY_PLATFORM = {
    "cfr25": 6,
    "cfr26": 8,
}

def configured_platform_name(platform, node):
    return "{}-node-{}".format(platform_target_name(platform), node)

def node_target_name(base, platform, node):
    return "{}-{}-node-{}".format(base, platform_output_name(platform), node)

def all_nodes_target_name(base, platform):
    return "{}-{}.all_nodes".format(base, platform_output_name(platform))

def feature_tree_target(platform, node):
    return ":feature-tree-{}-node-{}".format(
        platform_output_name(platform),
        node,
    )

def _default_platform(platform_variants):
    return platform_variants[0][0]

def _node_select(platform, node_count):
    return {
        ":node-{}".format(node): [feature_tree_target(platform, node)]
        for node in range(node_count)
    } | {
        # Platform-only consumers (for example the legacy unconfigured SIL
        # aliases) retain node 0 as their deterministic default. Configured
        # per-node platforms select the matching feature tree above.
        "DEFAULT": [feature_tree_target(platform, 0)],
    }

def feature_tree_targets(platform_variants):
    select_map = {
        platform_constraint_label(platform): select(_node_select(platform, node_count))
        for platform, _variant_id, node_count in platform_variants
    }
    default_platform = _default_platform(platform_variants)
    default_node_count = [node_count for platform, _variant_id, node_count in platform_variants if platform == default_platform][0]
    select_map["DEFAULT"] = select(
        _node_select(default_platform, default_node_count) | {
            "DEFAULT": [feature_tree_target(default_platform, 0)],
        },
    )
    return select(select_map)

def feature_tree_codegen_srcs(platform_variants, static_srcs):
    select_map = {
        platform_constraint_label(platform): select({
            ":node-{}".format(node): static_srcs | {
                "BuildDefines_generated.h": feature_tree_target(platform, node) + "-codegen[BuildDefines_generated.h]",
                "FeatureDefines_generated.h": feature_tree_target(platform, node) + "-codegen[FeatureDefines_generated.h]",
            }
            for node in range(node_count)
        } | {
            "DEFAULT": static_srcs | {
                "BuildDefines_generated.h": feature_tree_target(platform, 0) + "-codegen[BuildDefines_generated.h]",
                "FeatureDefines_generated.h": feature_tree_target(platform, 0) + "-codegen[FeatureDefines_generated.h]",
            },
        })
        for platform, _variant_id, node_count in platform_variants
    }
    default_platform = _default_platform(platform_variants)
    default_node_count = [node_count for platform, _variant_id, node_count in platform_variants if platform == default_platform][0]
    select_map["DEFAULT"] = select({
        ":node-{}".format(node): static_srcs | {
            "BuildDefines_generated.h": feature_tree_target(default_platform, node) + "-codegen[BuildDefines_generated.h]",
            "FeatureDefines_generated.h": feature_tree_target(default_platform, node) + "-codegen[FeatureDefines_generated.h]",
        }
        for node in range(default_node_count)
    } | {
        "DEFAULT": static_srcs | {
            "BuildDefines_generated.h": feature_tree_target(default_platform, 0) + "-codegen[BuildDefines_generated.h]",
            "FeatureDefines_generated.h": feature_tree_target(default_platform, 0) + "-codegen[FeatureDefines_generated.h]",
        },
    })
    return select(select_map)
