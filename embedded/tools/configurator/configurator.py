import logging
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

        template_paths = []
        for template in templates:
            p = Path(template)
            try:
                p.relative_to(SHARED_TEMPLATES_DIR, walk_up=False)
            except ValueError:
                pass
            else:
                pass

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
        # action="append",
        type=Path,
        help="Path in which to look for templates. Can be specified multiple times.",
    )
    args = Args()
    parser.parse_args(namespace=Args)

    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Process each yaml file
    for yaml_file in args.input.glob("**/*.yaml"):
        logger.info("Processing file '%s'", yaml_file)

        # PARSE YAML file
        data = parse_yaml_file(yaml_file)
        if not data:
            continue

        logger.info("Templates: %i", len(data["templates"]))

        # render each template
        lookup = TemplateLookup(directories=[str(args.template_dir.absolute()) + "/templates"])

        for template_info in data["templates"]:
            logger.info("Rendering template: %s", template_info)
            try:
                template = lookup.get_template(template_info.name)
            except TopLevelLookupException:
                raise RuntimeError(f"Failed to find template '{template_info.name}'") from None

            output = template.render(**data["data"])

            if not isinstance(output, str):
                raise Exception(
                    "Got bad type from rendering a template. Expected 'str' got '%s'",
                    type(output),
                )

            output_file = args.output_dir / template_info.with_suffix("").name
            output_file.write_text(output)

            logger.info("Generated: '%s'", output_file)

    raise Exception


if __name__ == "__main__":
    main()
