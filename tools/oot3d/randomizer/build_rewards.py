#!/usr/bin/env python3
"""Build optional reward presentation against the pinned, unchanged ARM payload."""
from pathlib import Path
import json, struct, subprocess, hashlib

root=Path(__file__).resolve().parents[3]
assets=root/'src/oot3d/randomizer/assets'
meta=(assets/'guest.json').read_bytes()
guest=(assets/'guest.bin').read_bytes()
symbols=json.loads(meta)['symbols']
source=root/'externals/oot3d-randomizer'
build=Path('/private/tmp/oot-reward-extension');build.mkdir(exist_ok=True)
base=0x900000
assert max(s['address']+s['size'] for s in symbols.values() if 0x5c7000<=s['address']<0xa00000)<base
hooks={'Matrix':'ItemOverride_EditDrawGetItemAfterMatrixUpdate','Send':'udsSendTo',
       'Receive':'Multiplayer_Receive_GhostData','Draw':'EnLinkPuppet_Draw',
       'Destroy':'EnLinkPuppet_Destroy','Shutdown':'Multiplayer_Shutdown'}
asm=['.syntax unified','.arm','.text']
sites=[]
for wrapper,name in hooks.items():
    address=symbols[name]['address']
    word=struct.unpack_from('<I',guest,address-0x5c7000)[0]
    asm += [f'.global Original_{wrapper}',f'Original_{wrapper}:']
    if word&0xffff0000==0xe59f0000: # Relocate a PC-relative literal load.
        reg=(word>>12)&15
        literal=struct.unpack_from('<I',guest,address+8+(word&4095)-0x5c7000)[0]
        asm += [f'ldr r{reg}, ={literal}']
    else:
        assert word&0xffff0000==0xe92d0000 or word==0xe2800f6a,hex(word)
        asm += [f'.word {word}']
    asm += [f'b 0x{address+4:x}']
    sites.append((address,word,wrapper))
(build/'bridges.s').write_text('\n'.join(asm)+'\n')
includes=[Path(__file__).parent/'guest_include',source/'code/include',source/'shared']
includes+=sorted({p.parent for p in (source/'code/src').rglob('*.h')})
includes += [source/'code/src']
flags=['-std=gnu11','-ffreestanding','-O2','-fno-builtin','-fno-strict-aliasing',
       '-march=armv6k','-mtune=mpcore','-mfloat-abi=softfp','-mfpu=vfpv2','-mtp=soft',
       '-DARM11','-D_3DS','-Wall','-Werror=implicit-function-declaration']+['-I'+str(p) for p in includes]
objects=[]
for path in [Path(__file__).parent/'rewards/rewards.c',build/'bridges.s']:
    obj=build/(path.stem+'.o');objects.append(str(obj))
    subprocess.run(['arm-none-eabi-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True)
link=['SECTIONS { . = 0x900000; .text : { *(.text*) *(.rodata*) *(.data*) }',
      '.bss : { *(.bss*) *(COMMON) } end = .; ASSERT(end < 0x910000, "reward extension overflow") }']
link += [f'{n} = 0x{s["address"]:x};' for n,s in symbols.items() if n.replace('_','').isalnum()]
(build/'rewards.ld').write_text('\n'.join(link)+'\n')
elf=build/'rewards.elf'
subprocess.run(['arm-none-eabi-gcc','-nostdlib','-march=armv6k','-mfloat-abi=softfp',
                '-mfpu=vfpv2','-T',str(build/'rewards.ld'),*objects,'-lgcc','-o',str(elf)],check=True)
subprocess.run(['arm-none-eabi-objcopy','-O','binary',str(elf),str(build/'rewards.bin')],check=True)
out_symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-n',str(elf)],text=True).splitlines():
    parts=line.split()
    if len(parts)==3 and base<=int(parts[0],16)<0x910000:out_symbols[parts[2]]=int(parts[0],16)
data=(build/'rewards.bin').read_bytes()
# Include zeroed BSS on activation, including subsequent file loads.
data+=bytes(out_symbols['end']-base-len(data))
(assets/'rewards.bin').write_bytes(data)
(assets/'rewards.json').write_text(json.dumps({'base':base,
    'runtime':hashlib.sha256(guest+meta).hexdigest(),'symbols':out_symbols,
    'hooks':[{'address':a,'instruction':w,'target':out_symbols['Rewards_'+n]} for a,w,n in sites]},indent=2)+'\n')
print('Reward extension:',len(data),'bytes; original seed runtime unchanged')
