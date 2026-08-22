from __future__ import annotations

import ctypes
import importlib.util
import os
import pathlib
import subprocess
import sys
import types
from collections.abc import MutableMapping


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[3]


def buck_output(target: str, root: pathlib.Path | None = None) -> pathlib.Path:
    root = root or repo_root()
    result = subprocess.run(
        ["buckle", "build", "--show-output", target],
        cwd=root,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    for line in result.stdout.splitlines():
        if line.startswith(f"root{target} "):
            return root / line.split(maxsplit=1)[1]
    raise RuntimeError(f"buckle did not report an output for {target}")


def shared_library_mode() -> int:
    # Controller models intentionally share generic rig_model_* symbol names.
    # Keep each shared object local so different controllers can coexist in one
    # Python process without dynamic-loader symbol interposition.
    mode = getattr(os, "RTLD_LOCAL", ctypes.DEFAULT_MODE)
    mode |= getattr(os, "RTLD_NOW", 0)
    mode |= getattr(os, "RTLD_DEEPBIND", 0)
    return mode


def load_shared_library(path: pathlib.Path) -> ctypes.CDLL:
    return ctypes.CDLL(str(path), mode=shared_library_mode())


def load_generated_module(
    env_var: str, target: str, module_name: str
) -> types.ModuleType:
    module_path = os.environ.get(env_var)
    if module_path is None:
        raise RuntimeError(
            f"{env_var} is not configured; build {target} with Buck and pass its output explicitly"
        )
    path = pathlib.Path(module_path)
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"failed to load generated module {module_name} from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def load_generated_enums(
    env_var: str,
    target: str,
    module_name: str,
    namespace: MutableMapping[str, object],
) -> types.ModuleType:
    cached = namespace.get("_CAN_ENUMS")
    if isinstance(cached, types.ModuleType):
        return cached
    module = load_generated_module(env_var, target, module_name)
    for name, value in vars(module).items():
        if isinstance(value, type) and hasattr(value, "__members__"):
            namespace[name] = value
    namespace["_CAN_ENUMS"] = module
    return module
