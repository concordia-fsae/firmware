from os.path import join
from typing import Required, TypedDict

from SCons.Script import (
    Dir,  # pyright: ignore[reportUnknownVariableType, reportAttributeAccessIssue]
    Environment,
    File,  # pyright: ignore[reportUnknownVariableType, reportAttributeAccessIssue]
)
from yaml import safe_load


class Translation(TypedDict):
    line: str
    family: str
    chip: str


class Hal(TypedDict):
    src: str
    include: str


class Chip(TypedDict):
    chipDefinition: str


class Family(TypedDict):
    cmsis: str
    hal: Hal
    chips: dict[str, Chip]


class Line(TypedDict):
    basePath: str
    families: dict[str, Family]


class KnownChips(TypedDict):
    translation: dict[str, Translation]
    line: dict[str, Line]


class Driver(TypedDict, total=False):
    required: bool
    sources: list[str]
    headers: list[str]
    extra_flags: list[str]
    ll_available: bool
    ll_sources: str
    ll_headers: str
    ll_only: bool
    ll_extra_flags: list[str]


class KnownChipConfig(TypedDict):
    name: str
    extraSources: list[str]
    defaultLinkerFile: str
    defines: dict[str, str | None]
    drivers: dict[str, Driver]


class Linker(TypedDict):
    useDefault: bool


class DriverConfig(TypedDict, total=False):
    use_ll: bool


class ChipConfig(TypedDict):
    mcu: str
    linker: Linker
    drivers: dict[str, DriverConfig | None]


class ProcessedChipConfiguration(TypedDict, total=False):
    sources: Required[list[tuple[str, list[str]]]]
    defines: list[str]
    linker_file: File
    hal_includes: Dir
    cmsis_includes: Dir


def generate_defines(chip_config: KnownChipConfig) -> list[str]:
    defines: list[str] = []
    for key, val in chip_config["defines"].items():
        if val is not None:
            defines.append(f"-D{key}={val}")
        else:
            defines.append(f"-D{key}")

    return defines


def _configure_chip(env: Environment, config_file: str) -> ProcessedChipConfiguration:
    REPO_ROOT_DIR: Dir = env["REPO_ROOT_DIR"]
    KNOWN_CHIPS_FILE: File = REPO_ROOT_DIR.File("site_scons/chips.yaml")

    with open(KNOWN_CHIPS_FILE.abspath, "r") as known_chips_file_handle:
        known_chips: KnownChips = safe_load(known_chips_file_handle)

    with open(File(config_file).abspath, "r") as chip_file_handle:
        chip_config: ChipConfig = safe_load(chip_file_handle)

    try:
        chip_name = chip_config["mcu"].lower()
    except KeyError as e:
        raise Exception(f"Expected an 'mcu' key in mcuConfig.yaml\n{e}")

    try:
        chip = known_chips["translation"][chip_name]
    except KeyError as e:
        raise Exception(
            f"{chip_name} specified in {config_file} does not have a translation. See site_scons/chips.yaml"
        )

    chip_line = known_chips["line"][chip["line"]]
    chip_family = chip_line["families"][chip["family"]]
    chip_file = chip_family["chips"][chip["chip"]]["chipDefinition"]
    base_path = chip_line["basePath"]
    chip_config_file = join(base_path, chip_file)
    hal_src_path = join(base_path, chip_line["families"][chip["family"]]["hal"]["src"])

    with open(REPO_ROOT_DIR.File(chip_config_file).abspath) as chip_config_file_handle:
        global_chip_config: KnownChipConfig = safe_load(chip_config_file_handle)

    required_drivers = [
        driver
        for driver in global_chip_config["drivers"]
        if global_chip_config["drivers"][driver].get("required")
    ]

    missing_drivers = [
        driver for driver in required_drivers if driver not in chip_config["drivers"]
    ]

    if missing_drivers:
        raise Exception(
            "Certain drivers that are required for this chip are "
            + "included in mcuConfig.yaml. Missing drivers:\n"
            + f"{missing_drivers}"
        )

    drivers = chip_config["drivers"]
    known_drivers = global_chip_config["drivers"]

    chip_source_files: dict[str, list[str]] = {}

    for driver, driver_settings in drivers.items():
        if driver not in known_drivers:
            raise Exception(
                f"Driver '{driver}' does not exist in global config for this chip"
            )

        if "extra_flags" not in known_drivers[driver]:
            known_drivers[driver]["extra_flags"] = []

        if driver_settings is not None and driver_settings.get("use_ll"):
            if known_drivers[driver].get("ll_available"):
                if "ll_extra_flags" not in known_drivers[driver]:
                    known_drivers[driver]["ll_extra_flags"] = []

                assert (
                    "ll_sources" in known_drivers[driver]
                ), "Missing ll_sources from driver with ll_available set to True"

                for path in known_drivers[driver].get("ll_sources", []):
                    chip_source_files[path] = known_drivers[driver].get(
                        "ll_extra_flags", []
                    )
            else:
                raise Exception(
                    f"Driver '{driver}' has 'use_ll: true', but this driver does not have a low level version"
                )
        elif known_drivers[driver].get("ll_only"):
            for path in known_drivers[driver].get("ll_sources", []):
                chip_source_files[path] = known_drivers[driver].get(
                    "ll_extra_flags", []
                )
        else:
            for path in known_drivers[driver].get("sources", []):
                chip_source_files[path] = known_drivers[driver].get("extra_flags", [])

    chip_source_files_full_path: list[tuple[str, list[str]]] = [
        (
            REPO_ROOT_DIR.Dir(base_path).Dir(chip["family"]).File(file),
            [],
        )
        for file in global_chip_config.get("extraSources", [])
    ]

    chip_source_files_full_path.extend(
        [
            (REPO_ROOT_DIR.Dir(hal_src_path).File(file), chip_source_files[file])
            for file in chip_source_files
        ]
    )

    ret: ProcessedChipConfiguration = {"sources": chip_source_files_full_path}

    if "defines" in global_chip_config:
        ret["defines"] = generate_defines(global_chip_config)

    if chip_config.get("linker", {}).get("useDefault"):
        if linker_file := global_chip_config.get("defaultLinkerFile"):
            ret["linker_file"] = (
                REPO_ROOT_DIR.Dir(base_path).Dir(chip["family"]).File(linker_file)
            )

    if chip_family.get("hal", {}).get("include"):
        ret["hal_includes"] = REPO_ROOT_DIR.Dir(base_path).Dir(
            chip_family["hal"]["include"]
        )

    if chip_family.get("cmsis"):
        ret["cmsis_includes"] = REPO_ROOT_DIR.Dir(base_path).Dir(chip_family["cmsis"])

    return ret


def generate(env: Environment):
    env.AddMethod(_configure_chip, "ChipConfig")


def exists():
    return True
