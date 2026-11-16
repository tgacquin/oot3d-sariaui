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
    # SDL otherwise auto-enables the image's ccache, whose default directories
    # may be inaccessible to the mapped runner UID. This job does not persist it.
    flags+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
            -DCMAKE_LINKER=/etc/bin/ld.lld -DSDL_CCACHE=OFF)
    export EXTRA_QT_PLUGINS=waylandcompositor
    export EXTRA_PLATFORM_PLUGINS='libqwayland-egl.so;libqwayland-generic.so'
    ;;
  *) echo "Unsupported platform: $platform" >&2; exit 2 ;;
esac
checks=(oot3d_interpolation_rate_check oot3d_file_copy_check oot3d_uds_rejoin_check
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
cmake --build "$build_dir" --target citra_meta "${checks[@]}" --parallel 3
for check in "${checks[@]}"; do
  suffix=''; if [[ "$platform" == windows-x64 ]]; then suffix=.exe; fi
  "$build_dir/bin/Release/$check$suffix"
done
cmake --build "$build_dir" --target bundle --parallel 3
if [[ "$platform" == windows-x64 ]]; then
  mkdir -p "$build_dir/bundle/licenses"
  cp -R /ucrt64/share/licenses/. "$build_dir/bundle/licenses/"
fi
python3 tools/oot3d/release/package.py "$platform" "$build_dir" artifacts "$release_version"
