# Luanti Vita development package

`luanti-vita-dev.vpk` is the development build of the Vita port based on Luanti
5.17.0. Install it using VitaShell. `SHA256SUMS` records its SHA-256 checksum.

The ARM build, package structure and 13 crypto vectors passed. Vita3K ran the
engine's `--version` path and exited with code 0. The emulator lacks the shader
compiler required for graphics testing. **Multiplayer gameplay, controls, sound
and sustained real Vita performance remain unverified.** The 30 FPS setting is
a rendering cap, not a measured frame-rate claim.

The device needs homebrew support and `ur0:data/libshacccg.suprx` installed.
Version 1.01 removes an incorrect system-mode memory request from the executable,
addressing the packaging fault associated with C1-2609-7 before startup.
See [the Vita instructions](../vita/README.md) for controls, rebuilding,
limitations, diagnostics and the hardware acceptance checklist.
