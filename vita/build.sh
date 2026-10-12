#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
: "${VITASDK:?Set VITASDK to the VitaSDK directory (see vita/README.md)}"
cmake_cmd=${CMAKE:-cmake}
build_dir=${LUANTI_VITA_BUILD_DIR:-"$repo_dir/build/vita"}
"$cmake_cmd" -S "$repo_dir" -B "$build_dir" \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
"$cmake_cmd" --build "$build_dir" --parallel "${LUANTI_BUILD_JOBS:-4}"
python3 "$repo_dir/vita/validate_package.py" "$build_dir/src/luanti-vita.vpk"
