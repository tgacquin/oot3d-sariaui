#!/usr/bin/env python3
"""Assemble checksums when all selected desktop packages and source exist."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from release_version import release_version as configured_version
from platforms import CHOICES, PLATFORMS, selected_platforms


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def collect(directory, release_version, commit, platform='all'):
    configured_version(release_version)
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('Expected a full Git commit ID')
    selected = selected_platforms(platform)
    suffixes = [f'{name}.{PLATFORMS[name][1]}' for name in selected] + ['Source.tar.gz']
    names = [f'SariaUI-{release_version}-{suffix}' for suffix in suffixes]
    records = []
    for name in names:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f'Missing/empty candidate artifact: {name}')
        checksum = digest(path)
        sidecar = path.with_name(name + '.sha256')
        if name.endswith('-Source.tar.gz'):
            pass
        elif not sidecar.is_file() or sidecar.read_text().strip() != f'{checksum}  {name}':
            raise ValueError(f'Package checksum mismatch: {name}')
        records.append(dict(file=name, sha256=checksum))
    (directory / 'SHA256SUMS').write_text(''.join(f'{r["sha256"]}  {r["file"]}\n' for r in records))
    (directory / 'release-manifest.json').write_text(json.dumps(dict(
        version=release_version, commit=commit, status='unpublished-candidate',
        platforms=selected, artifacts=records), indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('version')
    parser.add_argument('commit')
    parser.add_argument('--platform', choices=CHOICES, default='all')
    args = parser.parse_args()
    collect(args.directory, args.version, args.commit, args.platform)
