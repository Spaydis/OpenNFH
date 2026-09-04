#!/usr/bin/env python3
"""Reject original game/media files from source or release staging trees."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


PROHIBITED_SUFFIXES = {
    ".7z", ".asi", ".bnd", ".dll", ".exe", ".fot", ".mp3", ".mov",
    ".pdn", ".png", ".psd", ".rar", ".tga", ".ttf", ".wav", ".zip",
}
SKIP_DIRECTORY_NAMES = {".git", "build", "out", "vcpkg_installed"}


def iter_files(root: Path):
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if relative.parts and relative.parts[0].casefold() in SKIP_DIRECTORY_NAMES:
            continue
        if path.is_file():
            yield path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()

    violations = [
        path for path in iter_files(args.root)
        if path.suffix.casefold() in PROHIBITED_SUFFIXES
    ]
    for path in sorted(violations):
        print(f"prohibited source/release file: {path}", file=sys.stderr)
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
