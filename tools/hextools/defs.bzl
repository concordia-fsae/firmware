load("//tools/uv/defs.bzl", "uv_genrule", "uv_tool")

def inject_crc(
        name: str,
        binary: str,
        output: str,
        start_address: int):
    uv_genrule(
        name = name,
        tool = "//tools/hextools:hextools",
        srcs = [binary],
        cmd = "$(python) ${TOOLDIR}/hextools.py --input ${SRCS} --output ${OUT}" + " --start_address {}".format(start_address),
        out = output,
    )
