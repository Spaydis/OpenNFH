#!/usr/bin/env python3
"""Read-only audit of identifiers shared by reference binaries and XML data."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import sys
from typing import Iterable
import xml.etree.ElementTree as ET
import zipfile


_XML_DECLARATION = re.compile(r"^\s*<\?xml[^>]*\?>", re.IGNORECASE)


def scan_utf16le_strings(data: bytes, needle: str) -> list[int]:
    encoded = needle.encode("utf-16le")
    offsets: list[int] = []
    cursor = 0
    while True:
        offset = data.find(encoded, cursor)
        if offset < 0:
            return offsets
        offsets.append(offset)
        cursor = offset + 1


def _scan_bytes(data: bytes, needle: str) -> list[int]:
    encoded = needle.encode("ascii")
    offsets: list[int] = []
    cursor = 0
    while True:
        offset = data.find(encoded, cursor)
        if offset < 0:
            return offsets
        offsets.append(offset)
        cursor = offset + 1


def _pe_section_for_offset(data: bytes, offset: int) -> str | None:
    if len(data) < 64 or data[:2] != b"MZ":
        return None
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset : pe_offset + 4] != b"PE\0\0":
        return None
    section_count, optional_size = struct.unpack_from("<HH", data, pe_offset + 6)
    table = pe_offset + 24 + optional_size
    for index in range(section_count):
        entry = table + index * 40
        if entry + 40 > len(data):
            return None
        name = data[entry : entry + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        raw_size, raw_pointer = struct.unpack_from("<II", data, entry + 16)
        if raw_pointer <= offset < raw_pointer + raw_size:
            return name
    return None


def _decode_text(data: bytes) -> str:
    if data.startswith(b"\xff\xfe"):
        return data[2:].decode("utf-16le")
    if data.startswith(b"\xfe\xff"):
        return data[2:].decode("utf-16be")
    try:
        return data.decode("utf-8-sig")
    except UnicodeDecodeError:
        return data.decode("cp1252")


def _iter_xml_contexts(data_root: Path, needles: set[str]) -> tuple[list[str], dict[str, list[dict[str, str]]], list[str]]:
    pack_path = data_root / "gamedata.bnd"
    entries: list[str] = []
    contexts = {needle: [] for needle in sorted(needles)}
    warnings: list[str] = []
    with zipfile.ZipFile(pack_path) as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            entries.append(info.filename)
            try:
                text = _XML_DECLARATION.sub("", _decode_text(archive.read(info)))
                root = ET.fromstring(f"<__audit_root__>{text}</__audit_root__>")
            except (ET.ParseError, UnicodeDecodeError, zipfile.BadZipFile) as error:
                warnings.append(f"{info.filename}: {error}")
                continue
            for element in root.iter():
                for key, value in element.attrib.items():
                    for needle in needles:
                        if needle in value:
                            contexts[needle].append(
                                {
                                    "entry": info.filename,
                                    "element": element.tag,
                                    "attribute": key,
                                    "value": value,
                                }
                            )
    return entries, contexts, warnings


def _binary_report(path: Path, needles: Iterable[str]) -> dict[str, object]:
    data = path.read_bytes()
    matches: dict[str, object] = {}
    for needle in sorted(set(needles)):
        utf16_offsets = scan_utf16le_strings(data, needle)
        ascii_offsets = _scan_bytes(data, needle)
        matches[needle] = {
            "utf16le": utf16_offsets,
            "ascii": ascii_offsets,
            "sections": {
                str(offset): _pe_section_for_offset(data, offset)
                for offset in utf16_offsets + ascii_offsets
            },
        }
    return {"path": str(path), "matches": matches}


def build_report(binary_paths: Iterable[Path], data_root: Path | None, needles: Iterable[str]) -> dict[str, object]:
    needle_set = set(needles)
    report: dict[str, object] = {
        "binary_matches": [_binary_report(path, needle_set) for path in binary_paths],
        "xml_entries": [],
        "xml_contexts": {needle: [] for needle in sorted(needle_set)},
        "warnings": [],
    }
    if data_root is not None:
        try:
            entries, contexts, warnings = _iter_xml_contexts(data_root, needle_set)
        except (OSError, zipfile.BadZipFile) as error:
            report["warnings"] = [str(error)]
        else:
            report["xml_entries"] = entries
            report["xml_contexts"] = contexts
            report["warnings"] = warnings
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", action="append", type=Path, default=[])
    parser.add_argument("--data-root", type=Path)
    parser.add_argument("--needle", action="append", required=True)
    args = parser.parse_args()
    try:
        report = build_report(args.binary, args.data_root, args.needle)
    except OSError as error:
        print(str(error), file=sys.stderr)
        return 2
    json.dump(report, sys.stdout, ensure_ascii=False, indent=2, sort_keys=True)
    sys.stdout.write("\n")
    return 0 if not report["warnings"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
