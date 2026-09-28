#!/usr/bin/env python3
"""Convert an .obj file into a C string literal for embedding in renderer.c.

Comments and object names are dropped and vertex data is rounded to two
significant figures to keep the embedded string small.
"""

import argparse
from pathlib import Path

DEFAULT_OBJ = Path(__file__).resolve().parent.parent / "assets" / "teapot_min.obj"
VERTEX_PREFIXES = ("v", "vn", "vt")
DROPPED_PREFIXES = ("#", "o")


def minify_line(line):
    parts = line.split()
    if not parts or parts[0] in DROPPED_PREFIXES:
        return None

    if parts[0] in VERTEX_PREFIXES:
        try:
            values = [f"{float(x):.2g}" for x in parts[1:]]
        except ValueError:
            return line  # keep lines we can't parse unchanged
        return " ".join([parts[0], *values])

    return line


def obj_to_c_string(text):
    lines = (minify_line(line.strip()) for line in text.splitlines())
    content = "".join(line + "\n" for line in lines if line is not None)
    escaped = content.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{escaped}"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "obj_file",
        nargs="?",
        default=DEFAULT_OBJ,
        help="path to the .obj file to embed (default: assets/teapot_min.obj)",
    )
    args = parser.parse_args()

    with open(args.obj_file) as f:
        print(obj_to_c_string(f.read()))


if __name__ == "__main__":
    main()
