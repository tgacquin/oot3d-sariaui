Desktop release preparation
===========================

The release workflow builds candidate packages for testing before publication.
Hosted platform builds and clean-machine validation are required before a
binary release; local checks alone do not establish release readiness.

Player executables are SariaUI.exe (Windows), SariaUI.app (macOS, with an
internal SariaUI executable), and SariaUI.AppImage (Linux). Packages use
SariaUI-{version}-{platform}.zip/.tar.gz and extract into a SariaUI folder;
the source archive is SariaUI-{version}-Source.tar.gz. CMake target names and
developer output paths retain their upstream names. Source repository:
https://github.com/tgacquin/oot3d-sariaui

Prepared targets
----------------
  Windows x64: UCRT64 / GCC, deployed Qt and SDL, ZIP.
  macOS Apple Silicon: macos-15 arm64, Qt/MoltenVK app bundle, ZIP.
  Linux x86_64: existing Azahar Linux toolchain container, AppImage in tar.gz.
Intel Mac and Linux ARM packages are not covered by this initial matrix.

The oot3d-release.yml workflow has workflow_dispatch ONLY. It has read-only
repository permissions, creates downloadable candidate artifacts, and has no
publishing job or tag trigger. The three inherited MM3D workflows were also
made manual-only to prevent the first OoT repository push from running them.

Preparing a candidate
---------------------
1. Review and commit the intended source changes. Keep game dumps, saves,
   captures, local configuration and private working notes out of Git.
   Preflight requires a clean checkout and submodules at their recorded SHAs.
2. Set resources/oot3d/release-version.txt to the intended version (initially
   1.0.0) and commit it with the release changes.
3. Push the reviewed source. A push does not launch the build workflows.
4. Enable Actions if needed. Standard GITHUB_TOKEN read access is sufficient
   for artifact builds; no signing credentials or personal access token needed.
5. Select "SariaUI desktop release candidates" and run it when a build is
   wanted. Choose linux-x86_64 (default), windows-x64, macos-arm64, or all in
   the platform dropdown; no version input is needed.

Version naming
--------------
resources/oot3d/release-version.txt is shared by CMake and
tools/oot3d/release/release_version.py. It controls application versions, CI
artifact names, archive filenames, release.json and release-manifest.json.
Prereleases such as1.0.0-beta.1 retain their full label; numeric OS metadata
uses1.0.0. The developer --version output also identifies its developer mode.
Any explicit version arguments to build/preflight/package/collect must
match the shared file. Packaging rejects an app with a different version.

What the workflow does
----------------------
The source job verifies inputs, tests packaging rules with synthetic files,
and archives the exact commit plus pinned submodule contents. Platform jobs
build in separate build-release-* directories with OoT enabled and MM disabled.
They run selected checks that require no game data, bundle application
dependencies, and include oot3d-seed alongside the executable on all platforms.

Packaging validates required plugins and helper placement. Loader smoke checks
run --version and the seed helper's --catalog using temporary user directories,
without opening a game. macOS checks architecture, dependency paths and ad-hoc
signatures. Linux checks the helper inside the extracted AppImage. Windows
copies an allowlist of runtime files. Licenses and commit/version metadata are
included; no local user configuration or game dump is packaged.

Only the selected platforms receive build jobs. The final job requires each
selected package plus source, verifies per-package checksums, and emits
SHA256SUMS and release-manifest.json with the selected platforms recorded.
The all selection still requires all three packages. Candidate artifact names
end with the selection (for example, release-candidate-linux-x86_64).
Failed selected jobs prevent a complete candidate. Logs are uploaded
separately, including the resolved Linux container digest.

Limits / remaining release work
------------------------------
These workflows and packaging changes have not been validated by real Windows,
macOS or Linux release builds. The first manual run may expose compiler,
deployment or toolchain issues. Local syntax/fixture checks are not a substitute
for those builds or testing the resulting packages on clean machines.

The Linux image currently defaults to the inherited upstream :latest tag.
Set the OOT3D_LINUX_BUILD_IMAGE repository variable to a tested immutable
image@sha256:... before a public release. Qt/deployment downloads and MSYS2
packages also follow the existing tooling; bit-for-bit reproducibility is not
claimed. Record successful toolchain versions and pin them after first tests.

macOS is ad-hoc signed, not Developer ID signed/notarized. Windows executables
are unsigned. Public-release signing/notarization and final branding remain
separate work; no credentials are embedded here. Existing application/editor
defaults are compiled in; private editor settings are deliberately not exported.

Review the source/package contents and notices, test ordinary and randomized
files, multiplayer, audio/input, and all three clean-machine packages before
publishing. Publication is intentionally not automated yet. This preparatory
workflow never creates a public release, even when all builds pass.

Local checks that do not build or launch the game
-----------------------------------------------
  bash -n tools/oot3d/release/build.sh
  python3 -m unittest discover -s tools/oot3d/release -p 'test_*.py'

Do not run build.sh or dispatch the workflow merely to validate syntax.
