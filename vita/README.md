# Luanti Vita port

This is a **development build**, based on official Luanti 5.17.0
(`c0e6812b1a4260bb25a1f606f70f55f4962bb97d`). The original destination
repository was empty. The source and development VPK are maintained in this
repository; the downloadable package is in `releases/luanti-vita-dev.vpk`.

The client cross-compiles to a real ARM executable, SELF and VPK. Package
validation and 13 SHA-256 byte-order vectors pass. Vita3K loaded the VPK and
reached platform initialization and renderer startup. Its missing Sony shader
compiler prevented graphics validation; a subsequent crash was observed.
The port now checks for the compiler before initializing the renderer.
The packaged engine's `--version` path also ran in Vita3K and exited with code 0.
The final missing-compiler test exits with code 1 and writes the specific
dependency error to `startup.log`, without entering the graphics driver.
**Playing on a server, controls, sound, suspend/resume and frame rate have
not yet been verified on hardware. Do not treat this as a finished release.**

## Install on Vita

1. Use a Vita with homebrew support and VitaShell.
2. Install the shader compiler from your own Vita firmware using
   [ShaRKF00D](https://github.com/OsirizX/ShaRKF00D), if it is not installed.
   vitaGL expects `ur0:data/libshacccg.suprx`; it also supports
   `ur0:data/external/libshacccg.suprx`. This device dependency is not bundled.
3. Copy `build/vita/src/luanti-vita.vpk` to the Vita and install it with VitaShell.
   The homebrew title ID is `LUANTI001`, with the upstream Luanti icon and
   a Vita LiveArea background and launch image.
4. Connect the Vita to Wi-Fi. Tap the multiplayer address, port, name and
   password fields, then join a server compatible with Luanti 5.17.
   The Vita keyboard is supplied by SDL and the system IME dialog.

The first distribution supports direct server addresses and game-protocol
media transfers. HTTPS server discovery, the content browser and updater are
excluded. Keep certificate verification enabled if HTTPS support is added
later. Local world hosting is hidden in the Vita menu and no standalone server
is packaged. Server games/assets are downloaded on joining; no game is bundled.

Data and configuration: `ux0:data/luanti/`. Keep this directory when upgrading.
Diagnostics: `ux0:data/luanti/startup.log` and `debug.txt`. Do not share passwords
or private server details from configuration files.

## Controls

These use SDL's standard Vita gamepad mapping, not desktop keyboard emulation.

| Vita input | Action |
| --- | --- |
| Left stick | Move |
| Right stick | Look |
| R | Dig / attack |
| L | Place / use |
| Cross | Jump |
| Circle | Sneak |
| Square | Auxiliary action |
| Triangle | Inventory |
| D-pad left / right | Previous / next hotbar slot |
| D-pad up | Zoom |
| D-pad down | Drop item |
| Select | Chat |
| Start | Pause / close menu |
| Front touchscreen | Menus, inventory and text fields |

Rear touch input is disabled to avoid accidental menu interaction. Native
Vita analog sticks have no L3/R3 clicks; no required action uses them.

## Performance baseline

The baseline caps rendering at 30 FPS, uses 960×544, a 48-node view range,
two mesh workers, at most 1000 cached mapblocks, and a 60-second unused-block
timeout. Shadows, post-processing, anti-aliasing and 3D clouds default off.
These are starting settings, **not measured performance guarantees**. Some
server games and large texture packs can exceed the Vita's memory/CPU budget.

The libc heap is 192 MiB within the application's 256 MiB memory budget, leaving
space for thread stacks and native pools. A separate 4 MiB networking pool is
allocated from that heap. CPU/bus/GPU clocks are 444/222/222 MHz using supported
power APIs; no overclock plugin is required. Release builds and section garbage
collection remove unused binary sections. GLES2 is the only compiled graphics
backend. Desktop process launching and shell-based deletion are replaced or
excluded for Vita. Keeping shared engine source allows upstream fixes to be
merged; deleting unused platform source would not reduce the VPK.

## Rebuild

Linux x86_64 host prerequisites: Python 3.12+, curl, git, native C/C++ compiler,
make, patch, Tcl and CMake 3.16+. The lock pins the SDK archive, dependency
digests, SDL backend patch and SQLite source commit. Rolling GitHub package
URLs may be replaced upstream; retain the verified archive cache. A changed
checksum is a failure that requires an explicit dependency refresh and testing.

```sh
export VITASDK=/workspace/vitasdk
# Set CMAKE to the installed executable if it is not on PATH.
export CMAKE=/workspace/host-tools/cmake/data/bin/cmake
export LUANTI_VITA_CACHE=/workspace/downloads
python3 vita/bootstrap.py
bash vita/build.sh
python3 vita/test_crypto.py
```

The optional emulator runtime prepared in this cloud workspace can run the
startup smoke with `bash vita/emulator-smoke.sh --app-args=--version`.
It uses a separate, signed Debian package runtime and software graphics, without
changing the host's system libraries. Normal launch uses the same script without
arguments and requires the shader compiler in the emulator's own Vita filesystem.
The user's installed compiler on the physical Vita is separate from that filesystem.

Run from the existing checkout. Cloud tasks are already isolated; do not create
a Git worktree for ordinary development. The bootstrap writes dependencies and
caches outside the checkout and generates SQLite source in its dependency build
directory. It does not modify Luanti's tracked files. It refuses an SDK whose
version metadata differs from the lock. LTO is disabled until Vita conversion
and runtime compatibility are established. Position-independent desktop code
is disabled because Vita's SELF converter cannot encode its GOT relocations.

`validate_package.py` checks ZIP integrity, SELF/SFO headers, title identity,
required runtime assets, indexed PNG sizes and LiveArea image references.
`test_crypto.py` compiles the actual Vita SHA-256 branch with a native compiler
and compares standard, padding-boundary and long-input vectors to Python's
independent implementation. It does not test the kernel RNG or multiplayer SRP.

The SDL vitaGL backend patch is retained verbatim from the VitaSDK packages
commit recorded in `dependencies.lock.json`. Its inherited SDL/Zlib licensing
is included in the patch source. Upstream engine/artwork and font notices are
included in the VPK. Dependency license metadata is in the verified archives.

## Real-device acceptance checks

Record device model, firmware/homebrew versions, server game and server version.
Then verify:

- Launch, readable LiveArea/menu and native keyboard text entry; no crash.
- Join a small server, authenticate, download media and render its world.
- Test every binding, diagonal movement, deadzone, inventory dragging and chat.
- Walk, dig and place for at least 15 minutes; record average FPS, frame-time
  spikes, memory failures and temperature/battery impact. Start at view range 48.
- Disconnect/rejoin, suspend/resume and close/reopen; configuration persists.
- Repeat on the intended multiplayer game with its real texture/mod workload.

Vita3K is useful for startup diagnosis, but its software-rendered host frame
rate cannot establish real Vita performance. Never label the build validated
for gameplay until these device checks pass.
