load("//tools/uv/defs.bzl", "uv_genrule", "uv_tool")
load("@prelude//:rules.bzl", __rules__ = "rules")

def generate_feature_tree(
        name: str,
        config_id: int | Select,
        srcs: list[str] | dict[str, str],
        node_id: int | None = None,
        kv: dict[str, str] = {},
        build_renderer: str | None = None):
    sets = ""
    exported_identifier = ".config-{}"

    if node_id:
        name = name + "-{}".format(node_id)
        exported_identifier = exported_identifier + "-{}".format(node_id)

    uv_name = name + "-codegen"
    build_renderer = build_renderer or "//tools/feature-tree:BuildDefines_generated.h.mako"

    srcs_qualified = srcs
    if type(srcs) == list:
        srcs_qualified = srcs_qualified + [
            build_renderer,
            "//tools/feature-tree:FeatureDefines_generated.h.mako",
        ]
    else:
        srcs_qualified = srcs_qualified | {
            "BuildDefines_generated.h.mako": build_renderer,
            "FeatureDefines_generated.h.mako": "//tools/feature-tree:FeatureDefines_generated.h.mako",
        }

    for k, v in kv.items():
        sets = sets + " --set " + "=".join([k, v])
    return [
        uv_genrule(
            name = uv_name,
            tool = "//tools/feature-tree:feature-tree",
            srcs = srcs_qualified,
            cmd = "$(python) ${TOOLDIR}/feature-tree.py --config-id " + "{}".format(config_id) +
                " --sources ${SRCS} --output ${OUT} " + sets,
            outs = {
                "BuildDefines_generated.h": ["BuildDefines_generated.h"],
                "FeatureDefines_generated.h": ["FeatureDefines_generated.h"],
            },
        ),
        __rules__["prebuilt_cxx_library"](
            name = name,
            header_only = True,
            header_namespace = "",
            exported_headers = {
                "FeatureDefines.h": "//tools/feature-tree:FeatureDefines.h",
                "BuildDefines_generated.h": ":" + uv_name + "[BuildDefines_generated.h]",
                "FeatureDefines_generated.h": ":" + uv_name + "[FeatureDefines_generated.h]",
            },
        ),
        __rules__["export_file"](
            name = "BuildDefines_generated.h" + exported_identifier.format(config_id),
            src = ":" + uv_name + "[BuildDefines_generated.h]",
        ),
        __rules__["export_file"](
            name = "FeatureDefines_generated.h" + exported_identifier.format(config_id),
            src = ":" + uv_name + "[FeatureDefines_generated.h]",
        ),

    ]

