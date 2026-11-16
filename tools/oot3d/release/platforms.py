#!/usr/bin/env python3
"""Select desktop runners and package suffixes for a release candidate."""
import argparse
import json


PLATFORMS = {
    'windows-x64': ('windows-2022', 'zip'),
    'macos-arm64': ('macos-15', 'zip'),
    'linux-x86_64': ('ubuntu-24.04', 'tar.gz'),
}
CHOICES = ('all', *PLATFORMS)


def selected_platforms(selection):
    if selection not in CHOICES:
        raise ValueError(f'Unknown platform selection: {selection}')
    return list(PLATFORMS) if selection == 'all' else [selection]


def build_matrix(selection):
    return {'include': [dict(platform=name, runner=PLATFORMS[name][0])
                        for name in selected_platforms(selection)]}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=CHOICES)
    print(json.dumps(build_matrix(parser.parse_args().platform), separators=(',', ':')))
