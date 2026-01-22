import csv
import sys
from pathlib import Path

args = sys.argv[1:]

sep = args.index("--out")

csv_files = [Path(p) for p in args[:sep]]
outs = [Path(p) for p in args[sep + 1:]]
assert len(outs) == len(csv_files) * 2

for i, csv_file in enumerate(csv_files):
    out_h = outs[i * 2]
    out_c = outs[i * 2 + 1]
    print(f"Generating {out_h} / {out_c} from {csv_file}")

    data = []

    # ---------- read CSV ----------
    with open(csv_file, newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            data.append(float(row[1]))
    array_name = csv_file.stem

    # ---------- write header ----------
    with open(out_h, "w") as h:
        h.write("#pragma once\n\n")
        h.write(f"#define {array_name.upper()}_SIZE {len(data)}\n\n")
        h.write(f"extern const float {array_name}[{len(data)}];\n")

    # ---------- write source ----------
    with open(out_c, "w") as c:
        c.write(f'#include "{out_h.name}"\n\n')
        c.write(f"const float {array_name}[{len(data)}] = {{\n")
        for v in data:
            c.write(f"    {v:.6f}f,\n")
        c.write("};\n")