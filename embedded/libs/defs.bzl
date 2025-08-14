load("@prelude//:rules.bzl", __rules__ = "rules")
load("@prelude//cxx/cxx_toolchain_types.bzl", "CxxToolchainInfo")

def cmsis_headers(
        name: str,
        toolchain,
        **kwargs):
    return __rules__["prebuilt_cxx_library"](
        name = name,
        _cxx_toolchain = toolchain,
        header_only = True,
        header_namespace = "",
        exported_headers = {
            "core_cm3.h": "//embedded/libs:CMSIS.git[CMSIS/Core/Include/core_cm3.h]",
            "cmsis_version.h": "//embedded/libs:CMSIS.git[CMSIS/Core/Include/cmsis_version.h]",
            "cmsis_compiler.h": "//embedded/libs:CMSIS.git[CMSIS/Core/Include/cmsis_compiler.h]",
            "cmsis_gcc.h": "//embedded/libs:CMSIS.git[CMSIS/Core/Include/cmsis_gcc.h]",
            "m-profile/cmsis_gcc_m.h": "//embedded/libs:CMSIS.git[CMSIS/Core/Include/m-profile/cmsis_gcc_m.h]",
        },
        **kwargs
    )
