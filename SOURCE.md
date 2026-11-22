# Source and builds

This repository contains SariaUI's application source, tests, randomizer integration, packaging scripts, and platform build workflow. Upstream copyright notices and dependency licenses remain intact.

## Get the source

Clone the repository with its pinned dependencies:

```sh
git clone --recurse-submodules https://github.com/tgacquin/oot3d-sariaui.git SariaUI
cd SariaUI
```

For an existing checkout, run `git submodule update --init --recursive` after pulling changes. Submodules are pinned to the revisions recorded by Git. The adapted OoT3D Randomizer source is included under `externals/oot3d-randomizer/`.

GitHub's automatically generated source ZIP and tarball do not include submodule contents. Use a recursive clone or a complete source archive attached to a release.

## Build reference

The [SariaUI desktop release workflow](.github/workflows/oot3d-release.yml) specifies the toolchains and dependencies. It uses the shared [build script](tools/oot3d/release/build.sh) for three targets:

| Target | Build environment | Package |
| --- | --- | --- |
| `windows-x64` | MSYS2 UCRT64, GCC, Qt 6, SDL2, CMake, Ninja, Python | ZIP with `SariaUI.exe` and `oot3d-seed.exe` |
| `macos-arm64` | Apple Silicon, Xcode command-line tools, CMake, Ninja, Python | ZIP with `SariaUI.app` and its bundled randomizer helper |
| `linux-x86_64` | Linux toolchain container specified by the workflow | TAR.GZ with `SariaUI.AppImage` and its bundled randomizer helper |

Use the workflow's environment and dependency setup for the selected platform. From a clean, committed checkout with initialized submodules, the build command is:

```sh
bash tools/oot3d/release/build.sh <target>
```

The script builds in `build-release-<target>`, runs checks that do not require a game, and writes packages to `artifacts/`. Python is used by the build and packaging tools; players do not need it installed. No game dump is needed to build the application. Some additional developer checks require a separately acquired supported game dump and are not run by this workflow.

Player builds enable `ENABLE_OOT3D_PRESENTATION` and `ENABLE_OOT3D_PLAYER_MODE`, and disable the MM3D presentation and game-folder options. They expose player settings through F1. In developer builds (`ENABLE_OOT3D_PLAYER_MODE=OFF`), F1 edits the presentation and F2 edits the running profile. Press F3 to choose defaults for new player profiles. F3 saves your choices to `src/oot3d/defaults/player.ini`. Build again to embed them, and include that file when committing a release.

The shared version is stored in [resources/oot3d/release-version.txt](resources/oot3d/release-version.txt), initially `1.0.0`. Update and commit that file for each release. The workflow, artifact and package names, application version, and release metadata all use it. Versions such as `1.0.0-beta.1` are also supported. macOS and Windows numeric version fields use the three-part numeric portion; the full version is retained in the application and descriptive metadata.

The workflow is manually dispatched through **Actions → SariaUI desktop release candidates → Run workflow**. Choose **Platform to build**: `linux-x86_64` (the default), `windows-x64`, `macos-arm64`, or `all`. Only the selected platforms are scheduled; `all` builds the three platforms in parallel. Every selection includes matching source, checksums, and a release manifest listing the selected platforms. There is no separate version input. It does not create a tag or publish a GitHub Release. The inherited MM3D workflows are for DekUI, not SariaUI.

See [release preparation notes](tools/oot3d/release/README.txt) for deployment checks, platform limitations, and remaining release validation. macOS packages are ad-hoc signed rather than Apple-notarized; Windows packages are unsigned.

## Release source and notices

Each binary release must include complete corresponding source for the revision used to build it, SHA-256 checksums, and required dependency notices. From a clean Git checkout with all pinned submodules initialized, create the source archive with:

```sh
release_version=$(python3 tools/oot3d/release/release_version.py)
python3 tools/oot3d/release/preflight.py
python3 tools/mm3d/package_source.py "artifacts/SariaUI-$release_version-Source.tar.gz"
```

The source packager is shared with DekUI and still lives under `tools/mm3d/`. It archives committed source and pinned submodule contents, including the adapted randomizer, and can be inspected without Git. Uncommitted changes are not included. Keep game dumps, keys, saves, user configuration, and private working notes out of release source.

Binary packaging retains the main license and collects dependency notices under `licenses/`. `release.json` records the source commit, version, and platform. See [CREDITS.md](CREDITS.md) for project acknowledgements. Downloaded build dependencies are identified in the workflow and build scripts.
