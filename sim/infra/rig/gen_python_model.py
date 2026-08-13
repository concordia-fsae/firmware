from __future__ import annotations

import argparse
import re
from pathlib import Path


RUST_EXTERN_RE = re.compile(
    r"pub\s+extern\s+\"C\"\s+fn\s+(?P<name>[A-Za-z0-9_]+)\s*"
    r"\((?P<args>[^)]*)\)\s*(?:->\s*(?P<ret>[A-Za-z0-9_]+))?"
)

RUST_TO_CTYPES = {
    "bool": "ctypes.c_bool",
    "f32": "ctypes.c_float",
    "i32": "ctypes.c_int",
    "u32": "ctypes.c_uint32",
}

BASE_MODEL_SUFFIXES = {
    "new",
    "run_for",
}


def property_name(symbol_suffix: str) -> str:
    if symbol_suffix.startswith("get_"):
        return symbol_suffix.removeprefix("get_")
    return symbol_suffix


def parse_model_getters(rust_source: Path, symbol_prefix: str) -> list[tuple[str, str]]:
    getters = []
    for match in RUST_EXTERN_RE.finditer(rust_source.read_text()):
        symbol = match.group("name")
        if not symbol.startswith(symbol_prefix + "_"):
            continue
        suffix = symbol.removeprefix(symbol_prefix + "_")
        ret = match.group("ret")
        if suffix in BASE_MODEL_SUFFIXES or not suffix.startswith("get_"):
            continue
        if match.group("args").strip():
            continue
        if ret not in RUST_TO_CTYPES:
            continue
        getters.append((suffix, ret))
    return sorted(getters)


def emit_model(
    *,
    class_name: str,
    symbol_prefix: str,
    buck_target: str,
    env_var: str,
    enum_env_var: str,
    enum_target: str,
    getters: list[tuple[str, str]],
) -> str:
    lines = [
        "from __future__ import annotations",
        "",
        "from sim.infra.rig import NodeRig, load_generated_module",
        "",
        "",
        "_enums = load_generated_module(",
        f'    "{enum_env_var}",',
        f'    "{enum_target}",',
        f'    "{symbol_prefix}_generated_enums",',
        ")",
        "for _enum_name in (",
        '    "AnalogInput",',
        '    "DigitalInput",',
        '    "DigitalIo",',
        '    "DigitalOutput",',
        '    "Fault",',
        '    "TimerChannel",',
        '    "TimerPort",',
        "):",
        "    if hasattr(_enums, _enum_name):",
        "        globals()[_enum_name] = getattr(_enums, _enum_name)",
        "",
        "",
        f"class {class_name}(NodeRig):",
        f'    buck_target = "{buck_target}"',
        f'    env_var = "{env_var}"',
        f'    symbol_prefix = "{symbol_prefix}"',
        "",
    ]

    if getters:
        lines.insert(2, "import ctypes")
        lines.insert(3, "")

    for suffix, _ret in getters:
        prop = property_name(suffix)
        lines += [
            "    @property",
            f"    def {prop}(self):",
            f"        return self._{suffix}()",
            "",
        ]

    lines += [
        "    def _configure_abi(self) -> None:",
        "        super()._configure_abi()",
    ]
    if getters:
        for suffix, ret in getters:
            lines += [
                f"        self._{suffix} = self._bind_model_symbol(",
                f'            "{suffix}",',
                f"            restype={RUST_TO_CTYPES[ret]},",
                "        )",
            ]
    else:
        lines += ["        pass"]

    lines += [
        "",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rust-source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--class-name", required=True)
    parser.add_argument("--symbol-prefix", required=True)
    parser.add_argument("--buck-target", required=True)
    parser.add_argument("--env-var", required=True)
    parser.add_argument("--enum-env-var", required=True)
    parser.add_argument("--enum-target", required=True)
    args = parser.parse_args()

    args.out.write_text(
        emit_model(
            class_name=args.class_name,
            symbol_prefix=args.symbol_prefix,
            buck_target=args.buck_target,
            env_var=args.env_var,
            enum_env_var=args.enum_env_var,
            enum_target=args.enum_target,
            getters=parse_model_getters(args.rust_source, args.symbol_prefix),
        )
    )


if __name__ == "__main__":
    main()
