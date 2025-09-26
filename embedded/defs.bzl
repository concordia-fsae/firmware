load("@prelude//:rules.bzl", __rules__ = "rules")
load("@prelude//utils:utils.bzl", "flatten")

def preprocess_linkscript(
        name: str,
        toolchain: str,
        linkscript: str,
        srcs: dict[str, str],
        compiler_flags: list[str],
        force_includes: list[str],
        out: str):
    flags = compiler_flags + ["-include {}".format(x) for x in force_includes]
    flags = " ".join(flags)

    __rules__["cxx_genrule"](
        name = name,
        _cxx_toolchain = toolchain,
        cmd = "set -o pipefail && $(cpp) -P -undef " + flags + " linkscript.ld | sed -e '/^#.\\+$/d' > ${OUT}",
        out = out,
        srcs = {
            "linkscript.ld": linkscript,
        } | srcs,
    )

def produce_bin(
        name: str,
        toolchain: str,
        src: str,
        out: str):
    __rules__["cxx_genrule"](
        name = name,
        _cxx_toolchain = toolchain,
        srcs = [src],
        out = out,
        cmd = "$(objcopy) -O binary ${SRCS} ${OUT}",
    )
