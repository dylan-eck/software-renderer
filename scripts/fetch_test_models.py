#!/usr/bin/env python3
"""Download the test models into test/models/, skipping any already present."""

import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODEL_DIR = ROOT / "test" / "models"

# pinned to a specific commit so the test models never change underneath us
BASE_URL = (
    "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/"
    "8a4f8642acaf43f9cd7b67858a1502e1055ef202/data/"
)
MODELS = [
    "beast",
    "beetle-alt",
    "bimba",
    "cheburashka",
    "cow",
    "happy",
    "homer",
    "igea",
    "ogre",
    "stanford-bunny",
    "suzanne",
    "teapot",
    "xyzrgb_dragon",
]

TIMEOUT_SECONDS = 30
CHUNK_SIZE = 64 * 1024


def download(url, path, prefix):
    # write to a temporary file first so an interrupted download never leaves
    # a truncated model behind that later runs would treat as complete
    tmp_path = path.with_name(path.name + ".part")
    try:
        with urllib.request.urlopen(url, timeout=TIMEOUT_SECONDS) as response:
            total_size = int(response.headers.get("Content-Length", 0))
            received = 0
            with open(tmp_path, "wb") as f:
                while chunk := response.read(CHUNK_SIZE):
                    f.write(chunk)
                    received += len(chunk)
                    if total_size > 0:
                        percent = min(100, received * 100 // total_size)
                        print(f"\r{prefix} {percent}%", end="", flush=True)
        tmp_path.replace(path)
    finally:
        tmp_path.unlink(missing_ok=True)


def fetch_test_models():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)

    for i, model in enumerate(MODELS, 1):
        path = MODEL_DIR / f"{model}.obj"
        if path.exists():
            continue

        prefix = f"[{i}/{len(MODELS)}] Downloading {path.name}"
        download(BASE_URL + path.name, path, prefix)
        print(f"\r{prefix} done")


if __name__ == "__main__":
    fetch_test_models()
