#!/usr/bin/env python3
"""Read-only section-aware string/xref report for user-supplied PE files."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import struct
import sys
from typing import Iterable, Mapping


ASCII_STRING = re.compile(rb"[\x20-\x7e]{4,}")
UTF16LE_STRING = re.compile(rb"(?:[\x20-\x7e]\x00){4,}")
IMAGE_SCN_MEM_EXECUTE = 0x20000000


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_pointer: int
    raw_size: int
    characteristics: int

    def contains_raw_offset(self, offset: int) -> bool:
        return self.raw_pointer <= offset < self.raw_pointer + self.raw_size


def parse_pe(data: bytes) -> tuple[int, list[Section]]:
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("missing MZ signature")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("missing PE signature")
    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional = pe_offset + 24
    if optional + optional_size > len(data) or optional_size < 32:
        raise ValueError("truncated PE optional header")
    magic = struct.unpack_from("<H", data, optional)[0]
    if magic == 0x10B:
        image_base = struct.unpack_from("<I", data, optional + 28)[0]
    elif magic == 0x20B:
        image_base = struct.unpack_from("<Q", data, optional + 24)[0]
    else:
        raise ValueError("unsupported PE optional header")

    table = optional + optional_size
    sections: list[Section] = []
    for index in range(section_count):
        entry = table + index * 40
        if entry + 40 > len(data):
            raise ValueError("truncated PE section table")
        name = data[entry:entry + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        sections.append(Section(
            name=name,
            virtual_size=struct.unpack_from("<I", data, entry + 8)[0],
            virtual_address=struct.unpack_from("<I", data, entry + 12)[0],
            raw_size=struct.unpack_from("<I", data, entry + 16)[0],
            raw_pointer=struct.unpack_from("<I", data, entry + 20)[0],
            characteristics=struct.unpack_from("<I", data, entry + 36)[0],
        ))
    return image_base, sections


def section_for_offset(sections: Iterable[Section], offset: int) -> Section | None:
    return next((section for section in sections if section.contains_raw_offset(offset)), None)


def virtual_address(image_base: int, section: Section | None, offset: int) -> int | None:
    if section is None:
        return None
    return image_base + section.virtual_address + offset - section.raw_pointer


def _matches(text: str, needles: tuple[str, ...]) -> bool:
    lowered = text.casefold()
    return not needles or any(needle.casefold() in lowered for needle in needles)


def _find_xrefs(data: bytes, sections: list[Section], address: int) -> list[int]:
    encoded = struct.pack("<I", address & 0xFFFFFFFF)
    offsets: list[int] = []
    for section in sections:
        if not section.characteristics & IMAGE_SCN_MEM_EXECUTE:
            continue
        start = section.raw_pointer
        end = min(len(data), start + section.raw_size)
        cursor = start
        while cursor < end:
            found = data.find(encoded, cursor, end)
            if found < 0:
                break
            offsets.append(found)
            cursor = found + 1
    return offsets


def _iter_strings(data: bytes):
    for match in ASCII_STRING.finditer(data):
        yield match.start(), "ascii", match.group().decode("ascii")
    for match in UTF16LE_STRING.finditer(data):
        yield match.start(), "utf16le", match.group().decode("utf-16le")


def build_report(binary_data: Mapping[str, bytes], needles: Iterable[str]) -> dict[str, object]:
    needle_tuple = tuple(needles)
    binaries: list[dict[str, object]] = []
    for name, data in binary_data.items():
        image_base, sections = parse_pe(data)
        strings: list[dict[str, object]] = []
        for offset, encoding, text in _iter_strings(data):
            if not _matches(text, needle_tuple):
                continue
            section = section_for_offset(sections, offset)
            address = virtual_address(image_base, section, offset)
            strings.append({
                "text": text,
                "encoding": encoding,
                "file_offset": offset,
                "section": section.name if section else None,
                "rva": address - image_base if address is not None else None,
                "virtual_address": address,
                "xrefs": _find_xrefs(data, sections, address) if address is not None else [],
            })
        strings.sort(key=lambda value: (value["file_offset"], value["encoding"]))
        binaries.append({
            "path": name,
            "image_base": image_base,
            "sections": [section.__dict__ for section in sections],
            "strings": strings,
        })
    return {"binaries": binaries}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", action="append", type=Path, required=True)
    parser.add_argument("--needle", action="append", default=[])
    args = parser.parse_args()
    try:
        report = build_report(
            {str(path): path.read_bytes() for path in args.binary}, args.needle)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2
    json.dump(report, sys.stdout, ensure_ascii=False, indent=2, sort_keys=True)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
