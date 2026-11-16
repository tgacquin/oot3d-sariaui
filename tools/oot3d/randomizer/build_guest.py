#!/usr/bin/env python3
"""Build the pinned upstream ARM payload with a standard arm-none-eabi toolchain."""
from pathlib import Path
import concurrent.futures
import subprocess
import sys
import struct
import json
import base64
from hook_abi_audit import source_contracts, exported_audit

root = Path(__file__).resolve().parents[3]
source = root / 'externals/oot3d-randomizer'
source_contracts(root)
build = Path(sys.argv[1] if len(sys.argv) > 1 else '/tmp/oot-rando-guest')
build.mkdir(parents=True, exist_ok=True)
includes = [Path(__file__).parent / 'guest_include', source / 'code/include', source / 'shared']
includes += sorted({p.parent for p in (source / 'code/src').rglob('*.h')})
flags = ['-std=gnu11', '-ffreestanding', '-O2', '-fno-builtin', '-fno-strict-aliasing',
         '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=softfp', '-mfpu=vfpv2', '-mtp=soft',
         '-ffunction-sections', '-fdata-sections', '-DARM11', '-D_3DS']
flags += ['-I' + str(p) for p in includes]
sources = list((source / 'shared').glob('*.c')) + sorted((source / 'code/src').rglob('*.c')) + sorted((source / 'code/src').rglob('*.s'))

def compile_one(path):
    obj = build / (str(path.relative_to(source)).replace('/', '_') + '.o')
    result = subprocess.run(['arm-none-eabi-gcc', *flags, *(['-include', 'stddef.h'] if path.suffix == '.c' else []), '-c', str(path), '-o', str(obj)],
                            text=True, capture_output=True)
    return obj, result

objects = []
failed = False
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    for obj, result in pool.map(compile_one, sources):
        if result.returncode:
            print(result.stderr)
            failed = True
        objects.append(str(obj))
if failed:
    sys.exit(1)
subprocess.run(['arm-none-eabi-gcc', '-nostdlib', '-march=armv6k', '-mfloat-abi=softfp',
                '-mfpu=vfpv2', '-Wl,--defsym,_LD_USA=1', '-Wl,--defsym,_LD_EUR=0',
                '-Wl,-Map=' + str(build / 'payload.map'), '-T', str(source / 'code/oot.ld'),
                *objects, '-lgcc', '-o', str(build / 'payload.elf')], check=True)

# Export only relocatable patch records and the injected payload. Normal builds
# consume these checked-in resources and do not need an ARM compiler installed.
blob = (build / 'payload.elf').read_bytes()
assert blob[:6] == b'\x7fELF\x01\x01'
shoff = struct.unpack_from('<I', blob, 32)[0]
shsize, shcount, shstr = struct.unpack_from('<HHH', blob, 46)
sections = [struct.unpack_from('<10I', blob, shoff + i * shsize) for i in range(shcount)]
strings = sections[shstr]
names = blob[strings[4]:strings[4] + strings[5]]
excluded = {
    'loader', 'CamUpdate', 'CamRoll', 'Gfx_Update', 'AwakeCallback', 'SleepQueryCallback',
    'FairyReviveHealAmount', 'FairyUseHealAmount', 'ApplyDamageMultiplier',
    'TitleLinkObject', 'SaveMenuIgnoreOpen', 'GameOverMenuSkip',
    # File management is routed through the host for ordinary and randomized files.
    'FileSelect_CopyFile', 'SaveGame', 'BeforeLoadGame', 'SaveFile_Init',
}
patches = []
out = root / 'src/oot3d/randomizer/assets'
out.mkdir(exist_ok=True)
for section in sections:
    name = names[section[0]:].split(b'\0', 1)[0].decode()
    data = blob[section[4]:section[4] + section[5]]
    if name == '.text':
        (out / 'guest.bin').write_bytes(data)
        payload_address = section[3]
    elif name.startswith('.patch_') and name[7:] not in excluded:
        patches.append({'name': name[7:], 'address': section[3],
                        'data': base64.b64encode(data).decode()})
symbols = {}
output = subprocess.check_output(['arm-none-eabi-nm', '-S', str(build / 'payload.elf')], text=True)
for line in output.splitlines():
    parts = line.split()
    if len(parts) == 4:
        symbols[parts[3]] = {'address': int(parts[0], 16), 'size': int(parts[1], 16)}
    elif len(parts) == 3:
        symbols[parts[2]] = {'address': int(parts[0], 16), 'size': 0}
(out / 'guest.json').write_text(json.dumps({'base': payload_address, 'patches': patches,
                                          'symbols': symbols}, separators=(',', ':')))
print('Exported', len(patches), 'patches and', (out / 'guest.bin').stat().st_size, 'payload bytes')
exported_audit(root)
