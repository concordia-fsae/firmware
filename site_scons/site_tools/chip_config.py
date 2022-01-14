from yaml import load, Loader

from SCons.Script import *


def _configure_chip(env, config_file):
    REPO_ROOT_DIR = env["REPO_ROOT_DIR"]
    KNOWN_CHIPS_FILE = REPO_ROOT_DIR.File("site_scons/chips.yaml")

    with open(str(KNOWN_CHIPS_FILE), "r") as known_chips_file_handle:
        known_chips = load(known_chips_file_handle, Loader)

    with open(str(File(config_file)), "r") as chip_file_handle:
        chip_config = load(chip_file_handle, Loader)

    chip_name = chip_config["mcu"].lower() if "mcu" in chip_config else None
    if chip_name is None:
        return 1

    global_chip_config = {}

    for chip in known_chips:
        if chip_name.lower().startswith(chip.lower()):
            with open(
                str(REPO_ROOT_DIR.File(known_chips[chip]["path"] + chip_name + ".yaml"))
            ) as chip_config_file:
                global_chip_config = load(chip_config_file, Loader)

    required_drivers = [
        driver
        for driver in global_chip_config["drivers"]
        if "required" in global_chip_config["drivers"][driver]
        and global_chip_config["drivers"][driver]["required"]
    ]

    missing_required = False
    for driver in required_drivers:
        if driver not in chip_config["drivers"]:
            print(
                f"Driver '{driver}' is required, but is not currently enabled in chip config"
            )
            missing_required = True

    if missing_required:
        return 1

    drivers = chip_config["drivers"]
    known_drivers = global_chip_config["drivers"]

    chip_source_files = {}

    for driver, driver_settings in drivers.items():
        if not driver in known_drivers:
            print(f"Driver '{driver}' does not exist in global config for this chip")
            return 1

        if not "extra_flags" in known_drivers[driver]:
            known_drivers[driver]["extra_flags"] = ""

        if (
            not driver_settings is None
            and "use_ll" in driver_settings
            and driver_settings["use_ll"]
        ):
            if (
                "ll_available" in known_drivers[driver]
                and known_drivers[driver]["ll_available"]
            ):
                if not "ll_extra_flags" in known_drivers[driver]:
                    known_drivers[driver]["ll_extra_flags"] = ""
                chip_source_files[known_drivers[driver]["ll_path"]] = known_drivers[
                    driver
                ]["ll_extra_flags"]
            elif (
                not "ll_available" in known_drivers[driver]
                or not known_drivers[driver]["ll_available"]
            ):
                print(
                    f"Driver '{driver}' has 'use_ll: true', but this driver does not support ll"
                )
                return 1
        elif "ll_only" in known_drivers[driver] and known_drivers[driver]["ll_only"]:
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
        chip_source_files_full_path.append((REPO_ROOT_DIR.File(global_chip_config["basePath"] + file), chip_source_files[file]))

    return chip_source_files_full_path


def generate(env):
    env.AddMethod(_configure_chip, "ChipConfig")


def exists():
    return True
