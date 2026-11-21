#!/usr/bin/env python3
"""Convert any RPLAY3 container into the tiny raw container used by woodpile.

The woodpile rplay.xec has a mini zlib decoder and a trivial raw-container
reader. This tool keeps all existing PLAY3 files playable by undoing their
host-side compression and rewriting the header as rplay3-raw.
"""
import bz2
import json
import lzma
import shutil
import struct
import sys
import zlib
from pathlib import Path

MAGIC = b"RPLAY3\x00\x02"


def convert(src: Path, dst: Path) -> bool:
    raw = src.read_bytes()
    if not raw.startswith(MAGIC) or len(raw) < 12:
        print(f"rplay_prep: skip {src} (bad magic)")
        return False
    hsize = struct.unpack_from("<I", raw, 8)[0]
    header = json.loads(raw[12:12 + hsize].decode("utf-8"))
    compressed = raw[12 + hsize:]
    container = header.get("container")

    if container == "rplay3-raw":
        payload = compressed
    elif container == "rplay3-zlib":
        payload = zlib.decompress(compressed)
    elif container == "rplay3-bz2":
        payload = bz2.decompress(compressed)
    elif container == "rplay3-xz":
        payload = lzma.decompress(compressed)
    elif container == "rplay3-bz2-zlib":
        payload = bz2.decompress(zlib.decompress(compressed))
    else:
        print(f"rplay_prep: skip {src} (unknown container {container})")
        return False

    header["container"] = "rplay3-raw"
    body = json.dumps(header, separators=(",", ":"), sort_keys=True).encode("utf-8")
    dst.parent.mkdir(parents=True, exist_ok=True)
    with dst.open("wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<I", len(body)))
        f.write(body)
        f.write(payload)
    print(f"rplay_prep: {src.name} -> {dst.name} ({len(payload)} bytes payload)")
    return True


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: rplay_prep.py <source-dir> <output-dir>")
        return 1
    sources = Path(sys.argv[1])
    output = Path(sys.argv[2])
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)

    count = 0
    for src in sorted(sources.rglob("*.rplay3")):
        if convert(src, output / src.name):
            count += 1
    print(f"rplay_prep: converted {count} file(s)")
    return 0 if count else 1


if __name__ == "__main__":
    raise SystemExit(main())
