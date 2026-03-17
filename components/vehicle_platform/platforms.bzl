PLATFORM = enum(
    "cfr25",
    "cfr26",
)

PLATFORMS = struct(
    CFR25 = PLATFORM.cfr25,
    CFR26 = PLATFORM.cfr26,
)

ALL_PLATFORMS = [
    PLATFORMS.CFR25,
    PLATFORMS.CFR26,
]

def _platform_str(platform):
    if platform == PLATFORMS.CFR25:
        return "cfr25"
    if platform == PLATFORMS.CFR26:
        return "cfr26"
    fail("Unknown platform: {}".format(platform))

def platform_constraint_name(platform):
    return _platform_str(platform)

def platform_constraint_label(platform):
    return "root//buck2/constraints:{}".format(platform_constraint_name(platform))

def platform_output_name(platform):
    return _platform_str(platform)

def platform_target_name(platform):
    return "platform-{}".format(platform_constraint_name(platform))

def platform_target_label(platform):
    return "root//buck2/platforms:{}".format(platform_target_name(platform))
