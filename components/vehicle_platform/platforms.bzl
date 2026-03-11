PLATFORMS = struct(
    CFR25 = "cfr25",
    CFR26 = "cfr26",
)

ALL_PLATFORMS = [
    PLATFORMS.CFR25,
    PLATFORMS.CFR26,
]

def platform_constraint_name(platform):
    return platform

def platform_constraint_label(platform):
    return "root//buck2/constraints:{}".format(platform_constraint_name(platform))

def platform_output_name(platform):
    return platform

def platform_target_name(platform):
    return "platform-{}".format(platform_constraint_name(platform))

def platform_target_label(platform):
    return "root//buck2/platforms:{}".format(platform_target_name(platform))
