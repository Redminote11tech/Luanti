# Luanti Vita development package

`luanti-vita-dev.vpk` is version 1.02 of the multiplayer Vita port, based on
Luanti 5.17.0. Install it using VitaShell. `SHA256SUMS` records its checksum.

The ARM build, package validation, 13 SHA-256 vectors and Unicode boundary tests
pass. The packaged engine also passes Vita3K diagnostics for controller mappings,
analog axes, SQLite persistence, kernel RNG, SRP mutual authentication, kernel
semaphores, host UDP echo and reliable UDP fragmentation. Python independently
checks the SRP verifier. This verifies transport and crypto components;
**a complete rendered server session and physical controls are not yet verified**.
The emulator lacks the shader compiler required for graphics testing. Sound,
suspend/resume and sustained real Vita performance also remain unverified.
The 30 FPS setting is a cap, not a measured frame rate.

The device needs homebrew support and `ur0:data/libshacccg.suprx` installed.
1.01 corrected the SELF memory-mode request associated with the pre-startup
C1-2609-7 error. 1.02 fixes sockets and storage, adds native text editing and
menu/inventory controller controls, bounds particles and excludes hosting paths.

See [controls and installation](../vita/README.md) and
[the code review and validation results](../vita/REVIEW.md).
