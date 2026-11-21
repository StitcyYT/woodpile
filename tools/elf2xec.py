#!/usr/bin/env python3
"""Convert a static x86-64 ELF executable into a Woodpile XEC1 image.

Unlike tools/mkxec.py, this reads the ELF load segment directly instead of
asking objcopy for a flat binary. It refuses ELF files that need a host
loader (interpreter, dynamic segment, shared-library sections), so the
result is a bare image that can run against the Woodpile ABI jump table.

Usage:
    python3 tools/elf2xec.py input.elf output.xec 0x900000
"""

import struct
import sys

MAGIC = 0x31434558  # "XEC1"
HEADER = struct.Struct("<IHHIIIII")  # magic, ver, flags, entry, code, data, bss, load

ET_EXEC = 2
EM_X86_64 = 62

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3

SHT_DYNAMIC = 6
SHT_NOBITS = 8
SHT_SYMTAB = 2
SHT_RELA = 4
SHT_REL = 9

BAD_SECTIONS = {
    ".interp", ".dynamic", ".dynsym", ".dynstr", ".got", ".got.plt",
    ".plt", ".rela.plt", ".rel.plt",
}

ELF64_HDR = struct.Struct("<16sHHIQQQIHHHHHH")
ELF64_PHDR = struct.Struct("<IIQQQQQQ")
ELF64_SHDR = struct.Struct("<IIQQQQIIQQ")


def die(message):
    print("elf2xec: " + message, file=sys.stderr)
    raise SystemExit(1)


def parse_int(value):
    try:
        return int(value, 0)
    except ValueError:
        die("load address must be an integer, got %r" % value)


def read_elf(path):
    with open(path, "rb") as handle:
        return handle.read()


def main(argv):
    if len(argv) != 4:
        die("usage: elf2xec.py input.elf output.xec load_addr")

    src, dst = argv[1], argv[2]
    load_addr = parse_int(argv[3])
    data = read_elf(src)

    if len(data) < ELF64_HDR.size:
        die("file is too small to be ELF64")
    ident, etype, machine, version, entry, phoff, shoff, _flags, ehsize, \
        phentsize, phnum, shentsize, shnum, shstrndx = ELF64_HDR.unpack_from(data)

    if ident[:4] != b"\x7fELF":
        die("not an ELF file")
    if ident[4] != 2 or ident[5] != 1:
        die("only little-endian ELF64 is supported")
    if etype != ET_EXEC:
        die("only ET_EXEC is supported (build without -pie)")
    if machine != EM_X86_64:
        die("only x86-64 ELF files are supported")
    if version != 1 or ehsize != ELF64_HDR.size:
        die("unsupported ELF header")
    if phoff == 0 or phnum == 0 or phentsize != ELF64_PHDR.size:
        die("ELF has no usable program headers")

    loads = []
    for index in range(phnum):
        off = phoff + index * phentsize
        if off + phentsize > len(data):
            die("program header table is truncated")
        p_type, p_flags, p_offset, p_vaddr, _paddr, p_filesz, p_memsz, _align = \
            ELF64_PHDR.unpack_from(data, off)
        if p_type == PT_INTERP:
            die("ELF needs a dynamic loader (PT_INTERP)")
        if p_type == PT_DYNAMIC:
            die("ELF needs dynamic linking (PT_DYNAMIC)")
        if p_type == PT_LOAD:
            loads.append((p_offset, p_vaddr, p_filesz, p_memsz))

    if len(loads) != 1:
        die("XEC images need exactly one PT_LOAD segment, found %d" % len(loads))

    p_offset, p_vaddr, p_filesz, p_memsz = loads[0]
    if p_vaddr != load_addr:
        die("load address mismatch: ELF says 0x%x, requested 0x%x" %
            (p_vaddr, load_addr))
    if p_filesz == 0 or p_memsz < p_filesz:
        die("bad PT_LOAD sizes")
    if p_offset + p_filesz > len(data):
        die("PT_LOAD segment is truncated")
    if entry < p_vaddr or entry >= p_vaddr + p_filesz:
        die("ELF entry point is outside the load segment")

    if shoff and shnum:
        if shstrndx >= shnum:
            die("bad section name table index")
        sections = []
        for index in range(shnum):
            off = shoff + index * shentsize
            if off + shentsize > len(data):
                die("section header table is truncated")
            sh = ELF64_SHDR.unpack_from(data, off)
            sections.append(sh)
        strtab = sections[shstrndx]
        names = data[strtab[4]:strtab[4] + strtab[5]]

        for sh in sections:
            sh_type = sh[1]
            if sh_type == SHT_DYNAMIC:
                die("ELF contains a dynamic section")
            if sh_type in (SHT_RELA, SHT_REL):
                name_off = sh[0]
                name = ""
                if name_off < len(names):
                    end = names.find(b"\0", name_off)
                    name = names[name_off:end if end >= 0 else None].decode(
                        "utf-8", "replace")
                if name.startswith(".rela.plt") or name.startswith(".rel.plt"):
                    die("ELF contains PLT relocations")
            name_off = sh[0]
            if name_off < len(names):
                end = names.find(b"\0", name_off)
                name = names[name_off:end if end >= 0 else None].decode(
                    "utf-8", "replace")
                if name in BAD_SECTIONS:
                    die("ELF contains host-loader section %s" % name)

    entry_offset = entry - p_vaddr
    code_size = p_filesz
    bss_size = p_memsz - p_filesz
    image = data[p_offset:p_offset + code_size]

    header = HEADER.pack(MAGIC, 1, 0, entry_offset, code_size,
                         0, bss_size, load_addr)
    with open(dst, "wb") as handle:
        handle.write(header)
        handle.write(image)

    print("elf2xec: %s -> %s entry=0x%x code=%d bss=%d load=0x%x" %
          (src, dst, entry_offset, code_size, bss_size, load_addr))


if __name__ == "__main__":
    main(sys.argv)
