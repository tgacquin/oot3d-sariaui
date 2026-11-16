#!/usr/bin/env python3
"""Index a locally extracted OoT3D ROM and wrap its code for disassembly.

Usage: index-game.py /path/to/analysis/game /path/to/game.cci
The ELF is an analysis container, not a rebuilt or runnable game.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main(root, rom):
    header = (root / 'exheader.bin').read_bytes()
    code = (root / 'exefs/code-decompressed.bin').read_bytes()
    segments = []
    offset = 0
    for name, at, flags in [('text', 0x10, 5), ('rodata', 0x20, 4), ('data', 0x30, 6)]:
        address, pages, actual = struct.unpack_from('<III', header, at)
        size = pages * 0x1000
        assert 0 < actual <= size
        segments.append(dict(name=name, address=address, code_offset=offset,
                             file_size=size, content_size=actual, memory_size=size,
                             elf_flags=flags))
        offset += size
    assert offset == len(code), 'Decompressed code size does not match ExHeader'
    bss_size = struct.unpack_from('<I', header, 0x3c)[0]
    segments[-1]['memory_size'] += (bss_size + 0xfff) & ~0xfff
    metadata = dict(source_rom=rom.name, source_sha256=digest(rom),
                    program_id=f'{struct.unpack_from("<Q", header, 0x200)[0]:016X}',
                    code_sha256=digest(root / 'exefs/code-decompressed.bin'),
                    entrypoint=segments[0]['address'], bss_size=bss_size, segments=segments,
                    note='Analysis ELF uses the emulator NCCH loader page-aligned memory map.')
    (root / 'memory-map.json').write_text(json.dumps(metadata, indent=2) + '\n')

    # ELF32 ARM little-endian, three PT_LOAD segments with original virtual addresses.
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    elf = bytearray(struct.pack('<16sHHIIIIIHHHHHH', ident, 2, 40, 1,
                                segments[0]['address'], 52, 0, 0x05000000,
                                52, 32, 3, 0, 0, 0))
    for segment in segments:
        elf.extend(struct.pack('<8I', 1, 0x1000 + segment['code_offset'],
                               segment['address'], segment['address'], segment['file_size'],
                               segment['memory_size'], segment['elf_flags'], 0x1000))
    elf.extend(bytes(0x1000 - len(elf)))
    elf.extend(code)
    (root / 'oot3d-analysis.elf').write_bytes(elf)

    assets = ['path\tbytes\tsha256\tmagic_hex\n']
    ui_assets = [assets[0]]
    for path in sorted((root / 'romfs').rglob('*')):
        if not path.is_file():
            continue
        relative = path.relative_to(root / 'romfs').as_posix()
        with path.open('rb') as stream:
            magic = stream.read(4).hex()
        row = f'{relative}\t{path.stat().st_size}\t{digest(path)}\t{magic}\n'
        assets.append(row)
        if relative.startswith('menu/') or any(s in relative.lower() for s in ('title', 'logo')):
            ui_assets.append(row)
    (root / 'assets.tsv').write_text(''.join(assets))
    (root / 'ui-assets.tsv').write_text(''.join(ui_assets))

    strings = ['address\tsegment\ttext\n']
    candidates = [strings[0]]
    for segment in segments:
        start = segment['code_offset']
        data = code[start:start + segment['content_size']]
        for match in re.finditer(rb'[\x20-\x7e]{6,}', data):
            text = match.group().decode('ascii')
            row = f"{segment['address'] + match.start():08x}\t{segment['name']}\t{text}\n"
            strings.append(row)
            if any(s in text.lower() for s in ('title', 'file_select', 'menu_', 'hud_', '.qly')):
                candidates.append(row)
    (root / 'code-strings.tsv').write_text(''.join(strings))
    (root / 'ui-strings.tsv').write_text(''.join(candidates))
    print(f'Indexed {len(assets)-1} assets, {len(ui_assets)-1} UI candidates, '
          f'{len(strings)-1} code strings; analysis ELF generated.')


if __name__ == '__main__':
    main(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
