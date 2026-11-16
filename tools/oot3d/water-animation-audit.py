#!/usr/bin/env python3
"""Inspect local USA Rev 1 water assets; never modify or export game assets."""
import argparse
import json
import struct
from pathlib import Path


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def zar_entries(path):
    data = path.read_bytes()
    assert data[:4] == b'ZAR\x01', path
    count = u16(data, 10)
    files, offsets = u32(data, 16), u32(data, 20)
    for index in range(count):
        size, name_at = struct.unpack_from('<II', data, files + index * 8)
        name = data[name_at:data.index(0, name_at)].decode('ascii')
        start = u32(data, offsets + index * 4)
        assert start + size <= len(data)
        yield index, name, data[start:start + size]


def room_water(romfs):
    entries = list(zar_entries(romfs / 'scene/spot04.zar'))
    _, name, animation = next(e for e in entries if e[1].endswith('spot04_00.cmab'))
    assert animation[:4] == b'cmab' and u32(animation, 4) == 1
    header = u32(animation, 0x14)
    duration = u32(animation, header + 4)
    mads = header + u32(animation, header + 12)
    assert animation[mads:mads + 4] == b'mads'
    room = (romfs / 'scene/spot04_0_info.zsi').read_bytes()
    model_at = room.index(b'cmb ')
    model = room[model_at:model_at + u32(room, model_at + 4)]
    assert u32(model, 8) == 6
    materials, textures = u32(model, 0x28), u32(model, 0x2c)
    assert model[materials:materials + 4] == b'mats'
    assert model[textures:textures + 4] == b'tex '
    result = []
    for index in range(u32(animation, mads + 4)):
        mmad = mads + u32(animation, mads + 8 + index * 4)
        assert animation[mmad:mmad + 4] == b'mmad'
        kind, material, slot = struct.unpack_from('<III', animation, mmad + 4)
        assert kind == 1 and material < u32(model, materials + 8) and slot < 3
        # Version 6 CMB: 0x15c-byte material, 0x18-byte sampler records.
        sampler = materials + 12 + material * 0x15c + 0x10 + slot * 0x18
        texture = u16(model, sampler)
        assert texture < u32(model, textures + 8)
        texture_name = textures + 12 + texture * 0x24 + 0x14
        texture_name = model[texture_name:texture_name + 16].split(b'\0')[0].decode('ascii')
        channels = []
        for axis in range(2):
            relative = u16(animation, mmad + 0x10 + axis * 2)
            if not relative:
                continue
            track = mmad + relative
            method, count, start, end = struct.unpack_from('<IIII', animation, track)
            assert method == 1 and count == 2 and start == 0 and end == duration
            keys = [struct.unpack_from('<If', animation, track + 16 + key * 8)
                    for key in range(count)]
            assert keys[0][0] == 0 and keys[1][0] == duration
            channels.append({'axis': axis, 'keys': keys,
                             'half_frame_delta': (keys[1][1] - keys[0][1]) / duration / 2})
        result.append({'material': material, 'texture_slot': slot,
                       'texture': texture_name, 'track_type': kind, 'channels': channels})
    return {'archive_entry': name, 'duration': duration, 'tracks': result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='Local extracted game directory')
    args = parser.parse_args()
    water = room_water(args.game / 'romfs')
    textures = [{'entry': index, 'name': name, 'bytes': len(data)}
                for index, name, data in zar_entries(args.game / 'romfs/actor/zelda_keep.zar')
                if name in ('soft/tex/soft_splash.ctxb', 'soft/tex/soft_ripple.ctxb')]
    code = (args.game / 'exefs/code-decompressed.bin').read_bytes()
    # Original ARM instructions at the independently traced update points.
    signatures = {0x373c1c: 0xed940a02, 0x373c20: 0xedd40a03,
                  0x373c24: 0xee300a20, 0x373c28: 0xed840a02,
                  0x461fb0: 0xe2411001, 0x461fbc: 0xe1c016b0}
    for pc, expected in signatures.items():
        actual = u32(code, pc - 0x100000)
        assert actual == expected, f'{pc:08x}: {actual:08x} != {expected:08x}'
    profiles = []
    for kind, name, init, draw_literal, update_literal in (
            (9, 'ripple', 0x263818, 0x2639c0, 0x2639c4),
            (10, 'splash', 0x2639d4, 0x263c34, 0x263c38)):
        profile = u32(code, 0x5201a0 - 0x100000 + kind * 4)
        assert u32(code, profile - 0x100000) == kind
        assert u32(code, profile - 0x100000 + 4) == init
        profiles.append({'type': kind, 'name': name, 'init': f'{init:08x}',
                         'draw': f'{u32(code, draw_literal - 0x100000):08x}',
                         'update': f'{u32(code, update_literal - 0x100000):08x}'})
    print(json.dumps({'room_water': water, 'shared_water_textures': textures,
                      'native_effect_profiles': profiles,
                      'verified_native_sites': [f'{pc:08x}' for pc in signatures]}, indent=2))


if __name__ == '__main__':
    main()
