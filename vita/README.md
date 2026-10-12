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
Version 1.02 passes the packaged ARM engine's headless Vita3K diagnostics:
12 gameplay button mappings and their releases, all four analog axes and neutral
release, mounted SQLite transactions and persistence, kernel RNG, SRP-2048
mutual authentication and rejection of a bad proof, a host UDP echo, and Luanti's
reliable UDP handshake with 8 KiB fragmentation and two-way channels. Python
independently confirms its SRP verifier. This tests the transport and login
cryptography, not a complete authenticated session with a real game server.
**Rendered server gameplay, physical menu/inventory/keyboard operation, sound,
suspend/resume and frame rate remain unverified. Do not treat this as a finished
release.** See [the review](REVIEW.md) for findings and evidence.

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
   The app opens the native system keyboard when you select an editable field.
   Existing text is preserved, password entry is masked, and cancel keeps the
   original value. Accepting an edit does not automatically join a server.

The first distribution supports direct server addresses and game-protocol
media transfers. HTTPS server discovery, the content browser and updater are
excluded. Keep certificate verification enabled if HTTPS support is added
later. Local world hosting is hidden and rejected by the Vita client;
the dedicated-server execution path is excluded. Server games/assets are downloaded on joining; no game is bundled.

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
| Triangle | Open / close inventory |
| D-pad left / right | Previous / next hotbar slot |
| D-pad up | Zoom |
| D-pad down | Drop item |
| Select | Chat |
| Start | Pause / close menu |
| Front touchscreen | Menus, inventory and text fields |

In menus and inventory, the left stick or D-pad moves a visible pointer.
Cross or R selects with a left click; hold it to drag. L performs a right
click (split a stack or place one item). Square performs Shift+click for quick
transfer through the formspec's inventory rings. Circle or Start goes back.
Triangle closes in-game inventory/formspecs. Front touch remains available.
Select opens chat and its native keyboard; accepting sends the message,
including `/commands`. If chat remains open, Cross or Select opens the keyboard
again and Circle or Start closes chat.

These cover the essential gameplay and inventory actions. Desktop shortcuts
for fly/fast/noclip, camera mode and debugging do not all have dedicated Vita
buttons. Settings expose the supported movement options; server privileges
still apply. Do not assume every desktop shortcut has a controller equivalent.

Rear touch input is disabled to avoid accidental menu interaction. Native
Vita analog sticks have no L3/R3 clicks; no required action uses them.

## Performance baseline

The baseline caps rendering at 30 FPS, uses 960×544, a 48-node view range,
two mesh workers, at most 1000 cached mapblocks, and a 60-second unused-block
timeout. Shadows, post-processing, anti-aliasing and 3D clouds default off.
Particle spawners are capped at 512 particles per burst/rate and 1024 active
particles in total. Server effects may be reduced at these limits.
The VPK is about 9% smaller than 1.01 after excluding hosting startup paths.
These are starting settings, **not measured performance guarantees**. Some
server games and large texture packs can exceed the Vita's memory/CPU budget.

The libc heap is 192 MiB within the normal game memory allocation, leaving
space for thread stacks and native pools. A separate 4 MiB networking pool is
allocated from that heap. CPU/bus/GPU clocks are 444/222/222 MHz using supported
power APIs; no overclock plugin is required. Release builds and section garbage
collection remove unused binary sections. GLES2 is the only compiled graphics
backend. Desktop process launching and shell-based deletion are replaced or
excluded for Vita. Keeping shared engine source allows upstream fixes to be
merged; deleting unused platform source would not reduce the VPK.

The SELF must use the default zero memory-budget field (normal application
mode). VitaSDK's `MEMSIZE`/`vita-make-fself -m` selects system-mode memory,
not a game heap limit. The first package incorrectly requested `0x40000` there
and a real Vita reported C1-2609-7 before startup. Package version 1.01 removes
that request; the package validator rejects nonzero memory-mode fields.

Version 1.02 also fixes Vita socket address lengths, explicitly rejects IPv6,
and adapts SQLite mounted paths and locking. Queue waits use native Vita kernel
semaphores; the previous adapter failed during the reliable UDP diagnostic. The SQLite build uses `unix-none`
for this single-process multiplayer client with serialized database access.
Do not share its database with another process or use this dependency build
for local/standalone server hosting. It still uses real transactions, file I/O
and synchronization; storage errors are not hidden by dummy syscalls.

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
python3 vita/test_text.py
```

`python3 vita/test_emulator.py` runs opt-in diagnostics in the packaged ARM
executable using the optional local runtime. It checks Unicode conversion,
gameplay button presses/releases, analog direction changes, semaphore tokens, SQLite transactions
and reopen, kernel randomness, SRP authentication, reliable UDP fragmentation
and channels, plus a host UDP echo. Python independently checks the SRP verifier.
It writes `ux0:data/luanti/vita-selftest.log`; no real account credentials are
used. The diagnostic flag does not start graphics or connect to a public server.

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

`validate_package.py` checks ZIP integrity, SELF/SFO headers, normal application
memory-mode fields, title identity,
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
