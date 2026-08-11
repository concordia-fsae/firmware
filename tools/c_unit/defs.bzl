load("@prelude//:rules.bzl", __rules__ = "rules")

def c_unit_test(
        name: str,
        srcs: list,
        headers: dict | None = None,
        deps: list | None = None,
        compiler_flags: list | None = None,
        linker_flags: list | None = None,
        run_name: str | None = None,
        visibility: list = ["PUBLIC"],
        target_compatible_with: list = ["prelude//os/constraints:linux"],
        **kwargs):
    headers = headers if headers != None else {}
    deps = deps if deps != None else []
    compiler_flags = compiler_flags if compiler_flags != None else []
    linker_flags = linker_flags if linker_flags != None else []

    __rules__["cxx_test"](
        name = name,
        srcs = srcs,
        header_namespace = "",
        headers = headers,
        compiler_flags = [
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
        ] + compiler_flags,
        linker_flags = linker_flags,
        deps = ["//embedded/libs:unity"] + deps,
        target_compatible_with = target_compatible_with,
        visibility = visibility,
        **kwargs
    )

    if run_name != None:
        native.command_alias(
            name = run_name,
            exe = ":" + name,
            visibility = visibility,
        )
