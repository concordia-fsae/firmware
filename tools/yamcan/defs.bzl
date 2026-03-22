load("@prelude//:rules.bzl", "cxx_library")
load("//tools/defs.bzl", "remap_files")
load("@prelude//:rules.bzl", __rules__ = "rules")
load("//tools/uv/defs.bzl", "uv_genrule", "uv_tool")
load("//components/vehicle_platform:platforms.bzl", "PLATFORM", "platform_output_name", "platform_target_label")

_GENERATED_HEADERS = {
    "CANTypes_generated.h": "CANTypes_generated.h",
    "MessagePack_generated.h": "MessagePack_generated.h",
    "MessageUnpack_generated.h": "MessageUnpack_generated.h",
    "NetworkDefines_generated.h": "NetworkDefines_generated.h",
    "SigRx.h": "SigRx.h",
    "SigTx.h": "SigTx.h",
    "TemporaryStubbing.h": "TemporaryStubbing.h",
}

_GENERATED_SOURCES = [
    "generated/MessagePack_generated.c",
    "generated/MessageUnpack_generated.c",
]

def _codegen_outs(rust_wrapper: bool = False):
    outs = {
        key: ["generated/{}".format(val)]
        for key, val in _GENERATED_HEADERS.items()
    }
    outs["MessagePack_generated.c"] = ["generated/MessagePack_generated.c"]
    outs["MessageUnpack_generated.c"] = ["generated/MessageUnpack_generated.c"]
    outs["sources"] = _GENERATED_SOURCES
    if rust_wrapper:
        outs["lib.rs"] = ["generated/lib.rs"]
        outs["yamcan.rs"] = ["generated/yamcan.rs"]
        outs["rust_model_generated.rs"] = ["generated/rust_model_generated.rs"]
        outs["rust_decode_generated.rs"] = ["generated/rust_decode_generated.rs"]
    return outs

def _codegen_cmd(node: str, rust_wrapper: bool = False):
    cmd = (
        "$(python) ${TOOLDIR}/yamcan.py" +
        " --cache-dir ${SRCS}" +
        " --node {}".format(node) +
        " --codegen-dir ${OUT}/generated/"
    )
    if rust_wrapper:
        cmd += " --bridge-codegen"
        cmd += (
            " && cp ${TOOLDIR}/srcs/rust/lib.rs ${OUT}/generated/lib.rs" +
            " && cp ${TOOLDIR}/srcs/rust/yamcan.rs ${OUT}/generated/yamcan.rs"
        )
    return cmd

def generate_resources(
        name: str,
        network_dep: str,
        node: str,
        rust_wrapper: bool = False,
        **kwargs):
    uv_genrule(
        name = name,
        tool = "//tools/yamcan:yamcan",
        outs = _codegen_outs(rust_wrapper),
        cmd = _codegen_cmd(node, rust_wrapper),
        srcs = [network_dep],
        **kwargs
    )

def generate_c_library(
        name: str,
        codegen_target: str,
        library_deps: list[str] | None = None,
        extra_srcs: list[str] | None = None,
        extra_exported_headers: dict[str, str] | None = None,
        **kwargs):
    exported_headers = {
        header: codegen_target + "[{}]".format(header)
        for header in _GENERATED_HEADERS.keys()
    }
    if extra_exported_headers:
        exported_headers.update(extra_exported_headers)

    srcs = [codegen_target + "[sources]"] + (extra_srcs if extra_srcs else [])

    cxx_library(
        name = name,
        deps = library_deps if library_deps else [],
        header_namespace = "",
        srcs = srcs,
        exported_headers = exported_headers,
        headers = {
            "lib_atomic.h": "//embedded/libs:lib_atomic.h",
        },
        **kwargs
    )

def generate_rust_library(
        name: str,
        crate: str,
        codegen_target: str,
        c_dep: str,
        deps: list[str] | None = None,
        **kwargs):
    __rules__["rust_library"](
        name = name,
        crate = crate,
        crate_root = "generated/lib.rs",
        srcs = [
            codegen_target + "[lib.rs]",
            codegen_target + "[yamcan.rs]",
            codegen_target + "[rust_model_generated.rs]",
            codegen_target + "[rust_decode_generated.rs]",
        ],
        edition = "2024",
        deps = [c_dep] + (deps if deps else []),
        **kwargs
    )

def generate_manifest(name: str, dep: str, filters: list[str], ignore_nodes: list[str] | None = None, **kwargs):
    args = ""
    for node in ignore_nodes:
        args += " --ignore-node {}".format(node)

    for filt in filters:
        args += " --manifest-filter {}".format(filt)

    return uv_genrule(
        name = name,
        tool = "//tools/yamcan:yamcan",
        # Named output dir called "include"
        out = "manifest.yaml",
        cmd = (
            "$(python) ${TOOLDIR}/yamcan.py" +
            " --cache-dir ${SRCS}" +
            " --manifest-output ${OUT}" +
            args
        ),
        srcs = [dep],
        **kwargs,
    )

def generate_code(
        name: str,
        network_dep: str,
        node: str,
        library_deps: list[str] | None = None,
        **kwargs):
    codegen_name = name + "-codegen"
    generate_resources(
        name = codegen_name,
        network_dep = network_dep,
        node = node,
    )
    generate_c_library(
        name = name,
        codegen_target = ":" + codegen_name,
        library_deps = library_deps,
        **kwargs
    )

def generate_stats(
        name: str,
        dep: str,
        **kwargs):
    uv_genrule(
        name = name,
        tool = "//tools/yamcan:yamcan",
        out = "network",
        cmd = "$(python) ${TOOLDIR}/yamcan.py --output-dir ${OUT} --cache-dir ${SRCS} --gen-stats",
        srcs = [dep],
        **kwargs
    )

def generate_dbcs(
        name: str,
        dep: str,
        **kwargs):
    uv_genrule(
        name = name,
        tool = "//tools/yamcan:yamcan",
        out = "network",
        cmd = "$(python) ${TOOLDIR}/yamcan.py --output-dir ${OUT} --cache-dir ${SRCS} --gen-dbc",
        srcs = [dep],
        **kwargs
    )

def build_network(
        name: str,
        data_dir: str,
        platform: PLATFORM | None = None,
        **kwargs):
    visibility = kwargs.get("visibility")
    if "visibility" in kwargs:
        kwargs = dict(kwargs)
        kwargs.pop("visibility")
    configured_platform = "prelude//platforms:default"
    name_suffix = "default"
    if platform != None:
        configured_platform = platform_target_label(platform)
        name_suffix = platform_output_name(platform)
    defs = remap_files(data_dir, glob([data_dir + "**/*.yaml"]))
    __rules__["filegroup"](
        name = "network-defs",
        srcs = defs,
        visibility = visibility,
    )
    uv_genrule(
        name = name + "-{}".format(name_suffix),
        tool = "//tools/yamcan:yamcan",
        srcs = [":network-defs"],
        out = "network-cache",
        cmd = "$(python) ${TOOLDIR}/yamcan.py --data-dir ${SRCS} --cache-dir ${OUT} --build",
        visibility = visibility,
        **kwargs
    )
    native.configured_alias(
        name = name,
        actual = ":" + name + "-{}".format(name_suffix),
        platform = configured_platform,
        visibility = visibility,
    )
