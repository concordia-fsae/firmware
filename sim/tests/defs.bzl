load("//components/bms_worker:defs.bzl", "BMSW_NODE_COUNT_BY_PLATFORM")
load("//components/vehicle_platform:platforms.bzl", "ALL_PLATFORMS", "platform_output_name")
load("//sim/bindings:defs.bzl", "rig_platform_node_sim_lib_env", "rig_platform_node_sim_lib_resources", "rig_platform_sim_lib_env", "rig_platform_sim_lib_resources", "rig_platform_variants_env", "rig_pytest")


def define_tests(
        name,
        test_file,
        env,
        resources,
        models = [],
        node_models = []):
    """Define one isolated Buck test process for each firmware platform.

    ``models`` contains ``(environment_prefix, model_target)`` pairs for
    ordinary platform-specific native libraries. ``node_models`` additionally
    takes a platform node-count map and exposes only the selected platform's
    node libraries. The caller owns the test suite declaration so this helper
    remains usable from the package BUCK dialect.
    """
    for platform in ALL_PLATFORMS:
        platform_env = env | rig_platform_variants_env([platform])
        platform_resources = resources
        for prefix, model in models:
            platform_env = platform_env | rig_platform_sim_lib_env(prefix, model, [platform])
            platform_resources = platform_resources + rig_platform_sim_lib_resources(model, [platform])
        for prefix, model, node_counts in node_models:
            platform_nodes = [
                (platform, node)
                for node in range(node_counts[platform_output_name(platform)])
            ]
            platform_env = platform_env | rig_platform_node_sim_lib_env(prefix, model, platform_nodes)
            platform_resources = platform_resources + rig_platform_node_sim_lib_resources(model, platform_nodes)
        rig_pytest(
            name = "{}-{}".format(name, platform_output_name(platform)),
            test_file = test_file,
            env = platform_env,
            resources = platform_resources,
            visibility = ["PUBLIC"],
        )
