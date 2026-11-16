#!/usr/bin/env python3
"""Check the imported assembly/C boundary; no ROM or third-party Python modules needed."""
import argparse
import base64
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]


def source_contracts(root=ROOT):
    source = root / 'externals/oot3d-randomizer/code/src'
    assembly = source / 'asm'
    bridges = dict(re.findall(r'^ABI_BRIDGE (\w+), (void|value)$',
                             (assembly / 'hook_abi.s').read_text(), re.M))
    assert bridges, 'Missing hook ABI contracts'
    definitions = {}
    all_c_functions = set()
    for path in source.rglob('*.c'):
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
        text = text.replace('__attribute__((naked))', '')
        for match in re.finditer(r'^([\w *]+?)\s+(\w+)\s*\(([^()]*)\)\s*\{', text, re.M):
            result, name, args = match.groups()
            all_c_functions.add(name)
            if name in bridges:
                assert name not in definitions, f'Ambiguous definition: {name}'
                definitions[name] = (result.strip(), re.sub(r'\s+', ' ', args).strip())
    scalar = {'u8', 's8', 'u16', 's16', 'u32', 's32', 'f32', 'Bool', 'bool',
              'TextControlCode', 'Draw_Display', 'GsParamPointerType'}
    for name, kind in bridges.items():
        assert name in definitions, f'No C definition for {name}'
        result, args = definitions[name]
        assert kind == ('void' if result == 'void' else 'value'), f'Return contract changed: {name}'
        assert result == 'void' or '*' in result or result in scalar, f'Unsupported result: {name}: {result}'
        params = [] if args in ('', 'void') else args.split(',')
        assert len(params) <= 4, f'Stack arguments need another bridge: {name}'
        for param in params:
            assert '*' in param or param.strip().split()[0] in scalar, f'Unsupported argument: {name}: {param}'
    calls = []
    for path in (assembly / 'hooks.s', assembly / 'patches.s'):
        for match in re.finditer(r'^\s*b(?:l|lx)?(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?\s+(\w+)', path.read_text(), re.M):
            target = match[1]
            # The boot loader has its own section/ABI and is not exported by
            # this integration. All gameplay C calls must have a contract.
            assert target not in all_c_functions or target == 'loader_main', f'Unprotected call in {path.name}: {target}'
            if target.startswith('hookabi_'):
                kind, name = target[len('hookabi_'):].split('_', 1)
                assert bridges.get(name) == kind, f'Unknown bridge: {target}'
                calls.append(target)
    assert set(bridges) == {name.split('_', 2)[2] for name in calls}, 'Unused/missing bridge declaration'
    return bridges, calls, all_c_functions


def exported_audit(root=ROOT):
    bridges, calls, all_c_functions = source_contracts(root)
    meta = json.loads((root / 'src/oot3d/randomizer/assets/guest.json').read_text())
    symbols, base = meta['symbols'], meta['base']
    c_addresses = {symbols[name]['address']: name for name in all_c_functions
                   if name in symbols and symbols[name]['address'] >= base}
    for name, kind in bridges.items():
        assert symbols[f'hookabi_{kind}_{name}']['size'] == 16, f'Missing/malformed thunk: {name}'
    # Inspect the actual exported patches, including patches defined outside
    # patches.s. C function-pointer replacements are ordinary ABI callbacks;
    # only instruction branches from native code need inline-hook protection.
    inventory = []
    for patch in meta['patches']:
        code = base64.b64decode(patch['data'])
        targets = []
        for offset in range(0, len(code)-3, 4):
            word = int.from_bytes(code[offset:offset+4], 'little')
            if word & 0x0e000000 != 0x0a000000 or word >> 28 == 15:
                continue
            displacement = (word & 0xffffff) << 2
            if displacement & 0x2000000:
                displacement -= 0x4000000
            target = patch['address'] + offset + 8 + displacement
            assert target not in c_addresses, f"Unprotected exported branch: {patch['name']} -> {c_addresses[target]}"
            if target >= base:
                targets.append(hex(target))
        inventory.append({'patch': patch['name'], 'address': hex(patch['address']),
                          'bytes': len(code), 'guest_branches': targets})
    return {'exported_patches': len(inventory), 'declared_C_bridges': len(bridges),
            'assembly_calls_using_bridges': len(calls), 'patches': inventory}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = exported_audit()
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f"Audited {result['exported_patches']} exported patches, "
          f"{result['declared_C_bridges']} C contracts and "
          f"{result['assembly_calls_using_bridges']} assembly/C calls")
