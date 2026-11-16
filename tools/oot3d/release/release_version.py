#!/usr/bin/env python3
"""Shared SariaUI version for CI, packages and application builds."""
from pathlib import Path
import re

VERSION_FILE = Path(__file__).resolve().parents[3] / 'resources/oot3d/release-version.txt'


def version(value):
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?', value):
        raise ValueError('Use a version such as 1.0.0 or 1.0.0-beta.1 (no leading v).')
    return value


def release_version(expected=None):
    value = version(VERSION_FILE.read_text().strip())
    if expected is not None and version(expected) != value:
        raise ValueError(f'Release version {expected} does not match {VERSION_FILE.name}: {value}')
    return value


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('expected', nargs='?')
    print(release_version(parser.parse_args().expected))
