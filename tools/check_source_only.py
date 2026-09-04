#!/usr/bin/env python3
"""Reject original game/media files from source or release staging trees."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


PROHIBITED_SUFFIXES = {
    ".asi", ".bnd", ".dll", ".exe", ".fot", ".mp3", ".mov",
    ".pdn", ".png", ".psd", ".tga", ".ttf", ".wav",
}
SKIP_DIRECTORY_NAMES = {".git", "build", "out", "vcpkg_installed"}


def iter_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP_DIRECTORY_NAMES for part in path.relative_to(root).parts):
            continue
        yield path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()

    violations = [
        path for path in iter_files(args.root)
        if path.suffix.lower() in PROHIBITED_SUFFIXES
    ]
    for path in sorted(violations):
        print(f"prohibited source/release file: {path}", file=sys.stderr)
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
