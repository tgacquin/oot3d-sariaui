#!/usr/bin/env python3
"""Verify EN/FR/ES patch generation, script controls and native-font line widths.
Usage: message-catalog-check.py us.qm ltn16.qbf patched.qm [preview.txt]
"""
from pathlib import Path
import json
import re
import runpy
import struct
import sys

root=Path(__file__).resolve().parents[2]
m=runpy.run_path(str(Path(__file__).with_name('build-message-patches.py')))
original=Path(sys.argv[1]).read_bytes();font=Path(sys.argv[2]).read_bytes()
patched=Path(sys.argv[3]).read_bytes()
count=struct.unpack_from('<I',original,8)[0]
def records(b):
    result={}
    for i in range(count):
        at=16+i*96;mid=struct.unpack_from('<I',b,at)[0]
        result[mid]=[]
        for slot in range(10):
            off,size=struct.unpack_from('<II',b,at+16+slot*8)
            result[mid].append(b[off:off+size])
    return result
old=records(original);new=records(patched)
assert font[:4]==b'QBF1'
glyphs=struct.unpack_from('<H',font,4)[0]
advances={struct.unpack_from('<H',font,16+i*8)[0]:font[16+i*8+5]+font[15] for i in range(glyphs)}
def visible(s):
    s=re.sub(r'\{24:([0-9a-f]+)\}',lambda match:'['+{'1e':'X','1f':'Y','0a':'L','0b':'R','06':'A','07':'B','00':'Circle Pad'}.get(match[1],'icon '+match[1])+']',s)
    return re.sub(r'\{[^}]+\}', '',s.replace('{01}','\n\n').replace('{0b}','<Link>'))
def widths(s):
    # Use the eight widest letters for the player's editable name. Native
    # button icons occupy one 16px cell, irrespective of their report labels.
    s=re.sub(r'\{24:[0-9a-f]+\}', '□',s).replace('{0b}','WWWWWWWW').replace('{01}','\n')
    lines=re.sub(r'\{[^}]+\}','',s).splitlines()
    return [sum(16 if c=='□' else advances[ord(c)] for c in line) for line in lines]
review=['CONTROLLER TEXT: 78 controller messages each in English, French and Spanish.',
        'Labels in brackets represent native icons.\n']
expected_changes=set()
for language in ('english','french','spanish'):
    catalog=json.loads((root/f'src/oot3d/messages/{language}.json').read_text())
    slot=catalog['language_slot'];compiled,_=m['compile'](original,catalog)
    assert len(compiled)==78
    ids={entry[0] for entry in compiled}
    if slot==1:english_ids=ids
    else:assert ids==english_ids
    for mid,h,size,edits in compiled:
        source=old[mid][slot];result=bytearray();cursor=0
        for off,remove,insert in edits:
            result+=source[cursor:off]+insert;cursor=off+remove
        result+=source[cursor:]
        text=m['decode'](result);before=m['decode'](source)
        assert m['script_controls'](text)==m['script_controls'](before),(language,hex(mid))
        assert max(widths(text))<=max(310,max(widths(before))),(language,hex(mid),max(widths(text)))
        plain=' '.join(visible(text).split())
        assert not re.search(r"écran (tactile|supérieur|inférieur)|pantalla (táctil|superior|inferior)",plain,re.I),(language,hex(mid))
        assert not re.search(r"\{24:(16|17|18|19|1a|1b|1c|1d)\}",text),(language,hex(mid))
        for old_page,new_page in zip(re.split(r'\{(?:01|08:..)\}\n?',before),re.split(r'\{(?:01|08:..)\}\n?',text)):
            assert len(new_page.splitlines())<=max(4,len(old_page.splitlines())),(language,hex(mid))
        if slot==1:
            assert not re.search(r'\b(tap|touch|subscreen|upper.screen|lower.screen)\b',visible(text),re.I),hex(mid)
        assert new[mid][slot]==result,(language,hex(mid))
        expected_changes.add((mid,slot))
        review.append(f'{language.upper()} 0x{mid:04X}\n'+visible(text)+'\n')
for mid,locales in old.items():
    for slot in range(10):
        if (mid,slot) not in expected_changes:
            assert new[mid][slot]==locales[slot],(hex(mid),slot)
        # Scan all populated messages, including IDs outside the controller catalogs.
        if slot in (1,5,7) and new[mid][slot]:
            text=m['decode'](new[mid][slot])
            assert not re.search(r"\{24:(16|17|18|19|1a|1b|1c|1d)\}",text),(hex(mid),slot)
            plain=' '.join(visible(text).split())
            assert not re.search(r"écran (tactile|inférieur|supérieur)|pantalla (táctil|inferior|superior)",plain,re.I),(hex(mid),slot)
if len(sys.argv)>4:Path(sys.argv[4]).write_text('\n'.join(review))
print('234 localized rewrites match catalogs; script controls, native-font widths, retired Navi/View references and untouched payloads verified')
