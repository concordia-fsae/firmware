load("//tools/uv/defs.bzl", "uv_genrule")
load("@prelude//:rules.bzl", __rules__ = "rules")

def CSVtoARRAY(name,srcs):
    outs = []
    for src in srcs:
        base = src.replace(".csv", "")
        outs.append(base + ".h")
        outs.append(base + ".c")

    uv_genrule(
        name = name,
        srcs = srcs,
        outs = {"generated": outs,},
        tool = "//tools/csv_import:CSV_ARRAY_tool",
        cmd = "$(python) ${TOOLDIR}/CSV_integration.py --source-dir $(SRCDIR) --outdir $(OUT)",
    )