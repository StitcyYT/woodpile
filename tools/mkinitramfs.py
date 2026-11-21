#!/usr/bin/env python3
import struct, sys, os

MAGIC = 0x53465057  # "WPFS" little-endian

def pack_initramfs(outpath, files):
    entries = b''
    for name, data in files:
        name_bytes = name.encode('utf-8')
        entries += struct.pack('<I', len(name_bytes))
        entries += name_bytes
        entries += struct.pack('<I', len(data))
        entries += data

    total_size = 4 + 4 + len(entries)  # magic + count + entries
    header = struct.pack('<II', MAGIC, len(files))
    with open(outpath, 'wb') as f:
        f.write(header)
        f.write(entries)

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <output> <dir> [dir2 ...]")
        sys.exit(1)

    outpath = sys.argv[1]
    files = []
    for d in sys.argv[2:]:
        for root, dirs, filenames in os.walk(d):
            for fn in filenames:
                fpath = os.path.join(root, fn)
                arcname = os.path.relpath(fpath, d)
                with open(fpath, 'rb') as f:
                    data = f.read()
                files.append((arcname, data))

    pack_initramfs(outpath, files)
    print(f"wpfs: {len(files)} files -> {outpath}")

if __name__ == '__main__':
    main()
