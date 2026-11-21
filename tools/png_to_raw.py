#!/usr/bin/env python3
"""Turn a PNG into a tiny header + raw RGB blob the kernel can stretch.

The kernel has no PNG/deflate decoder.  Conversion happens at build time so
panic_worst() only has to read a flat buffer from the initramfs.
"""
import struct
import sys

from PIL import Image


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: png_to_raw.py INPUT.png OUTPUT.raw", file=sys.stderr)
        return 1

    image = Image.open(sys.argv[1]).convert("RGB")
    width, height = image.size
    if width <= 0 or height <= 0:
        print("png_to_raw: empty image", file=sys.stderr)
        return 1

    with open(sys.argv[2], "wb") as out:
        out.write(struct.pack("<II", width, height))
        out.write(image.tobytes())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
