import json
import os
from pathlib import Path
import tempfile
import subprocess
import shutil
import unittest
from unittest.mock import patch

from collect import collect, digest
from export_defaults import export, read
from package import copy_payload, inspect_tree, smoke
from preflight import check, private_path, version
from release_version import release_version
from platforms import build_matrix


class ReleaseChecks(unittest.TestCase):
    def test_platform_selection_only_schedules_requested_runners(self):
        expected = [
            {'platform': 'windows-x64', 'runner': 'windows-2022'},
            {'platform': 'macos-arm64', 'runner': 'macos-15'},
            {'platform': 'linux-x86_64', 'runner': 'ubuntu-24.04'},
        ]
        self.assertEqual(build_matrix('all'), {'include': expected})
        for entry in expected:
            with self.subTest(platform=entry['platform']):
                self.assertEqual(build_matrix(entry['platform']), {'include': [entry]})
        with self.assertRaises(ValueError):
            build_matrix('unknown-platform')

    def test_single_platform_candidate_requires_its_package_and_source(self):
        for platform, extension in (('windows-x64', 'zip'), ('macos-arm64', 'zip'),
                                    ('linux-x86_64', 'tar.gz')):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                version = release_version()
                commit = 'b' * 40
                package = root / f'SariaUI-{version}-{platform}.{extension}'
                package.write_bytes(b'package fixture')
                checksum = package.with_name(package.name + '.sha256')
                checksum.write_text(f'{digest(package)}  {package.name}\n')
                with self.assertRaisesRegex(ValueError, 'Source.tar.gz'):
                    collect(root, version, commit, platform)
                source = root / f'SariaUI-{version}-Source.tar.gz'
                source.write_bytes(b'source fixture')
                collect(root, version, commit, platform)
                manifest = json.loads((root / 'release-manifest.json').read_text())
                self.assertEqual(manifest['platforms'], [platform])
                self.assertEqual([item['file'] for item in manifest['artifacts']],
                                 [package.name, source.name])
                self.assertEqual(len((root / 'SHA256SUMS').read_text().splitlines()), 2)
                package.write_bytes(b'changed package')
                with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                    collect(root, version, commit, platform)
                package.unlink()
                with self.assertRaisesRegex(ValueError, 'Missing/empty'):
                    collect(root, version, commit, platform)

    @unittest.skipUnless(shutil.which('cmake'), 'CMake unavailable')
    def test_cmake_version_and_incremental_refresh(self):
        module = Path(__file__).resolve().parents[3] / 'CMakeModules/GenerateBuildInfo.cmake'
        # Exercise the module's real environment-dependent upstream version rules.
        # A manual GitHub branch build has no tag; an archive outside CI uses GIT-TAG.
        base_env = {key: value for key, value in os.environ.items() if key not in {
            'CI', 'GITHUB_ACTIONS', 'GITHUB_REF_TYPE', 'GITHUB_REF_NAME',
            'CITRA_USE_TAG_AS_VERSION'}}
        cases = (
            ('archive', {}, '9.9.9|9.9.9|'),
            ('github-branch', {'CI': 'true', 'GITHUB_ACTIONS': 'true',
                               'GITHUB_REF_TYPE': 'branch', 'GITHUB_REF_NAME': 'main'},
             '0|aaaaaaa|'),
            ('github-tag', {'CI': 'true', 'GITHUB_ACTIONS': 'true',
                            'GITHUB_REF_TYPE': 'tag', 'GITHUB_REF_NAME': '8.7.6'},
             '8.7.6|8.7.6|'),
        )
        for label, overrides, upstream_version in cases:
            env = {**base_env, **overrides}
            with self.subTest(environment=label), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                version_file = root / 'resources/oot3d/release-version.txt'
                version_file.parent.mkdir(parents=True)
                version_file.write_text('1.0.0\n')
                (root / 'GIT-COMMIT').write_text('a'*40+'\n')
                (root / 'GIT-TAG').write_text('9.9.9\n')
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.25)\nproject(VersionCheck NONE)\n'
                    f'include("{module.as_posix()}")\ngenerate_build_info()\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/version.txt" '
                    '"${BUILD_VERSION}|${BUILD_FULLNAME}|${SARIAUI_NUMERIC_VERSION}")\n')
                build = root / 'build'
                command = ['cmake', '-S', str(root), '-B', str(build), '-DENABLE_OOT3D_PRESENTATION=ON']
                subprocess.run(command, check=True, capture_output=True, text=True, env=env)
                self.assertEqual((build / 'version.txt').read_text(), '1.0.0|1.0.0|1.0.0')
                version_file.write_text('1.2.3-beta.2\n')
                subprocess.run(['cmake', '--build', str(build)], check=True, capture_output=True, text=True, env=env)
                self.assertEqual((build / 'version.txt').read_text(), '1.2.3-beta.2|1.2.3-beta.2|1.2.3')
                version_file.write_text('1.02.3\n')
                result = subprocess.run(command, capture_output=True, text=True, env=env)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Invalid SariaUI release version', result.stderr)
                # Non-OoT builds continue using their existing tag-based version.
                command[-1] = '-DENABLE_OOT3D_PRESENTATION=OFF'
                subprocess.run(command, check=True, capture_output=True, text=True, env=env)
                self.assertTrue((build / 'version.txt').read_text().startswith(upstream_version))

    def test_shared_version_and_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'release-version.txt'
            with patch('release_version.VERSION_FILE', path):
                for value in ('1.0.0', '1.2.3-beta.2', '2.0.0-rc.1'):
                    path.write_text(value+'\n')
                    self.assertEqual(release_version(), value)
                    self.assertEqual(release_version(value), value)
                    with self.assertRaisesRegex(ValueError, 'does not match'):
                        release_version('9.9.9')
                path.write_text('1.0.0\n2.0.0\n')
                with self.assertRaises(ValueError):
                    release_version()

    def test_application_version_must_match_package(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result = subprocess.CompletedProcess([], 0, stdout='SariaUI wrong-version [player]\n')
            with patch('package.subprocess.run', return_value=result):
                with self.assertRaisesRegex(ValueError, 'Application version'):
                    smoke('windows-x64', root, root)

    def test_developer_frontend_is_rejected_before_release(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result = subprocess.CompletedProcess([], 0, stdout='SariaUI test [developer]\n')
            with patch('package.subprocess.run', return_value=result):
                with self.assertRaisesRegex(ValueError, 'ENABLE_OOT3D_PLAYER_MODE'):
                    smoke('windows-x64', root, root)

    def test_linux_smoke_requires_bundled_offscreen_plugin(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            seed = root / 'squashfs-root/usr/bin/oot3d-seed'
            seed.parent.mkdir(parents=True)
            seed.write_bytes(b'helper fixture')
            with patch('package.subprocess.run') as run:
                with self.assertRaisesRegex(ValueError, 'libqoffscreen'):
                    smoke('linux-x86_64', root, root)
                self.assertEqual(run.call_count, 1)  # Only extraction, no app launch.

    def test_smoke_reports_application_stderr(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            error = subprocess.CalledProcessError(-6, ['AppRun', '--version'],
                output='', stderr='Could not find the Qt platform plugin offscreen')
            with patch('package.subprocess.run', side_effect=error):
                with self.assertRaisesRegex(ValueError, 'Qt platform plugin offscreen'):
                    smoke('windows-x64', root, root)

    def test_default_export_preserves_layouts_without_private_player_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            editor, runtime = root / 'editor.ini', root / 'runtime.ini'
            editor.write_text('[HUD]\nhearts\\x=-12\n[Interpolation]\ntarget_fps=120\n'
                'match_refresh_rate=true\n[Randomizer]\nseed=private-seed\n'
                '[Graphics]\nfxaa=true\nbloom=true\nactor_draw_distance=1\n'
                '[UserHUD]\nhearts\\x=20\n[Multiplayer]\nroom=private-room\n'
                'player_names\\scale=0.8\n')
            runtime.write_text('[Renderer]\nresolution_factor=5\nphysical_device=2\n'
                'resolution_factor\\default=false\n[Audio]\nenable_audio_stretching=true\n'
                'output_device=private-device\n[WebService]\ntoken=private-token\n'
                '[UI]\nrecentFiles=private-path\n')
            for name in ('oot3d-world-map-outline.json', 'oot3d-map-cutout.json'):
                asset = Path(__file__).parents[3] / 'src/oot3d/defaults' / name
                (root / name).write_bytes(asset.read_bytes())
            (root / 'private-capture.json').write_text('private-capture')
            export(editor, runtime, root / 'out')
            self.assertFalse((root / 'out/private-capture.json').exists())
            for name in ('oot3d-world-map-outline.json', 'oot3d-map-cutout.json'):
                self.assertEqual(json.loads((root / name).read_text()),
                                 json.loads((root / 'out' / name).read_text()))
            (root / 'oot3d-world-map-outline.json').unlink()
            with self.assertRaises(FileNotFoundError):
                export(editor, runtime, root / 'incomplete')
            presentation = read(root / 'out/presentation.ini')
            frontend = read(root / 'out/runtime.ini')
            self.assertEqual(presentation['HUD']['hearts\\x'], '-12')
            self.assertEqual(presentation['Multiplayer']['player_names\\scale'], '0.8')
            self.assertEqual(presentation['Interpolation']['target_fps'], '60')
            self.assertEqual(presentation['Graphics']['fxaa'], 'true')
            self.assertEqual(presentation['Graphics']['bloom'], 'true')
            self.assertEqual(presentation['Graphics']['actor_draw_distance'], '3.75')
            self.assertEqual(presentation['Interpolation']['cpu_vertices'], 'true')
            self.assertEqual(frontend['Renderer']['use_hw_shader'], 'false')
            self.assertEqual(presentation['Interpolation']['match_refresh_rate'], 'false')
            self.assertEqual(frontend['Renderer']['resolution_factor'], '3')
            self.assertEqual(frontend['Audio']['enable_audio_stretching'], 'false')
            for file in (root / 'out').iterdir():
                self.assertNotIn('private-', file.read_text())
            self.assertNotIn('UserHUD', presentation)
            self.assertNotIn('physical_device', frontend['Renderer'])

    def test_version_cannot_escape_artifact_name(self):
        self.assertEqual(version('0.1.0-preview.1'), '0.1.0-preview.1')
        for value in ('../release', 'v1.0.0', '1.0.0\nextra', '1.0.0;echo bad', '1.0.0/other'):
            with self.assertRaises(ValueError):
                version(value)

    def test_private_inputs_rejected_but_runtime_source_allowed(self):
        for name in ('private/notes.txt', 'game/test.cci', 'save.SAV', 'oot3d-editor.ini',
                     'tools/code-decompressed.bin', 'aes_keys.txt', 'HANDOFF.md',
                     'notes/camera-handoff-20260101.txt', 'DEVELOPMENT.md',
                     'analysis/notes.txt', 'local/captures/actors.json'):
            self.assertTrue(private_path(name), name)
        for name in ('src/oot3d/randomizer/assets/guest.bin', 'externals/oot3d-randomizer/LICENSE.md'):
            self.assertFalse(private_path(name), name)

    def test_dirty_checkout_not_silently_packaged(self):
        with patch('preflight.git', return_value='?? src/oot3d/example.h\n'):
            with self.assertRaisesRegex(ValueError, 'clean and committed'):
                    check()

    def test_windows_requires_helper_and_only_copies_runtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, stage = Path(tmp) / 'bundle', Path(tmp) / 'stage'
            stage.mkdir()
            for name in ('SariaUI.exe', 'qt.conf', 'Qt6Core.dll', 'plugins/platforms/qwindows.dll',
                         'azahar.exe', 'user/config/oot3d-editor.ini', 'game/private.cci', 'notes.txt'):
                path = bundle / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'fixture')
            with self.assertRaisesRegex(ValueError, 'oot3d-seed.exe'):
                copy_payload('windows-x64', bundle, stage)
            (bundle / 'oot3d-seed.exe').write_bytes(b'fixture')
            copy_payload('windows-x64', bundle, stage)
            self.assertEqual({p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()},
                {'SariaUI.exe', 'oot3d-seed.exe', 'qt.conf', 'Qt6Core.dll', 'plugins/platforms/qwindows.dll'})

    def test_linux_requires_sariaui_appimage(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, stage = Path(tmp) / 'bundle', Path(tmp) / 'stage'
            bundle.mkdir()
            stage.mkdir()
            (bundle / 'azahar.AppImage').write_bytes(b'old frontend')
            with self.assertRaisesRegex(ValueError, 'SariaUI.AppImage'):
                copy_payload('linux-x86_64', bundle, stage)
            (bundle / 'SariaUI.AppImage').write_bytes(b'player frontend')
            copy_payload('linux-x86_64', bundle, stage)
            self.assertEqual([p.name for p in stage.iterdir()], ['SariaUI.AppImage'])
            self.assertEqual((stage / 'SariaUI.AppImage').read_bytes(), b'player frontend')

    def test_macos_requires_helper(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, stage = Path(tmp) / 'bundle', Path(tmp) / 'stage'
            stage.mkdir()
            main = bundle / 'SariaUI.app/Contents/MacOS/SariaUI'
            main.parent.mkdir(parents=True)
            main.write_bytes(b'fixture')
            with self.assertRaisesRegex(ValueError, 'oot3d-seed'):
                copy_payload('macos-arm64', bundle, stage)

    def test_external_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'app'
            root.mkdir()
            external = Path(tmp) / 'private.txt'
            external.write_text('fixture')
            try:
                (root / 'link').symlink_to(external)
            except OSError:
                self.skipTest('Symlinks unavailable')
            with self.assertRaisesRegex(ValueError, 'escapes'):
                inspect_tree(root)

    def test_macos_requires_headless_loader_plugin_before_smoke(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, stage = Path(tmp) / 'bundle', Path(tmp) / 'stage'
            stage.mkdir()
            app = bundle / 'SariaUI.app'
            for name in ('Contents/MacOS/SariaUI', 'Contents/MacOS/oot3d-seed',
                         'Contents/PlugIns/platforms/libqcocoa.dylib'):
                path = app / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'fixture')
            with self.assertRaisesRegex(ValueError, 'libqoffscreen'):
                copy_payload('macos-arm64', bundle, stage)
            plugin = Path('Contents/PlugIns/platforms/libqoffscreen.dylib')
            (app / plugin).write_bytes(b'fixture')
            copy_payload('macos-arm64', bundle, stage)
            self.assertTrue((stage / 'SariaUI.app' / plugin).is_file())

    def test_candidate_requires_all_platforms_and_valid_checksums(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            commit = 'a' * 40
            selected = release_version()
            for suffix in ('windows-x64.zip', 'macos-arm64.zip', 'Source.tar.gz'):
                path = root / f'SariaUI-{selected}-{suffix}'
                path.write_bytes(b'fixture')
                path.with_name(path.name + '.sha256').write_text(f'{digest(path)}  {path.name}\n')
            with self.assertRaisesRegex(ValueError, 'linux-x86_64'):
                collect(root, selected, commit)
            self.assertFalse((root / 'release-manifest.json').exists())
            linux = root / f'SariaUI-{selected}-linux-x86_64.tar.gz'
            linux.write_bytes(b'fixture')
            sidecar = linux.with_name(linux.name + '.sha256')
            sidecar.write_text('wrong checksum')
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                collect(root, selected, commit)
            sidecar.write_text(f'{digest(linux)}  {linux.name}\n')
            collect(root, selected, commit)
            result = json.loads((root / 'release-manifest.json').read_text())
            self.assertEqual(result['commit'], commit)
            self.assertEqual(result['version'], selected)
            self.assertEqual(len(result['artifacts']), 4)
            self.assertEqual(result['status'], 'unpublished-candidate')


if __name__ == '__main__':
    unittest.main()
