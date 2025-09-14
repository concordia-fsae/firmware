load("@prelude//:rules.bzl", __rules__ = "rules")

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
