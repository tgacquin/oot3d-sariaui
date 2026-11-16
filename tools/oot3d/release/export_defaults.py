#!/usr/bin/env python3
"""Export reviewed, portable presentation defaults; never copy a user directory."""
import argparse
import configparser
import json
import math
from pathlib import Path

PRESENTATION_GROUPS = {
    'Audio', 'BossChallenge', 'Camera', 'Controller', 'DeathMenu', 'Difficulty',
    'FileSelect', '%General', 'Graphics', 'HDNumbers', 'HUD', 'Interpolation',
    'Ocarina', 'PausePrompts', 'PauseScreen', 'QOL', 'Restorations', 'SaveMenu',
    'Textboxes', 'TitleScreen',
}
PRESENTATION_ASSETS = {
    'oot3d-world-map-outline.json': {'version', 'feather', 'rounding', 'points'},
    'oot3d-map-cutout.json': {'version', 'left', 'top', 'right', 'bottom', 'points'},
}
RUNTIME_GROUPS = {'Core', 'Renderer', 'Layout', 'Utility'}
AUDIO_KEYS = {'audio_emulation', 'enable_audio_stretching', 'enable_realtime_audio',
              'simulate_headphones_plugged', 'volume'}


def read(path):
    settings = configparser.ConfigParser(interpolation=None)
    settings.optionxform = str
    with path.open() as stream:
        settings.read_file(stream)
    return settings


def export(editor, runtime, output):
    # Explicit asset allowlist: do not export captures, seeds, saves or paths.
    assets = {}
    for name, keys in PRESENTATION_ASSETS.items():
        value = json.loads((editor.parent / name).read_text())
        if set(value) != keys or value['version'] != 1:
            raise ValueError(f'Invalid presentation asset: {name}')
        points = value['points']
        if not isinstance(points, list) or not 3 <= len(points) <= 128:
            raise ValueError(f'Invalid presentation points: {name}')
        numbers = [v for k, v in value.items() if k != 'points']
        for point in points:
            if not isinstance(point, list) or len(point) != 2:
                raise ValueError(f'Invalid presentation point: {name}')
            numbers.extend(point)
        if any(type(v) not in (int, float) or not math.isfinite(v) for v in numbers):
            raise ValueError(f'Invalid presentation number: {name}')
        assets[name] = value
    authored, emulator = read(editor), read(runtime)
    presentation, frontend = read(editor), read(runtime)
    for group in list(presentation.sections()):
        if group not in PRESENTATION_GROUPS and group != 'Multiplayer':
            presentation.remove_section(group)
    if 'Multiplayer' in presentation:
        for key in list(presentation['Multiplayer']):
            if not key.startswith('player_names\\'):
                presentation.remove_option('Multiplayer', key)
    # Player defaults are deliberate release choices, independent of the
    # author's monitor, last-used resolution, randomizer seed or F2 overrides.
    if ('PausePrompts' in presentation and
            presentation['PausePrompts'].get('map_move\\x') == '301' and
            presentation['PausePrompts'].get('map_return\\x') == '69.5'):
        presentation['PausePrompts']['map_return\\x'] = '69'
    presentation['Interpolation']['cpu_vertices'] = 'true'
    if 'Graphics' not in presentation:
        presentation['Graphics'] = {}
    presentation['Graphics'].setdefault('disable_fog', 'false')
    presentation['Interpolation']['enabled'] = 'true'
    presentation['Interpolation']['target_fps'] = '60'
    presentation['Interpolation']['match_refresh_rate'] = 'false'
    for group in list(frontend.sections()):
        if group not in RUNTIME_GROUPS and group != 'Audio':
            frontend.remove_section(group)
            continue
        for key in list(frontend[group]):
            if ('\\' in key or key == 'physical_device' or
                    (group == 'Audio' and key not in AUDIO_KEYS)):
                frontend.remove_option(group, key)
    frontend['Renderer']['resolution_factor'] = '3'
    frontend['Renderer']['use_hw_shader'] = 'false'
    frontend['Audio']['enable_audio_stretching'] = 'false'
    output.mkdir(parents=True, exist_ok=True)
    for name, settings in [('presentation.ini', presentation), ('runtime.ini', frontend)]:
        with (output / name).open('w') as stream:
            stream.write('; SariaUI authored release defaults. No user identity, paths or saves.\n')
            settings.write(stream, space_around_delimiters=False)
    for name, value in assets.items():
        (output / name).write_text(json.dumps(value, indent=4) + '\n')
    return authored, emulator, presentation, frontend


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('editor', type=Path)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    export(args.editor, args.runtime, args.output)
