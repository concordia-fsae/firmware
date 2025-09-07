load("@prelude//:rules.bzl", __rules__ = "rules")

def conUDS_download(
        name: str,
        binary: str,
        node: str):
    return __rules__["command_alias"](
        name = "download",
        exe = "//drive-stack/conUDS:conUDS",
        args = ["-n", node, "download", "$(location {})".format(binary)],
    )
