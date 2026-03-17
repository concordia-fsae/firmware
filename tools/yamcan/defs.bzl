load("@prelude//:rules.bzl", "cxx_library")
load("//tools/defs.bzl", "remap_files")
load("@prelude//:rules.bzl", __rules__ = "rules")
load("//tools/uv/defs.bzl", "uv_genrule", "uv_tool")
load("//components/vehicle_platform:platforms.bzl", "PLATFORM", "platform_output_name", "platform_target_label")

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

def generate_code(name: str, network_dep: str, node: str, library_deps: list[str] | None = None, **kwargs):
    uv_name = name + "-codegen"
    cxx_deps = [":" + uv_name] + library_deps if library_deps else []
    return [
        uv_genrule(
            name = uv_name,
            tool = "//tools/yamcan:yamcan",
            # Named output dir called "include"
            outs = {
                "CANTypes_generated.h": ["generated/CANTypes_generated.h"],
                "MessagePack_generated.h": ["generated/MessagePack_generated.h"],
                "MessageUnpack_generated.h": ["generated/MessageUnpack_generated.h"],
                "NetworkDefines_generated.h": ["generated/NetworkDefines_generated.h"],
                "SigRx.h": ["generated/SigRx.h"],
                "SigTx.h": ["generated/SigTx.h"],
                "TemporaryStubbing.h": ["generated/TemporaryStubbing.h"],
                "sources": [
                    "generated/MessagePack_generated.c",
                    "generated/MessageUnpack_generated.c",
                ],
            },
            cmd = (
                "$(python) ${TOOLDIR}/yamcan.py" +
                " --cache-dir ${SRCS}" +
                " --node {}".format(node) +
                " --codegen-dir ${OUT}/generated/"
            ),
            srcs = [network_dep],
        ),
        cxx_library(
            name = name,
            deps = cxx_deps,
            header_namespace = "",
            srcs = [":" + uv_name + "[sources]"],
            exported_headers = {
                "CANTypes_generated.h": ":" + uv_name + "[CANTypes_generated.h]",
                "MessagePack_generated.h": ":" + uv_name + "[MessagePack_generated.h]",
                "MessageUnpack_generated.h": ":" + uv_name + "[MessageUnpack_generated.h]",
                "NetworkDefines_generated.h": ":" + uv_name + "[NetworkDefines_generated.h]",
                "SigRx.h": ":" + uv_name + "[SigRx.h]",
                "SigTx.h": ":" + uv_name + "[SigTx.h]",
                "TemporaryStubbing.h": ":" + uv_name + "[TemporaryStubbing.h]",
            },
            headers = {
                "lib_atomic.h": "//embedded/libs:lib_atomic.h",
            },
            **kwargs
        ),
    ]

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
