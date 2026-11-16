#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$(cd "$source_dir/.." && pwd)"
app="$source_dir/build-macos/bin/Release/azahar.app/Contents/MacOS/azahar"
if [[ ! -x "$app" ]]; then
    echo "Build the app first: $source_dir/tools/oot3d/build-macos.sh" >&2
    exit 1
fi
if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [path-to-game.cci]" >&2
    exit 1
fi
if [[ $# -eq 1 ]]; then
    rom="$1"
else
    shopt -s nullglob
    roms=("$project_dir"/*.cci "$project_dir/private"/*.cci "$project_dir/private/game"/*.cci)
    if [[ ${#roms[@]} -ne 1 ]]; then
        echo "Expected one .cci in the project or private folder; pass the game path explicitly." >&2
        exit 1
    fi
    rom="${roms[0]}"
fi
if [[ ! -f "$rom" ]]; then
    echo "Game not found: $rom" >&2
    exit 1
fi
rom="$(cd "$(dirname "$rom")" && pwd)/$(basename "$rom")"
export OOT3D_USER_PATH="$project_dir/private/user"
mkdir -p "$OOT3D_USER_PATH"
exec "$app" "$rom"
