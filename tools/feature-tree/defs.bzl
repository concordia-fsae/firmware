load("@prelude//:rules.bzl", __rules__ = "rules")
load("@prelude//utils:utils.bzl", "flatten")
load("//tools/uv/defs.bzl", "uv_genrule")

DEFAULT_RENDERER = "//tools/feature-tree:BuildDefines_generated.h.mako"

def generate_feature_tree(
        name: str,
        config_id: int | Select,
        srcs: list[str] | dict[str, str],
        node_id: int | None = None,
        build_renderer: str = DEFAULT_RENDERER,
        **kwargs):
    exported_identifier = ".config-{}"

    if node_id:
        name = "{}-{}".format(name, node_id)
        exported_identifier = "{}-{}".format(exported_identifier, node_id)

    uv_name = name + "-codegen"

    srcs_qualified = srcs
    if isinstance(srcs, list):
        srcs_qualified = srcs_qualified + [
            build_renderer,
            "//tools/feature-tree:FeatureDefines_generated.h.mako",
        ]
    else:
        srcs_qualified = srcs_qualified | {
            "BuildDefines_generated.h.mako": build_renderer,
            "FeatureDefines_generated.h.mako": "//tools/feature-tree:FeatureDefines_generated.h.mako",
        }

    extra_args = " ".join(["--set {}={}".format(k, v) for k, v in kwargs.items()])
    uv_genrule(
        name = uv_name,
        tool = "//tools/feature-tree:feature-tree",
        srcs = srcs_qualified,
        cmd = "$(python) ${TOOLDIR}/feature-tree.py --config-id " + str(config_id) +
              " --sources ${SRCS} --output ${OUT} " + extra_args,
        outs = {
            "BuildDefines_generated.h": ["BuildDefines_generated.h"],
            "FeatureDefines_generated.h": ["FeatureDefines_generated.h"],
        },
    )
    __rules__["prebuilt_cxx_library"](
        name = name,
        header_only = True,
        header_namespace = "",
        exported_headers = {
            "FeatureDefines.h": "//tools/feature-tree:FeatureDefines.h",
            "BuildDefines_generated.h": ":{}[BuildDefines_generated.h]".format(uv_name),
            "FeatureDefines_generated.h": ":{}[FeatureDefines_generated.h]".format(uv_name),
        },
    )
