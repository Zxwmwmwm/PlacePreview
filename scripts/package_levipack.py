#!/usr/bin/env python3
"""Packages libBuildGhost.so + manifest.json into a verified BuildGhost.levipack archive."""

from __future__ import annotations

import json
import re
import sys
import zipfile
from pathlib import Path


def parse_version_header(header_path: Path) -> dict[str, str]:
    text = header_path.read_text(encoding="utf-8")
    fields: dict[str, str] = {}
    for match in re.finditer(
        r'inline\s+constexpr\s+std::string_view\s+(\w+)\s*=\s*"([^"]+)"\s*;',
        text,
    ):
        fields[match.group(1)] = match.group(2)
    return fields


def build_levipack(repo_root: Path, so_path: Path, out_pack_path: Path) -> None:
    version_hdr = parse_version_header(repo_root / "include" / "buildghost" / "Version.hpp")
    manifest_path = repo_root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    if manifest.get("name") != "BuildGhost":
        raise SystemExit(f"Invalid manifest name: {manifest.get('name')}")
    if manifest.get("entry") != "libBuildGhost.so":
        raise SystemExit(f"Invalid manifest entry: {manifest.get('entry')}")
    if manifest.get("version") != version_hdr.get("Version"):
        raise SystemExit(
            f"Version mismatch: manifest={manifest.get('version')} vs Version.hpp={version_hdr.get('Version')}"
        )
    if manifest.get("author") != "Zxwmwmwm":
        raise SystemExit(f"Invalid manifest author: {manifest.get('author')}")
    if not so_path.is_file() or so_path.stat().st_size == 0:
        raise SystemExit(f"Missing or empty shared library: {so_path}")

    out_pack_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out_pack_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        zf.write(manifest_path, arcname="manifest.json")
        zf.write(so_path, arcname="libBuildGhost.so")

    with zipfile.ZipFile(out_pack_path, "r") as zf:
        bad = zf.testzip()
        if bad is not None:
            raise SystemExit(f"Corrupt zip entry detected: {bad}")
        entries = set(zf.namelist())
        expected = {"manifest.json", "libBuildGhost.so"}
        if entries != expected:
            raise SystemExit(f"Unexpected levipack contents: {entries}")


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        print("Usage: package_levipack.py <repo_root> <libBuildGhost.so> <out.levipack>", file=sys.stderr)
        return 2
    repo_root = Path(argv[1]).resolve()
    so_path = Path(argv[2]).resolve()
    out_pack = Path(argv[3]).resolve()
    build_levipack(repo_root, so_path, out_pack)
    print(f"Packaged {out_pack} ({out_pack.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
