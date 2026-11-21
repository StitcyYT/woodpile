#!/usr/bin/env python3
"""Convert a cursor PNG to a raw RGBA bitmap for the initramfs."""
import sys

def main():
    if len(sys.argv) not in (3, 4):
        print(f"Usage: {sys.argv[0]} <input.png> <output.raw> [size]")
        sys.exit(1)

    try:
        from PIL import Image
    except ImportError:
        print("png2raw: Pillow is required")
        sys.exit(1)

    im = Image.open(sys.argv[1]).convert("RGBA")
    if len(sys.argv) == 4:
        size = int(sys.argv[3])
        im = im.resize((size, size), Image.Resampling.NEAREST)

    with open(sys.argv[2], "wb") as f:
        f.write(im.tobytes())

    print(f"png2raw: {im.width}x{im.height} RGBA -> {sys.argv[2]}")

if __name__ == "__main__":
    main()
