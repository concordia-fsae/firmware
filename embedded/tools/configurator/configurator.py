import logging
import os
from argparse import ArgumentParser
from pathlib import Path
from types import SimpleNamespace
from typing import Any, TypedDict

from mako.exceptions import TopLevelLookupException
from mako.lookup import TemplateLookup
from yaml import safe_load

logger = logging.getLogger("configurator")

SHARED_TEMPLATES_DIR = Path("embedded/tools/configurator/templates")


class ConfigInfo(TypedDict):
    templates: list[Path]
    data: dict[str, Any]


class Args(SimpleNamespace):
    input: Path
    output_dir: Path
    template_dir: Path


def parse_yaml_file(file: Path) -> ConfigInfo | None:
    try:
        with file.open("r") as f:
            data = safe_load(f)

        templates = data.pop("templates", None)
        if not templates:
            raise Exception(f"Config file '{file}' is missing key 'templates'")

        templates = [Path(t) for t in templates]

        return {
            "templates": templates,
            "data": data,
        }
    except Exception as e:
        logger.error(f"Failed to parse file '{file}': {e}")
        return None


def main():
    """Main function"""
    parser = ArgumentParser()
    parser.add_argument(
        "--input", 
        required=True, 
        type=Path, 
        help="Path to directory containing yaml configuration files. Nesting is allowed in this directory.",
    )
    parser.add_argument(
        "--output-dir", 
        required=True, 
        type=Path,
        help="Path in which to write rendered files.",
    )
    parser.add_argument(
        "--template-dir", 
        "-t", 
        type=Path,
        help="Path in which to look for templates, Can be specified multiple times.",
    )
    args = Args()
    parser.parse_args(namespace=args)

    args.output_dir.mkdir(parents=True, exist_ok=True)

    if args.input.is_file():
        yaml_files = [args.input]
    else:
        yaml_files = list(args.input.glob("**/*.yaml"))

    base = args.template_dir.absolute()

    candidate_dirs = [
        base,
        base / "templates",
        base / "srcs",
        base / "srcs" / "templates",
        base / "embedded" / "tools" / "configurator" / "templates",
        base / "srcs" / "embedded" / "tools" / "configurator" / "templates",
    ]

    existing = [str(p) for p in candidate_dirs if p.exists()]

    if not existing:
        raise RuntimeError(f"Configurator: none of the template dirs exist under base={base}. Checked: {candidate_dirs}")

    for d in existing:
        try:
            logger.info("Template dir candidate: %s", d)
            print(f"[configurator] template dir: {d}")
            print(f"[configurator] contents of {d}: {os.listdir(d)}")
        except Exception:
            pass

    lookup = TemplateLookup(directories=existing)

    for yaml_file in yaml_files:
        logger.info("Processing file '%s'", yaml_file)
        print(f"[configurator] processing yaml: {yaml_file}")

        data = parse_yaml_file(yaml_file)
        if not data:
            continue

        for template_info in data["templates"]:
            name = template_info.name
            logger.info("Rendering template: %s", name)
            try:
                template = lookup.get_template(name)
            except TopLevelLookupException:
                raise RuntimeError(f"Failed to find template '{name}' in {existing}") from None

            output = template.render(**data["data"])

            if not isinstance(output, str):
                raise Exception("Template render did not return str")

            out_path = args.output_dir / template_info.with_suffix("").name
            out_path.write_text(output)

            logger.info("Generated: '%s'", out_path)


if __name__ == "__main__":
    main()
