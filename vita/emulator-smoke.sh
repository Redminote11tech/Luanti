#!/usr/bin/env bash
# Uses the optional local Vita3K runtime prepared during cloud development.
# This is a startup smoke test, not a gameplay/performance acceptance test.
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
runtime=${LUANTI_EMULATOR_RUNTIME:-/workspace/emulator-runtime}
emulator=${VITA3K:-/workspace/vita3k/Vita3K}
vpk=${LUANTI_VITA_VPK:-"$repo_dir/build/vita/src/luanti-vita.vpk"}
python3 "$repo_dir/vita/validate_package.py" "$vpk"
export XDG_CONFIG_HOME="$runtime/user/config"
export XDG_DATA_HOME="$runtime/user/data"
export XDG_CACHE_HOME="$runtime/user/cache"
export XDG_RUNTIME_DIR="$runtime/user/run"
export QT_QPA_PLATFORM=offscreen
export QT_PLUGIN_PATH="$runtime/root/usr/lib/x86_64-linux-gnu/qt6/plugins"
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
export LIBGL_ALWAYS_SOFTWARE=1
export LIBGL_DRIVERS_PATH="$runtime/root/usr/lib/x86_64-linux-gnu/dri"
python3 - "$vpk" "$XDG_DATA_HOME/Vita3K/Vita3K/ux0/app/LUANTI001" <<'PY'
from pathlib import Path
import sys
import zipfile
target = Path(sys.argv[2])
target.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(sys.argv[1]) as vpk:
    vpk.extractall(target)
PY
cd -- "$(dirname -- "$emulator")"
exec timeout "${LUANTI_EMULATOR_TIMEOUT:-30}" \
  "$runtime/root/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2" \
  --library-path "$runtime/root/usr/lib/x86_64-linux-gnu:$runtime/root/lib/x86_64-linux-gnu" \
  "$emulator" --frontend sdl -r LUANTI001 -B OpenGL -l 2 "$@"
