import csv
import sys
from pathlib import Path

csv_file = Path(sys.argv[1])
out_h = Path(sys.argv[2])
out_c = Path(sys.argv[3])

def read_csv_into_array(filename):
    csv_path = Path(filename)

    data = []

    # ---------- read CSV ----------
    with open(csv_path, newline="", encoding="utf-8") as file:
        reader = csv.reader(file)
        next(reader, None)
        for row in reader:
            data.append(float(row[1]))

    # ---------- output paths ----------
    out_dir = Path("generated")
    out_dir.mkdir(exist_ok=True)
    out_c = out_dir / f"{csv_path.stem}.c"
    out_h = out_dir / f"{csv_path.stem}.h"
    array_name = csv_path.stem

    # ---------- write header ----------
    with open(out_h, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define {array_name.upper()}_SIZE {len(data)}\n\n")
        f.write(f"extern const float {array_name}[{len(data)}];\n")

    # ---------- write source ----------
    with open(out_c, "w") as f:
        f.write(f'#include "{array_name}.h"\n\n')
        f.write(f"const float {array_name}[{len(data)}] = {{\n")
        for v in data:
            f.write(f"    {v:.6f}f,\n")
        f.write("};\n")
