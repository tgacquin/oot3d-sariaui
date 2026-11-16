#!/usr/bin/env python3
"""Package CI output only; test deployed loaders using isolated temporary user data."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from preflight import ROOT, private_path
from collect import digest
from release_version import release_version as configured_version

PLATFORMS = ('windows-x64', 'macos-arm64', 'linux-x86_64')


def require(path):
    if not path.is_file():
        raise ValueError(f'Missing deployed file: {path}')


def inspect_tree(root):
    for path in root.rglob('*'):
        relative = path.relative_to(root)
        if private_path(relative.as_posix()):
            raise ValueError(f'Private/game file in bundle: {relative}')
        if path.is_symlink() and not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f'Bundle symlink escapes its directory: {relative}')


def copy_payload(platform, bundle, destination):
    if platform == 'windows-x64':
        for name in ('SariaUI.exe', 'oot3d-seed.exe', 'qt.conf', 'plugins/platforms/qwindows.dll'):
            require(bundle / name)
        for path in bundle.rglob('*'):
            rel = path.relative_to(bundle)
            allowed = (len(rel.parts) == 1 and (path.suffix.lower() == '.dll' or
                       path.name in {'SariaUI.exe', 'oot3d-seed.exe', 'qt.conf'}))
            allowed |= rel.parts[0] == 'plugins' and path.suffix.lower() == '.dll'
            if path.is_file() and allowed:
                target = destination / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    elif platform == 'macos-arm64':
        app = bundle / 'SariaUI.app'
        for name in ('Contents/MacOS/SariaUI', 'Contents/MacOS/oot3d-seed',
                     'Contents/PlugIns/platforms/libqcocoa.dylib',
                     'Contents/PlugIns/platforms/libqoffscreen.dylib'):
            require(app / name)
        inspect_tree(app)
        shutil.copytree(app, destination / 'SariaUI.app', symlinks=True)
    elif platform == 'linux-x86_64':
        require(bundle / 'SariaUI.AppImage')
        shutil.copy2(bundle / 'SariaUI.AppImage', destination / 'SariaUI.AppImage')
    else:
        raise ValueError(f'Unsupported platform: {platform}')
    inspect_tree(destination)


def verify_macos(app):
    magics = {b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'}
    for path in app.rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open('rb') as stream:
            if stream.read(4) not in magics:
                continue
        arches = subprocess.check_output(['lipo', '-archs', str(path)], text=True).split()
        if 'arm64' not in arches:
            raise ValueError(f'Missing arm64 code: {path}')
        identities = subprocess.check_output(['otool', '-arch', 'arm64', '-D', str(path)], text=True).splitlines()[1:]
        for line in subprocess.check_output(['otool', '-arch', 'arm64', '-L', str(path)], text=True).splitlines()[1:]:
            dep = line.strip().split(' (', 1)[0]
            if dep not in identities and not dep.startswith(('@rpath/', '@loader_path/',
                    '@executable_path/', '/usr/lib/', '/System/Library/')):
                raise ValueError(f'Runner-only macOS dependency: {dep}')
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)


def smoke(platform, stage, work, expected_version=None):
    env = os.environ.copy()
    for key in ('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QTDIR',
                'DYLD_LIBRARY_PATH', 'DYLD_FRAMEWORK_PATH', 'LD_LIBRARY_PATH'):
        env.pop(key, None)
    env.update(OOT3D_USER_PATH=str(work / 'user'), QT_QPA_PLATFORM='offscreen')
    if platform == 'macos-arm64':
        app = stage / 'SariaUI.app'
        verify_macos(app)
        program = app / 'Contents/MacOS/SariaUI'
        seed = program.with_name('oot3d-seed')
    elif platform == 'windows-x64':
        program, seed = stage / 'SariaUI.exe', stage / 'oot3d-seed.exe'
        # Do not let the MSYS toolchain's DLLs conceal a missing bundled DLL.
        system_root = Path(env.get('SystemRoot', r'C:\Windows'))
        env['PATH'] = os.pathsep.join(map(str, (stage, system_root / 'System32', system_root)))
        env['QT_PLUGIN_PATH'] = str(stage / 'plugins')
    else:
        subprocess.run([str(stage / 'SariaUI.AppImage'), '--appimage-extract'],
                       cwd=work, check=True, stdout=subprocess.DEVNULL, timeout=120)
        appdir = work / 'squashfs-root'
        program, seed = appdir / 'AppRun', appdir / 'usr/bin/oot3d-seed'
        require(seed)
        env['LD_LIBRARY_PATH'] = str(appdir / 'usr/lib')
        env['QT_PLUGIN_PATH'] = str(appdir / 'usr/plugins')
    version_result = subprocess.run([str(program), '--version'], cwd=work, env=env,
                                    check=True, timeout=60, capture_output=True, text=True)
    if '[player]' not in version_result.stdout:
        raise ValueError('Release requires ENABLE_OOT3D_PLAYER_MODE; refusing developer frontend')
    expected_version = configured_version(expected_version)
    if version_result.stdout.strip() != f'SariaUI {expected_version} [player]':
        raise ValueError('Application version does not match the package version')
    catalog = work / 'catalog.json'
    subprocess.run([str(seed), '--catalog', str(catalog)], cwd=work, env=env, check=True, timeout=120)
    if not json.loads(catalog.read_text()):
        raise ValueError('Deployed randomizer helper returned an empty catalog')


def notices(root, build, bundle):
    shutil.copy2(ROOT / 'license.txt', root / 'LICENSE.txt')
    for source, prefix in ((ROOT / 'externals', 'source'),
                           (build / 'externals', 'build'), (bundle / 'licenses', 'toolchain')):
        if not source.is_dir():
            continue
        for path in source.rglob('*'):
            if path.is_file() and not path.is_symlink() and (prefix == 'toolchain' or
                    path.name.lower().startswith(('license', 'copying', 'copyright'))):
                if path.stat().st_size > 2_000_000:
                    continue
                target = root / 'licenses' / prefix / path.relative_to(source)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)


def package(platform, build, output, release_version):
    configured_version(release_version)
    build, output = build.resolve(), output.resolve()
    suffix = '.tar.gz' if platform == 'linux-x86_64' else '.zip'
    archive = output / f'SariaUI-{release_version}-{platform}{suffix}'
    if archive.exists():
        raise ValueError(f'Refusing to overwrite {archive}')
    with tempfile.TemporaryDirectory(prefix='oot3d-release-') as tmp:
        work = Path(tmp)
        stage = work / 'SariaUI'
        stage.mkdir()
        copy_payload(platform, build / 'bundle', stage)
        smoke(platform, stage, work, release_version)
        notices(stage, build, build / 'bundle')
        shutil.copy2(ROOT / 'tools/oot3d/release/INSTALL.txt', stage / 'INSTALL.txt')
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        (stage / 'release.json').write_text(json.dumps(dict(version=release_version,
            commit=revision, platform=platform, signing='ad-hoc' if platform == 'macos-arm64' else 'unsigned'), indent=2) + '\n')
        output.mkdir(parents=True, exist_ok=True)
        if platform == 'macos-arm64':
            subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent',
                            str(stage), str(archive)], check=True)
        elif platform == 'linux-x86_64':
            with tarfile.open(archive, 'w:gz') as tar:
                tar.add(stage, arcname='SariaUI')
        else:
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as zip_out:
                for path in sorted(stage.rglob('*')):
                    if path.is_file():
                        zip_out.write(path, path.relative_to(work))
    checksum = digest(archive)
    archive.with_name(archive.name + '.sha256').write_text(f'{checksum}  {archive.name}\n')
    print(archive)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=PLATFORMS)
    parser.add_argument('build', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('version')
    args = parser.parse_args()
    package(args.platform, args.build, args.output, args.version)
