load("@prelude//:rules.bzl", __rules__ = "rules")

DeployableAsset = provider(
    # @unsorted-dict-items
    fields = {
        "node": provider_field(str),
        "binary": provider_field(str),
    },
)

def deployable_target(
        node: str,
        binary: str):
    return DeployableAsset(
        node = node,
        binary = binary,
    )

def conUDS_batch(
        name: str,
        binaries: list[DeployableAsset],
        manifest: str):

    args = []
    for binary in binaries:
        print(binary)
        args.append("-u")
        args.append("{}:$(location {})".format(binary.node, binary.binary))

    return __rules__["command_alias"](
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "batch", "{}".format(*args)],
    )

def conUDS_download(
        name: str,
        binary: str,
        node: str,
        manifest: str):
    return __rules__["command_alias"](
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "-n", node, "download", "$(location {})".format(binary)],
    )

def conUDS_bootloader_download(
        name: str,
        binary: str,
        node: str,
        manifest: str):
    return __rules__["command_alias"](
        name = name,
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-m", "$(location {})".format(manifest), "-n", node, "bootloader-download", "$(location {})".format(binary)],
    )
