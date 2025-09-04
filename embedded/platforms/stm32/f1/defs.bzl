load("@prelude//:rules.bzl", __rules__ = "rules")
load("@prelude//cxx/cxx_toolchain_types.bzl", "CxxToolchainInfo")
load("@prelude//utils:expect.bzl", "expect")
load("//embedded/libs/defs.bzl", "cmsis_headers")
load(":chips.bzl", "chips")

def stm32f1_hal(
        name: str,
        toolchain: str,
        compiler_flags: list,
        variant: str,
        hal_conf_header: str,
        config: dict,
        **kwargs):
    # check all input data
    expect(variant in chips, "Unknown chip variant '{}'. See embedded/platforms/stm32/f1/chips.bzl for supported chip variants".format(variant))
    chip = chips[variant]

    expect("drivers" in config, "Expected key 'drivers' not present in config!")
    drivers = chip["drivers"]

    required_drivers = [driver for driver in drivers if drivers[driver].get("required")]
    for driver in required_drivers:
        expect(driver in config["drivers"], "Required driver '{}' is not present in the config".format(driver))

    # start building src and header lists
    srcs = chip.get("srcs", [])[:]
    headers = chip.get("headers", [])[:]

    for driver, cfg in config["drivers"].items():
        expect(driver in chip["drivers"], "Unknown driver '{}' in config".format(driver))
        driver_config = drivers[driver]

        if cfg.get("use_ll"):
            expect("ll" in driver_config, "Low-level driver requested for driver '{}' but it is not supported".format(driver))
            srcs.extend(driver_config["ll"].get("srcs", []))
            headers.extend(driver_config["ll"].get("headers", []))
            continue
        else:
            expect("srcs" in driver_config or "headers" in driver_config, "Low-level driver was not requested for driver '{}' but it is required".format(driver))
            srcs.extend(driver_config.get("srcs", []))
            headers.extend(driver_config.get("headers", []))

    srcs_qualified = [
        "//embedded/platforms/stm32/f1:cmsis.git[Source/Templates/system_stm32f1xx.c]",
    ]
    headers_qualified = {
        "stm32f1xx_hal_conf.h": hal_conf_header,
        "stm32f1xx.h": "//embedded/platforms/stm32/f1:cmsis.git[Include/stm32f1xx.h]",
        "stm32f103xb.h": "//embedded/platforms/stm32/f1:cmsis.git[Include/stm32f103xb.h]",
        "system_stm32f1xx.h": "//embedded/platforms/stm32/f1:cmsis.git[Include/system_stm32f1xx.h]",
    }

    HAL_PATH = "//embedded/platforms/stm32/f1:hal.git[{}]"
    for src in srcs:
        if isinstance(src, str):
            srcs_qualified.append(HAL_PATH.format(src))
        elif isinstance(src, tuple):
            # if a source file is a tuple, the second element is the extra flags to compile it with
            srcs_qualified.append((HAL_PATH.format(src[0]), src[1]))

    for header in headers:
        if isinstance(header, str):
            headers_qualified[header.split("/")[-1]] = HAL_PATH.format(header)
        elif isinstance(header, tuple):
            # if a header file is a tuple, the first element is where the header should be located/named and the second
            # element is the path to the header
            headers_qualified[header[0]] = HAL_PATH.format(header[1])

    cmsis_headers(
        name = "CMSIS",
        toolchain = toolchain,
    )

    return __rules__["cxx_library"](
        name = name,
        _cxx_toolchain = toolchain,
        compiler_flags = compiler_flags,
        srcs = srcs_qualified,
        header_namespace = "",
        exported_headers = headers_qualified,
        deps = [
            ":CMSIS",
        ],
        **kwargs
    )
