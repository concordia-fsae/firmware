load("@prelude//:rules.bzl", __rules__ = "rules")

def shared_shockpot_library(
        name,
        toolchain,
        componentSpecific_header_target,
        deps = [],
        srcs = [],
        headers = {},
        visibility = None):

    all_deps = deps + [componentSpecific_header_target,]

    return __rules__["cxx_library"](
        name = name,
        _cxx_toolchain = toolchain,
        header_namespace = "",
        srcs = srcs,
        headers = headers,
        deps = all_deps,
        visibility = visibility,
        )
