from yaml import load, Loader

from SCons.Script import *

from os.path import join


def generate_defines(chip_config):
    defines = []
    for key, val in chip_config["defines"].items():
        if not val is None:
            defines.append(f"-D{key}={val}")
        else:
            defines.append(f"-D{key}")

    return defines


def _configure_chip(env, config_file):
    REPO_ROOT_DIR = env["REPO_ROOT_DIR"]
    KNOWN_CHIPS_FILE = REPO_ROOT_DIR.File("site_scons/chips.yaml")

    with open(str(KNOWN_CHIPS_FILE), "r") as known_chips_file_handle:
        known_chips = load(known_chips_file_handle, Loader)

    with open(str(File(config_file)), "r") as chip_file_handle:
        chip_config = load(chip_file_handle, Loader)

    try:
        chip_name = chip_config["mcu"].lower()
    # FIXME: figure out which exception to catch here
    except Exception as e:
        print("Expected an 'mcu' key in mcuConfig.yaml")
        raise e

    try:
        chip = known_chips["translation"][chip_name]
    # FIXME: figure out which exception to catch here
    except Exception as e:
        print(
            "Chip specified in mcuConfig.yaml does not have a translation. See site_scons/chips.yaml"
        )
        raise e

    chip_line = known_chips["line"][chip["line"]]
    chip_family = chip_line["families"][chip["family"]]
    chip_file = chip_family["chips"][chip["chip"]]
    base_path = chip_line["basePath"]
    chip_config_file = join(base_path, chip_file)
    hal_src_path = join(base_path, chip_line["families"][chip["family"]]["hal"]["src"])

    with open(REPO_ROOT_DIR.File(chip_config_file).abspath) as chip_config_file_handle:
        global_chip_config = load(chip_config_file_handle, Loader)

    required_drivers = [
        driver
        for driver in global_chip_config["drivers"]
        if global_chip_config["drivers"][driver].get("required", False)
    ]

    missing_drivers = [
        driver for driver in required_drivers if driver not in chip_config["drivers"]
    ]

    if missing_drivers:
        raise Exception(
            "Certain drivers that are required for this chip are "
            "included in mcuConfig.yaml. Missing drivers:\n"
            f"{missing_drivers}"
        )

    drivers = chip_config["drivers"]
    known_drivers = global_chip_config["drivers"]

    chip_source_files = {}

    for driver, driver_settings in drivers.items():
        if not driver in known_drivers:
            raise Exception(
                f"Driver '{driver}' does not exist in global config for this chip"
            )

        if not "extra_flags" in known_drivers[driver]:
            known_drivers[driver]["extra_flags"] = ""

        if not driver_settings is None and driver_settings.get("use_ll", False):
            if known_drivers[driver].get("ll_available", False):
                if not "ll_extra_flags" in known_drivers[driver]:
                    known_drivers[driver]["ll_extra_flags"] = ""
                chip_source_files[known_drivers[driver]["ll_path"]] = known_drivers[
                    driver
                ]["ll_extra_flags"]
            elif (
                not "ll_available" in known_drivers[driver]
                or not known_drivers[driver]["ll_available"]
            ):
                raise Exception(
                    f"Driver '{driver}' has 'use_ll: true', "
                    "but this driver does not have a low level version"
                )
        elif known_drivers[driver].get("ll_only", False):
            if not "ll_extra_flags" in known_drivers[driver]:
                known_drivers[driver]["ll_extra_flags"] = ""
            chip_source_files[known_drivers[driver]["ll_path"]] = known_drivers[driver][
                "ll_extra_flags"
            ]
        else:
            chip_source_files[known_drivers[driver]["path"]] = known_drivers[driver][
                "extra_flags"
            ]
    chip_source_files_full_path = []

    for file in chip_source_files:
        chip_source_files_full_path.append(
            (
                REPO_ROOT_DIR.Dir(hal_src_path).File(file),
                chip_source_files[file],
            )
        )

    if "extraSources" in global_chip_config:
        for file in global_chip_config["extraSources"]:
            chip_source_files_full_path.append(
                (
                    REPO_ROOT_DIR.Dir(base_path).Dir(chip["family"]).File(file),
                    "",
                )
            )

    ret = {"sources": chip_source_files_full_path}

    if "defines" in global_chip_config:
        ret["defines"] = generate_defines(global_chip_config)

    if chip_config.get("linker", False) and chip_config["linker"].get(
        "useDefault", False
    ):
        linker_file = global_chip_config.get("defaultLinkerFile", "")
        if linker_file:
            ret["linker_file"] = (
                REPO_ROOT_DIR.Dir(base_path).Dir(chip["family"]).File(linker_file)
            )

    if chip_family.get("hal", False) and chip_family["hal"].get("include", False):
        ret["hal_includes"] = REPO_ROOT_DIR.Dir(base_path).Dir(chip_family["hal"]["include"])

    if chip_family.get("cmsis", False):
        ret["cmsis_includes"] = REPO_ROOT_DIR.Dir(base_path).Dir(chip_family["cmsis"])
    return ret


def generate(env):
    env.AddMethod(_configure_chip, "ChipConfig")


def exists():
    return True
