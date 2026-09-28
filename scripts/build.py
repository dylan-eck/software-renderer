#!/usr/bin/env python3
"""Compile renderer.c into bin/, skipping the build if the binary is up to date."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "renderer.c"
BINARY = ROOT / "bin" / ("renderer.exe" if sys.platform == "win32" else "renderer")


def find_compiler():
    return (
        os.environ.get("CC")
        or shutil.which("cc")
        or shutil.which("gcc")
        or shutil.which("clang")
    )


def build():
    if BINARY.exists() and BINARY.stat().st_mtime > SOURCE.stat().st_mtime:
        return BINARY

    BINARY.parent.mkdir(exist_ok=True)

    compiler = find_compiler()
    if compiler:
        cmd = [compiler, "-std=c99", "-O2", str(SOURCE), "-o", str(BINARY)]
        if sys.platform != "win32":
            cmd.append("-lm")
    elif shutil.which("cl"):
        # /Fo keeps cl's intermediate .obj file in bin/ too
        cmd = ["cl", "/O2", str(SOURCE), f"/Fe:{BINARY}", f"/Fo:{BINARY.parent}\\"]
    else:
        sys.exit("No C compiler found (set CC or install clang/gcc/cl)")

    print("Building:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    return BINARY


if __name__ == "__main__":
    build()
