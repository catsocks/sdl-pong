#!/usr/bin/env python3
import argparse
import os
import sys
from pathlib import Path
import shutil

parser = argparse.ArgumentParser()
parser.add_argument("--force-url-https", action="store_true")

if __name__ == "__main__":
    args = parser.parse_args()

    url = os.environ.get("URL")
    if url is None:
        sys.exit("Must provide URL environment variable")

    if args.force_url_https:
        if url.startswith("http") and not url.startswith("https"):
            url = url.replace("http", "https", 1)

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
