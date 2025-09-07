load("@prelude//:rules.bzl", __rules__ = "rules")
load("//tools/defs.bzl", "remap_files")

def remap_headers(bases):
    remap = {}
    for base in bases:
        hdrs = native.glob([base + "**/*.h"])
        remap = remap | remap_files(base, hdrs)
    return remap

def shared_code_library(
        name: str,
        toolchain,
        **kwargs):
    return __rules__["cxx_library"](
        name = name,
        _cxx_toolchain = toolchain,
        header_namespace = "",
        exported_deps = ["//components/shared/code:headers"],
        **kwargs
    )
