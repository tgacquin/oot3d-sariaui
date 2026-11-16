# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the shipped preset recipes with the real generator, using isolated files."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys

exe, checks = (Path(p).resolve() for p in sys.argv[1:3])
out = Path(sys.argv[3]).resolve()
out.mkdir(parents=True, exist_ok=True)
subprocess.run([str(checks), str(out)], check=True)


def run(case):
    name, mode = case
    work = out / f'{name}-{mode}'
    work.mkdir(exist_ok=True)
    with (work / 'generation.log').open('w') as log:
        result = subprocess.run([str(exe), str(out / f'{name}.json'), mode, str(work / 'seed.json')],
                                cwd=work, stdout=log, stderr=subprocess.PIPE, timeout=180)
    assert result.returncode == 0, (case, result.stderr.decode(errors='replace'))
    package = json.loads((work / 'seed.json').read_text())
    expected = json.loads((out / f'{name}.json').read_text())
    assert package['format'] == 2 and package['mq'] == (mode == 'mq')
    assert package['fields']['rItemOverrides'] and package['hash']
    actual = package['config']
    for key, value in expected.items():
        if key != 'advanced':
            assert actual[key] == value, (case, key, actual[key], value)
    # No accidental no-logic mode or high-count hunt inherited from the previous setup.
    assert actual.get('advanced', {}).get('Logic', 0) == 0
    assert actual.get('advanced', {}).get('TriforceHunt', 0) == 0
    for key in ('BridgeRewardCount', 'LACSRewardCount', 'ShuffleDungeonEntrances', 'ShuffleBossEntrances'):
        if key in expected.get('advanced', {}):
            assert actual['advanced'][key] == expected['advanced'][key], (case, key)
    print(name, mode, 'passed', flush=True)


cases = [(name, mode) for name in ('Defaults', 'Beginner', 'Standard', 'Advanced', 'Saved Standard')
         for mode in ('original', 'mq')]
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(run, cases))
print('All preset recipes generated successfully in Original and Master Quest.', flush=True)
