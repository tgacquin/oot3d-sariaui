#!/usr/bin/env python3
"""Audit SariaUI's Dynarmic instruction substitutions without a ROM or emulator build."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import base64
from collections import defaultdict
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def source(path):
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', path.read_text(encoding='utf-8'), flags=re.S)


def required(pattern, text):
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(f'unrecognized hook declaration: {pattern}')
    return match


def dispatchers(root, mode):
    """Follow build guards at instruction fetch, including direct SVC substitutions."""
    code = source(root / 'src/core/arm/dynarmic/arm_dynarmic.cpp')
    code = required(r'MemoryReadCode\(VAddr.*?override\s*\{(.*?)\n    \}', code)[1]
    guards = {
        'ENABLE_OOT3D_PRESENTATION': True,
        'ENABLE_MM3D_PRESENTATION': False,
        'OOT3D_DEV_AREA_LOAD_TRACE': mode == 'dev',
        'ENABLE_OOT3D_PLAYER_MODE': mode == 'player',
    }
    enabled = [True]
    lines = []
    for line in code.splitlines():
        directive = line.strip().split()
        if directive and directive[0] in ('#ifdef', '#ifndef'):
            if directive[1] not in guards:
                raise ValueError(f'unknown hook build guard: {line.strip()}')
            value = guards[directive[1]]
            enabled.append(enabled[-1] and (value if directive[0] == '#ifdef' else not value))
        elif directive == ['#else']:
            enabled[-1] = enabled[-2] and not enabled[-1]
        elif directive == ['#endif']:
            enabled.pop()
        elif directive and directive[0].startswith('#'):
            raise ValueError(f'unsupported build guard: {line}')
        elif enabled[-1]:
            lines.append(line)
    if len(enabled) != 1:
        raise ValueError('unbalanced build guards')
    code = '\n'.join(lines)
    for expr in re.findall(r'\breturn\s+([^;]+);', code):
        call = re.fullmatch(r'OOT3D::([\w:]+)::Instruction\(\s*vaddr\s*,\s*\*instruction\s*\)', expr)
        if expr.strip() == 'instruction':
            continue  # Original instruction when no substitution applies.
        if re.fullmatch(r'memory\.Read32OrNullopt\(\w+\)', expr):
            continue  # Read method on the local CodeMemory adapter.
        if call:
            yield call[1], None
        elif expr.strip() == '0xef000000|OOT3D::Prices::TextSVC':
            site = required(r'vaddr\s*==\s*OOT3D::Prices::(\w+)\.pc', code)[1]
            yield 'Prices', site
        else:
            raise ValueError(f'unrecognized instruction dispatcher: {expr}')


def resolve(value, header, headers):
    if '::' in value:
        namespace, value = value.rsplit('::', 1)
        header = headers[namespace]
    if re.fullmatch(r'0[xX][0-9a-fA-F]+|\d+', value):
        return int(value, 0)
    return int(required(r'\b' + re.escape(value) + r'\s*=\s*(0x[\da-fA-F]+)\s*[,;]', header)[1], 16)


def table_addresses(name, header, headers):
    table = required(r'\bstd::array<[^<>]+,\s*(\d+)\s*>\s*' + name + r'\s*\{\{(.*?)\}\}\s*;', header)
    rows = re.findall(r'\{\s*([\w:]+)\s*,', table[2])
    if len(rows) != int(table[1]):
        raise ValueError(f'could not read every entry in {name}')
    return [resolve(row, header, headers) for row in rows]


def randomizer_patches(header, guest):
    # Match the native code / data boundary used by Randomizer::CodePatches.
    limit = int(required(r'patch\.address\s*<\s*(0x[\da-fA-F]+)', header)[1], 16)
    for patch in guest['patches']:
        if patch['address'] < limit:
            size = len(base64.b64decode(patch['data'], validate=True))
            for offset in range(0, size - 3, 4):
                yield patch['address'] + offset, patch['name']


def instruction_sites(owner, direct, headers, guest, rewards):
    header = headers[owner]
    if direct:
        expressions = [direct + '.pc']
    else:
        body = required(r'\bInstruction\([^)]*\)\s*\{(.*?)\n\}', header)[1]
        # All ordinary selectors compare pc to a constant or a member of a site table.
        # Reject unfamiliar uses of pc so a new handler cannot silently evade the audit.
        selectors = r'\bpc\s*==\s*([^&?;)]+?)\s*&&'
        expressions = re.findall(selectors, body)
        remaining = re.sub(selectors, '', body)
        if owner == 'Randomizer':
            for address, name in randomizer_patches(header, guest):
                yield address, name
            remaining = remaining.replace('CodePatches().find(pc)', '').replace('hook.target-pc-8', '')
        if re.search(r'\bpc\b', remaining) or not expressions:
            raise ValueError(f'{owner}: unrecognized instruction address selector')

    for expr in expressions:
        expr = expr.strip()
        label = expr
        if owner == 'Randomizer' and expr == 'hook.address':
            for hook in rewards['hooks']:
                yield hook['address'], f"reward branch to {hook['target']:#x}"
            continue
        symbol = re.fullmatch(r'(\w+)\.address(?:\s*\+\s*(0x\w+))?', expr)
        if symbol and owner == 'Randomizer':
            label = required(r'\b' + symbol[1] + r'\s*=\s*Data\(\)\.Find\("([^"]+)"\)', body)[1]
            addresses = [guest['symbols'][label]['address'] + int(symbol[2] or '0', 0)]
        elif re.fullmatch(r'\w+\[i\](?:\.code)?\.pc', expr):
            label = expr.split('[')[0]
            addresses = table_addresses(label, header, headers)
        elif expr.endswith('.pc'):
            name = expr.split('.')[0]
            loop = None if direct else re.search(r'for\s*\(\s*const auto\s*&\s*' + name + r'\s*:\s*(\w+)\s*\)', body)
            if loop:
                label = loop[1]
                addresses = table_addresses(label, header, headers)
            else:
                value = required(r'\bSite\s+' + name + r'\s*\{\s*([\w:]+)\s*,', header)[1]
                addresses = [resolve(value, header, headers)]
        else:
            addresses = [resolve(expr, header, headers)]
        for i, address in enumerate(addresses):
            yield address, f'{label}[{i}]' if len(addresses) > 1 else label


def audit(root, mode):
    folder = root / 'src/oot3d'
    headers = defaultdict(str)
    for path in sorted(folder.rglob('*.h')):
        body = source(path)
        # Headers may extend the same namespace (e.g. Prices and its text hook).
        parts = re.split(r'\bnamespace\s+OOT3D::([\w:]+)\s*\{', body)
        for namespace, section in zip(parts[1::2], parts[2::2]):
            headers[namespace] += '\n' + section
    assets = folder / 'randomizer/assets'
    guest = json.loads((assets / 'guest.json').read_bytes())
    rewards = json.loads((assets / 'rewards.json').read_bytes())
    claims = defaultdict(list)
    for owner, direct in dispatchers(root, mode):
        for address, label in instruction_sites(owner, direct, headers, guest, rewards):
            claims[address].append(f'{owner}::{label}')
    if not claims:
        raise ValueError('no instruction substitutions found')
    return claims


def collisions(claims):
    # Randomized bean purchases call Host_PurchasePrice themselves. Ordinary
    # files fall through to Prices. Order matters. The release ARM smoke test
    # exercises both at the real call site, including the shuffled reward flag.
    shared = {0x14eda8: ['Randomizer::BeanDaddyPurchase', 'Prices::Sites[68]']}
    return {pc: owners for pc, owners in claims.items()
            if len(owners) > 1 and owners != shared.get(pc)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['dev', 'player'], required=True)
    mode = parser.parse_args(argv).mode
    claims = audit(ROOT, mode)
    conflicts = collisions(claims)
    for pc, owners in sorted(conflicts.items()):
        print(f'{mode} hook collision at {pc:#010x}: {", ".join(owners)}', file=sys.stderr)
    if conflicts:
        return 1
    print(f'{mode}: checked {len(claims)} instruction addresses')
    return 0


if __name__ == '__main__':
    sys.exit(main())
