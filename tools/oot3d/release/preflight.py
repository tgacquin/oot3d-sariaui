#!/usr/bin/env python3
"""Read-only checks for a committed, shareable release checkout."""
import argparse
from pathlib import Path, PurePosixPath
import subprocess
from release_version import release_version, version

ROOT = Path(__file__).resolve().parents[3]


def private_path(name):
    path = PurePosixPath(name)
    return (any(part.lower() in {'private', 'user', 'game', 'roms', 'saves', 'analysis', 'captures'}
                for part in path.parts[:-1]) or
            ('handoff' in path.name.lower() and path.suffix.lower() in {'.md', '.txt'}) or
            path.name.lower() == 'development.md' or
            path.suffix.lower() in {'.cci', '.3ds', '.cia', '.cxi', '.sav', '.state'} or
            path.name.lower() in {'aes_keys.txt', 'boot9.bin', 'boot11.bin', 'movable.sed',
                                 'seeddb.bin', 'code-decompressed.bin', 'oot3d-editor.ini'})


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True)


def check(expected=None):
    selected_version = release_version(expected)
    changes = git('status', '--porcelain', '--untracked-files=normal').rstrip()
    if changes:
        raise ValueError(f'Release checkout must be clean and committed. Git reports:\n{changes}')
    status = git('submodule', 'status', '--recursive')
    if any(line.startswith(('-', '+', 'U')) for line in status.splitlines()):
        raise ValueError('Initialize submodules at their recorded revisions.')
    files = git('ls-files', '-z').split('\0')
    blocked = [name for name in files if name and private_path(name)]
    if blocked:
        raise ValueError('Private/game files tracked in release source: ' + ', '.join(blocked))
    required = ('resources/oot3d/release-version.txt', 'tools/oot3d/release/release_version.py',
                'src/oot3d/CMakeLists.txt', 'src/oot3d/randomizer/assets/guest.bin',
                'src/oot3d/randomizer/assets/rewards.bin',
                'externals/oot3d-randomizer/source/settings.cpp',
                'externals/oot3d-randomizer/LICENSE.md',
                'externals/mm3d-imgui/imgui.cpp', 'tools/oot3d/release/build.sh')
    for name in required:
        if name not in files or not (ROOT / name).is_file():
            raise ValueError(f'Missing committed release input: {name}')
    print(f'Release {selected_version}, commit {git("rev-parse", "HEAD").strip()}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version', nargs='?')
    check(parser.parse_args().version)
