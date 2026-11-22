#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../../.."
platform="${1:?platform required}"
release_version=$(python3 tools/oot3d/release/release_version.py "${@:2}")
python3 tools/oot3d/release/preflight.py "$release_version"
build_dir="build-release-$platform"
flags=()
case "$platform" in
  macos-arm64)
    test "$(uname -m)" = arm64
    flags+=(-DCMAKE_OSX_ARCHITECTURES=arm64 -DUSE_SYSTEM_QT=OFF
            -DUSE_SYSTEM_SDL2=OFF -DUSE_SYSTEM_MOLTENVK=OFF)
    ;;
  windows-x64)
    flags+=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
            -DUSE_SYSTEM_QT=ON -DUSE_SYSTEM_SDL2=ON)
    ;;
  linux-x86_64)
    test "$(uname -m)" = x86_64
    # Use the shared CMake compiler launcher below instead of SDL's implicit
    # ccache integration, which may select inaccessible default directories.
    flags+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
            -DCMAKE_LINKER=/etc/bin/ld.lld -DSDL_CCACHE=OFF)
    export EXTRA_QT_PLUGINS=waylandcompositor
    # The packaged-app smoke test initializes Qt without a display server.
    export EXTRA_PLATFORM_PLUGINS='libqwayland-egl.so;libqwayland-generic.so;libqoffscreen.so'
    ;;
  *) echo "Unsupported platform: $platform" >&2; exit 2 ;;
esac
if command -v ccache >/dev/null 2>&1; then
  # This path is writable by the runner, including its mapped UID in Docker.
  export CCACHE_DIR="$PWD/.ccache"
  export CCACHE_BASEDIR="$PWD"
  export CCACHE_TEMPDIR="$CCACHE_DIR/tmp"
  export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-2G}"
  export CCACHE_COMPILERCHECK="${CCACHE_COMPILERCHECK:-content}"
  mkdir -p "$CCACHE_TEMPDIR"
  ccache --zero-stats
  trap 'ccache --show-stats || true' EXIT
  flags+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
else
  echo 'ccache unavailable; compiling without a compiler cache.'
fi
checks=(oot3d_atomic_shared_ptr_check oot3d_interpolation_rate_check oot3d_file_copy_check oot3d_uds_rejoin_check
        oot3d_release_defaults_check oot3d_player_controls_check)
cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_OOT3D_PRESENTATION=ON \
  -DENABLE_OOT3D_PLAYER_MODE=ON \
  -DENABLE_MM3D_PRESENTATION=OFF -DENABLE_MM3D_GAME_FOLDER=OFF \
  -DENABLE_QT=ON -DENABLE_VULKAN=ON -DENABLE_LTO=OFF \
  -DENABLE_LIBRETRO=OFF -DENABLE_ROOM_STANDALONE=OFF \
  -DENABLE_SDL2_FRONTEND=OFF -DENABLE_TESTS=OFF \
  -DENABLE_DISCORD_RPC=OFF -DENABLE_QT_UPDATE_CHECKER=OFF \
  -DENABLE_NATIVE_OPTIMIZATION=OFF "${flags[@]}"
# Check the standalone presentation library before compiling the large dependencies.
cmake --build "$build_dir" --target oot3d_hd_font oot3d_atomic_shared_ptr_check --parallel 3
cmake --build "$build_dir" --target citra_meta "${checks[@]}" --parallel 3
for check in "${checks[@]}"; do
  suffix=''; if [[ "$platform" == windows-x64 ]]; then suffix=.exe; fi
  "$build_dir/bin/Release/$check$suffix"
done
if [[ "$platform" == linux-x86_64 ]]; then
  cmake --build "$build_dir" --target oot3d_randomizer_check --parallel 3
  "$build_dir/bin/Release/oot3d_randomizer_check" --check-hook-routing
  echo 'Disk usage before AppImage bundling:'
  df -h . /tmp
  du -sh "$build_dir" .ccache 2>/dev/null || true
fi
cmake --build "$build_dir" --target bundle --parallel 3
if [[ "$platform" == windows-x64 ]]; then
  mkdir -p "$build_dir/bundle/licenses"
  cp -R /ucrt64/share/licenses/. "$build_dir/bundle/licenses/"
fi
python3 tools/oot3d/release/package.py "$platform" "$build_dir" artifacts "$release_version"
