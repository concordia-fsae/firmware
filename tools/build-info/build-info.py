from argparse import ArgumentParser
from mako.template import Template
from git import Repo
from glob import glob
import os
from zlib import crc32

def render_build_info(output_dir, templates, build_info):
    for template in templates:
        renderer = Template(filename=template)
        try:
            rendered = renderer.render(**{"build_info": build_info})
        except Exception as e:
            raise Exception(f"Template render error on '{template}': {e}")
        if not isinstance(rendered, str):
            raise Exception("Template render error on '{template}': "+
                            "Mako rendering didn't produce a str to write to the file")

        with open(output_dir + os.sep + template.rstrip(".mako"), "w") as out_fd:
            out_fd.write(rendered)


if __name__ == "__main__":
    parser = ArgumentParser("build-info")

    parser.add_argument(
        "--source-dir",
        required=True,
        help="Template file.",
        metavar="TEMPLATE",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output folder destination.",
        metavar="OUTPUT_FOLDER",
    )

    args = parser.parse_args()

    templates = glob(args.source_dir + os.sep + "*.mako")

    if not len(templates):
        print("No templates defined")
        exit(1)
    elif not os.path.exists(args.output):
        print(f"Output directory '{args.output}' does not exist")
        exit(1)

    repo = Repo(args.source_dir)
    print("Generating build info...")

    build_info = {}
    build_info["repo_sha"] = repo.head.commit.hexsha
    build_info["repo_sha_crc"] = hex(crc32(repo.head.commit.hexsha.encode()))
    build_info["repo_dirty"] = repo.is_dirty()

    try:
        render_build_info(args.output, templates, build_info)
    except Exception as e:
        print(f"Error: {e}")
        exit(1)
