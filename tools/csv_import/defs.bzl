load("//tools/uv/defs.bzl", "uv_genrule")

def CSVtoARRAY(name, srcs):
    outs = []

    for src in srcs:
        base = src.replace(".csv", "")
        outs.append(base + ".h")
        outs.append(base + ".c")

    uv_genrule(
        name = name,
        srcs = srcs,
        tool = "//tools/csv_import:CSV_ARRAY_tool",
        cmd = (
            "$(location //tools/csv_import:CSV_ARRAY_tool) "
            + "$(SRCS) $(OUTS)"
        ),
    )
