#!/usr/bin/env python3
"""Fetch pinned authoritative artwork at build time; verify every byte and IHDR."""
import hashlib
import json
import pathlib
import struct
import sys
import urllib.request

def verify(data, spec):
    digest = hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()
    if digest != spec["git_blob"]:
        raise ValueError(f"Git blob mismatch for {spec['file']}: {digest}")
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG signature for {spec['file']}")
    if struct.unpack(">II", data[16:24]) != (spec["width"], spec["height"]):
        raise ValueError(f"Unexpected filmstrip geometry for {spec['file']}")

def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    manifest = json.loads((root / "assets" / "knobs.json").read_text(encoding="utf-8"))
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for spec in manifest["assets"]:
        dest = out / spec["file"]
        if dest.is_file():
            try:
                verify(dest.read_bytes(), spec)
                continue
            except ValueError:
                pass
        url = f"https://raw.githubusercontent.com/{manifest['repository']}/{manifest['commit']}/{spec['path']}"
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read(8 * 1024 * 1024 + 1)
        if len(data) > 8 * 1024 * 1024:
            raise ValueError("Asset too large")
        verify(data, spec)
        tmp = dest.with_suffix(".tmp")
        tmp.write_bytes(data)
        tmp.replace(dest)
        print(f"Verified {spec['file']} {spec['width']}x{spec['height']} {spec['git_blob']}")
if __name__ == "__main__":
    main()
