#!/usr/bin/env python3
"""Build the renderer and render every test model into test/images/."""

import argparse
import subprocess

from build import ROOT, build
from fetch_test_models import MODEL_DIR, MODELS, fetch_test_models

IMAGE_DIR = ROOT / "test" / "images"
OUTPUT_FILE = ROOT / "out.bmp"


def render_test_models(width, height):
    binary = build()
    fetch_test_models()

    IMAGE_DIR.mkdir(parents=True, exist_ok=True)

    for model in MODELS:
        model_path = MODEL_DIR / f"{model}.obj"
        cmd = [str(binary), str(model_path), str(width), str(height)]
        subprocess.run(cmd, cwd=ROOT, check=True)
        OUTPUT_FILE.replace(IMAGE_DIR / f"{model}_{width}x{height}.bmp")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    # same defaults as the renderer itself
    parser.add_argument("width", nargs="?", type=int, default=800)
    parser.add_argument("height", nargs="?", type=int, help="defaults to width")
    args = parser.parse_args()

    render_test_models(args.width, args.height or args.width)


if __name__ == "__main__":
    main()
