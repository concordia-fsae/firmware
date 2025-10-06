import logging
from argparse import ArgumentParser
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TypedDict

from mako.exceptions import TopLevelLookupException
from mako.lookup import TemplateLookup
from yaml import safe_load

logger = logging.getLogger("configurator")


class ConfigInfo(TypedDict):
    templates: list[str]
    data: dict[str, Any]


@dataclass()
class Args:
    input: Path
    output_dir: Path
    template_dir: Path
    output_log: bool = False
    output_log_name: Path = Path("outs.log")


def parse_yaml_file(file: Path) -> ConfigInfo | None:
    try:
        with file.open("r") as f:
            data = safe_load(f)

        templates = data.pop("templates", None)
        if not templates:
            raise Exception(f"Config file '{file}' is missing key 'templates'")

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
        required=True,
        type=Path,
        help="Path in which to look for templates.",
    )
    parser.add_argument(
        "--output-log",
        action="store_true",
        help="Write a file containing a newline-separated list of successfully generated files.",
    )
    parser.add_argument(
        "--output-log-name",
        type=Path,
        default="outs.log",
        help="Path, relative to --output-dir, at which to write a log file listing each of the files "
        "that were successfully generated. Defaults to `outs.log`.",
    )
    args = Args(**vars(parser.parse_args()))

    args.output_dir.mkdir(parents=True, exist_ok=True)

    # get each of the subdirs of the given template dir that contain at least one mako template
    template_dirs: set[Path] = set()
    for dirpath, _, filenames in args.template_dir.walk(follow_symlinks=True):
        if any(fn.endswith(".mako") for fn in filenames):
            template_dirs.add(dirpath)

    # build a lookup with each directory that contains at least one mako template
    lookup = TemplateLookup(directories=[str(d) for d in template_dirs])

    outputs: set[Path] = set()

    # Process each of the yaml files in the given config directory
    for yaml_file in args.input.glob("**/*.yaml"):
        logger.info("Processing file '%s'", yaml_file)

        # PARSE YAML file
        data = parse_yaml_file(yaml_file)
        if not data:
            continue

        logger.info("Templates to render: %i", len(data["templates"]))

        for template_name in data["templates"]:
            logger.info("Rendering template: %s", template_name)
            try:
                template = lookup.get_template(template_name)
            except TopLevelLookupException:
                raise RuntimeError(
                    f"Failed to find template '{template_name}'"
                ) from None

            output = template.render(**data["data"])

            if not isinstance(output, str):
                raise Exception(
                    "Got bad type from rendering a template. Expected 'str' got '%s'",
                    type(output),
                )

            output_file = args.output_dir / template_name.replace(".mako", "")
            output_file.write_text(output)
            outputs.add(output_file)

            logger.info("Generated: '%s'", output_file)

    if args.output_log:
        args.output_log_name = args.output_dir / args.output_log_name
        if args.output_log_name.exists():
            if args.output_log_name.is_dir():
                args.output_log_name /= "outs.log"
        elif args.output_log_name.parent is args.output_dir:
            args.output_log_name /= "outs.log"

        args.output_log_name.write_text("\n".join(str(o) for o in outputs))


if __name__ == "__main__":
    main()
