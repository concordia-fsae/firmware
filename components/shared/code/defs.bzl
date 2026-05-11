load("@prelude//:rules.bzl", __rules__ = "rules")
load("//tools/defs.bzl", "remap_files")
load("//tools/c_unit:defs.bzl", "c_unit_test")

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

shared_lib_test_headers = {
    "BuildDefines.h": "tests/libs/stubs/BuildDefines.h",
    "FeatureDefines.h": "tests/libs/stubs/shared_lib_stubs.h",
    "FeatureDefines_generated.h": "tests/libs/stubs/shared_lib_stubs.h",
    "FreeRTOS.h": "tests/libs/stubs/FreeRTOS.h",
    "HW_tim.h": "tests/libs/stubs/HW_tim.h",
    "LIB_FloatTypes.h": "//components/shared/code:libs/LIB_FloatTypes.h",
    "LIB_Types.h": "//components/shared/code:libs/LIB_Types.h",
    "LIB_app.h": "//components/shared/code:libs/LIB_app.h",
    "LIB_app_config.h": "tests/libs/stubs/LIB_app_config.h",
    "Types.h": "//components/shared/code:libs/Types.h",
    "Utility.h": "//components/shared/code:libs/Utility.h",
    "drv_imu.h": "tests/libs/stubs/drv_imu.h",
    "drv_timer.h": "//components/shared/code:DRV/drv_timer.h",
    "lib_buffer.h": "//components/shared/code:libs/lib_buffer.h",
    "lib_interpolation.h": "//components/shared/code:libs/lib_interpolation.h",
    "lib_linAlg.h": "//components/shared/code:libs/lib_linAlg.h",
    "lib_madgwick.h": "//components/shared/code:libs/lib_madgwick.h",
    "lib_nvm.h": "//components/shared/code:libs/lib_nvm.h",
    "lib_nvm_componentSpecific.h": "tests/libs/stubs/lib_nvm_componentSpecific.h",
    "lib_pid.h": "//components/shared/code:libs/lib_pid.h",
    "lib_rateLimit.h": "//components/shared/code:libs/lib_rateLimit.h",
    "lib_simpleFilter.h": "//components/shared/code:libs/lib_simpleFilter.h",
    "lib_swFuse.h": "//components/shared/code:libs/lib_swFuse.h",
    "lib_thermistors.h": "//components/shared/code:libs/lib_thermistors.h",
    "lib_utility.h": "//components/shared/code:libs/lib_utility.h",
    "lib_voltageDivider.h": "//components/shared/code:libs/lib_voltageDivider.h",
    "libcrc.h": "//embedded/libs:libcrc.h",
    "libcrc_componentSpecific.h": "tests/libs/stubs/libcrc_componentSpecific.h",
    "semphr.h": "tests/libs/stubs/semphr.h",
    "shared_lib_stubs.h": "tests/libs/stubs/shared_lib_stubs.h",
}

shared_lib_test_flags = [
    "-Wno-macro-redefined",
    "-Wno-missing-field-initializers",
    "-Wno-gnu-statement-expression",
    "-include",
    "shared_lib_stubs.h",
]

def shared_lib_test(name, srcs, headers = None, compiler_flags = None, linker_flags = None):
    headers = headers if headers != None else {}
    compiler_flags = compiler_flags if compiler_flags != None else []
    linker_flags = linker_flags if linker_flags != None else ["-lm"]
    c_unit_test(
        name = name,
        srcs = srcs,
        headers = shared_lib_test_headers | headers,
        compiler_flags = shared_lib_test_flags + compiler_flags,
        linker_flags = linker_flags,
        coverage = True,
    )
