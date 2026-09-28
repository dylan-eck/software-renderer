import subprocess
from pathlib import Path
import os
import urllib.request

base_url = "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/8a4f8642acaf43f9cd7b67858a1502e1055ef202/data/"
models = [
    "beast", "beetle-alt", "bimba",
    "cheburashka", "cow", "happy", "homer", "igea",
    "ogre",
    "stanford-bunny", "suzanne", "teapot", "xyzrgb_dragon",
]

model_dir_path = Path('./test/models')
out_dir_path = Path("./test/images")

os.makedirs(model_dir_path, exist_ok=True)
os.makedirs(out_dir_path, exist_ok=True)

for i, model in enumerate(models, 1):
    file = model + ".obj"
    url = base_url + file
    path = model_dir_path / file

    if os.path.exists(path):
        continue

    prefix = f"[{i}/{len(models)}] Downloading {file}"

    def show_progress(block_count, block_size, total_size):
        if total_size > 0:
            percent = min(100, block_count * block_size * 100 // total_size)
            print(f"\r{prefix} {percent}%", end="", flush=True)

    urllib.request.urlretrieve(url, path, show_progress)
    print(f"\r{prefix} done")

test_files = list(model_dir_path.glob('*.obj'))

for file in test_files:
    subprocess.run(["./a.out", file, "7680", "4320"])
    os.rename("out.bmp", out_dir_path / f'{file.stem}_8k.bmp')

#     # w = 512
#     # for i in range(5):
#     #     subprocess.run(["./a.out", file, str(w)])
#     #     os.rename("out.bmp", f'test/out/{file.stem}_{w}.bmp')

#     #     w *= 2
