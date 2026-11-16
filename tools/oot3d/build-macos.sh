#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cmake -S "$source_dir" -B "$source_dir/build-macos" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_OOT3D_PRESENTATION=ON -DENABLE_MM3D_PRESENTATION=OFF \
    -DENABLE_MM3D_GAME_FOLDER=OFF -DENABLE_QT=ON -DENABLE_VULKAN=ON \
    -DENABLE_LTO=OFF -DENABLE_TESTS=OFF -DENABLE_LIBRETRO=OFF \
    -DENABLE_SDL2_FRONTEND=OFF -DENABLE_ROOM_STANDALONE=OFF \
    -DENABLE_DISCORD_RPC=OFF -DENABLE_QT_UPDATE_CHECKER=OFF
cmake --build "$source_dir/build-macos" --target azahar --parallel 6
