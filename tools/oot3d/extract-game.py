#!/usr/bin/env python3
"""Extract a decrypted cartridge into a new local analysis directory with 3dstool."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tool', default=shutil.which('3dstool'), help='Path to 3dstool')
    args = parser.parse_args()
    if not args.tool:
        parser.error('Provide --tool /path/to/3dstool')
    tool = str(Path(args.tool).resolve())
    rom, root = args.rom.resolve(), args.output.resolve()
    if not rom.is_file():
        parser.error('ROM does not exist')
    if root.exists():
        parser.error('Output already exists; choose a new folder to preserve previous analysis')
    root.mkdir(parents=True)
    containers = root / 'containers'
    containers.mkdir()
    steps = [
        ['-x', '-t', 'cci', '-f', str(rom), '--header', str(containers / 'ncsd-header.bin'),
         '-0', str(containers / 'game.cxi')],
        ['-x', '-t', 'cxi', '-f', str(containers / 'game.cxi'),
         '--header', str(containers / 'ncch-header.bin'), '--exh', str(root / 'exheader.bin'),
         '--exefs', str(containers / 'exefs.bin'), '--romfs', str(containers / 'romfs.bin')],
        ['-x', '-t', 'exefs', '-f', str(containers / 'exefs.bin'), '--exefs-dir', str(root / 'exefs')],
        ['-x', '-t', 'romfs', '-f', str(containers / 'romfs.bin'), '--romfs-dir', str(root / 'romfs')],
    ]
    with (root / 'extraction.log').open('w') as log:
        for step in steps:
            print('Extracting', step[2], flush=True)
            subprocess.run([tool, *step], stdout=log, stderr=subprocess.STDOUT, check=True)
        if (root / 'exheader.bin').read_bytes()[0x0d] & 1:
            subprocess.run([tool, '-u', '-f', str(root / 'exefs/code.bin'),
                            '--compress-type', 'blz', '--compress-out',
                            str(root / 'exefs/code-decompressed.bin')],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        else:
            shutil.copyfile(root / 'exefs/code.bin', root / 'exefs/code-decompressed.bin')
    subprocess.run([sys.executable, str(Path(__file__).with_name('index-game.py')),
                    str(root), str(rom)], check=True)


if __name__ == '__main__':
    main()
