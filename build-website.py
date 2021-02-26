#!/usr/bin/env python3
import os
import sys
from pathlib import Path
import shutil

if __name__ == "__main__":
    url = os.environ.get("URL")
    if url is None:
        sys.exit("Must provide URL environment variable")

    build = Path("build/wasm/release")
    dest = Path("build/website")

    os.makedirs(dest, exist_ok=True)

    for name in ("index.html", "game.html", "game.js", "game.wasm"):
        shutil.copyfile(build / name, dest / name)

    for name in ("screenshot.png", "screenshot-2-by-1.png"):
        shutil.copyfile(name, dest / name)

    for file in ("index.html", "game.html"):
        path = dest / file
        text = path.read_text().replace(r"{{url}}", url)
        path.write_text(text)
