#!/usr/bin/env python3
"""Check packaged PE architecture and exact Asteria icon resources (stdlib only)."""
import argparse
from pathlib import Path
import struct
import zipfile


def ico_frames(data):
    reserved, kind, count = struct.unpack_from("<HHH", data)
    assert (reserved, kind) == (0, 1), "Expected ICO header"
    frames = {}
    for i in range(count):
        w, h, _, _, _, _, length, offset = struct.unpack_from("<BBBBHHII", data, 6 + 16 * i)
        frames[(w or 256, h or 256)] = data[offset:offset + length]
    return frames


def verify_pe(data, expected, architecture):
    assert data[:2] == b"MZ", "Expected PE executable"
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0", "Invalid PE signature"
    machine, section_count = struct.unpack_from("<HH", data, pe + 4)
    assert machine == {"x64": 0x8664, "arm64": 0xAA64}[architecture], "Wrong PE architecture"
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    assert struct.unpack_from("<H", data, optional)[0] == 0x20B, "Expected PE32+"
    resource_rva = struct.unpack_from("<I", data, optional + 112 + 8 * 2)[0]
    assert resource_rva, "Missing PE resources"
    sections = []
    for i in range(section_count):
        offset = optional + optional_size + i * 40
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, offset + 8)
        sections.append((rva, max(virtual_size, raw_size), raw_offset))

    def file_offset(rva):
        for start, size, offset in sections:
            if start <= rva < start + size:
                return offset + rva - start
        raise ValueError(f"Unmapped RVA: {rva:#x}")

    base = file_offset(resource_rva)
    resources = {}

    def walk(relative, path=()):
        assert len(path) <= 3, "Unexpected resource directory depth"
        named, ids = struct.unpack_from("<HH", data, base + relative + 12)
        for i in range(named + ids):
            key, target = struct.unpack_from("<II", data, base + relative + 16 + i * 8)
            if key & 0x80000000:
                continue  # Only numeric icon/group resource IDs are relevant.
            if target & 0x80000000:
                walk(target & 0x7FFFFFFF, path + (key,))
            else:
                rva, size = struct.unpack_from("<II", data, base + target)
                offset = file_offset(rva)
                resources[path + (key,)] = data[offset:offset + size]

    walk(0)
    groups = [(path, value) for path, value in resources.items() if path[0] == 14]
    assert groups, "Missing RT_GROUP_ICON"
    for path, group in groups:
        reserved, kind, count = struct.unpack_from("<HHH", group)
        assert (reserved, kind) == (0, 1), "Invalid icon group"
        actual = {}
        for i in range(count):
            w, h, _, _, _, _, size, icon_id = struct.unpack_from("<BBBBHHIH", group, 6 + 14 * i)
            payload = resources[(3, icon_id, path[2])]
            assert len(payload) == size, "Wrong icon resource size"
            actual[(w or 256, h or 256)] = payload
        assert actual == expected, "Executable icon differs from the approved Asteria ICO"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--architecture", choices=("x64", "arm64"), required=True)
    args = parser.parse_args()
    expected = ico_frames((args.source_root / "app/asteria.ico").read_bytes())
    assert set(expected) == {(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 128, 256)}
    packages = sorted((args.source_root / f"build/installer-{args.architecture}-release").glob("*.zip"))
    assert packages, "No portable package produced"
    for package in packages:
        with zipfile.ZipFile(package) as archive:
            executables = [name for name in archive.namelist() if Path(name).name == "Asteria.exe"]
            assert len(executables) == 1, "Expected exactly one packaged Asteria.exe"
            verify_pe(archive.read(executables[0]), expected, args.architecture)
        print(f"PASS {package.name}: {args.architecture} PE, exact Asteria icon payloads at all nine sizes")


if __name__ == "__main__":
    main()
