#!/usr/bin/env python3
"""Wrap a flat ELF binary as an XEC1 executable.

Usage: mkxec.py <flat.bin> <linked.elf> <out.xec> <load_addr>
"""
import struct
import sys

MAGIC = 0x31434558  # "XEC1"


def elf_bss_size(path):
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 2:
        raise ValueError("not an ELF64 file")
    shoff = struct.unpack_from("<Q", data, 40)[0]
    shentsize = struct.unpack_from("<H", data, 58)[0]
    shnum = struct.unpack_from("<H", data, 60)[0]
    total = 0
    for i in range(shnum):
        off = shoff + i * shentsize
        sh_type = struct.unpack_from("<I", data, off + 4)[0]
        sh_flags = struct.unpack_from("<Q", data, off + 8)[0]
        sh_size = struct.unpack_from("<Q", data, off + 32)[0]
        if sh_type == 8 and (sh_flags & 2):  # SHT_NOBITS, SHF_ALLOC
            total += sh_size
    return total


def elf_entry(path):
    data = open(path, "rb").read()
    return struct.unpack_from("<Q", data, 24)[0]


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(1)

    binpath, elfpath, outpath, load_addr = sys.argv[1:]
    load_addr = int(load_addr, 0)
    image = open(binpath, "rb").read()
    entry = elf_entry(elfpath) - load_addr
    bss = elf_bss_size(elfpath)

    header = struct.pack(
        "<IHHIIIII",
        MAGIC,       # magic
        1,           # version
        0,           # flags
        entry,       # entry offset
        len(image),  # code_size
        0,           # data_size
        bss,         # bss_size
        load_addr,   # load_addr
    )

    with open(outpath, "wb") as f:
        f.write(header)
        f.write(image)

    print(f"mkxec: {outpath} image={len(image)} bss={bss} entry=0x{entry:x} load=0x{load_addr:x}")


if __name__ == "__main__":
    main()
