#!/usr/bin/env python3
import argparse
import os
import sys
from pathlib import Path
import shutil

parser = argparse.ArgumentParser()
parser.add_argument(
    "-g",
    "--game-build-path",
    default="build",
    help="path to a build of the game",
)
parser.add_argument(
    "-b",
    "--build-path",
    default="build-website",
    help="path to where the website should be built",
)
parser.add_argument("--force-url-https", action="store_true")
parser.add_argument(
    "--clear-first", action="store_true", help="clear the build path before building"
)

if __name__ == "__main__":
    args = parser.parse_args()

    url = os.environ.get("URL")
    if url is None:
        sys.exit("Must provide URL environment variable")

    if args.force_url_https:
        if url.startswith("http") and not url.startswith("https"):
            url = url.replace("http", "https", 1)

    game_path = Path(args.game_build_path)
    build_path = Path(args.build_path)

    if args.clear_first:
        try:
            shutil.rmtree(build_path)
        except FileNotFoundError:
            pass

    os.makedirs(build_path, exist_ok=True)

    shutil.copytree(
        "assets", build_path, copy_function=shutil.copyfile, dirs_exist_ok=True
    )

    for name in ("game.html", "game.js", "game.wasm"):
        shutil.copyfile(game_path / name, build_path / name)

    for file in ("index.html", "game.html"):
        path = build_path / file
        text = path.read_text().replace(r"{{url}}", url)
        path.write_text(text)
