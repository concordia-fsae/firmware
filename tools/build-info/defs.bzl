load("//tools/uv/defs.bzl", "uv_genrule", "uv_tool")
load("@prelude//:rules.bzl", __rules__ = "rules")

def generate_build_info(
        name: str,
        **kwargs):
    uv_name = name + "-codegen"
    templates = "--template {}".format
    return [
        uv_genrule(
            name = uv_name,
            tool = "//tools/build-info:build-info-tool",
            srcs = [
                "//tools/build-info:BuildInfo.h.mako",
                "//:.git",
            ],
            cmd = "$(python) ${TOOLDIR}/build-info.py --source-dir ${SRCDIR} --output ${OUT}",
            outs = {
                "BuildInfo.h": ["BuildInfo.h"],
            },
        ),
        __rules__["prebuilt_cxx_library"](
            name = name,
            header_only = True,
            header_namespace = "",
            exported_headers = {
                "BuildInfo.h": "//tools/build-info:" + uv_name + "[BuildInfo.h]",
            },
            **kwargs,
        ),
    ]

