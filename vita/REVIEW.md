# Vita 1.02 review — 12 October 2026

Scope: launch/package compatibility, multiplayer transport and authentication,
client storage, controller/UI input, and obvious allocation risks. This is a
focused code review and component validation, not proof that every possible
error or every server game has been covered.

## Fixed findings

| Area | Finding and change |
| --- | --- |
| Launch | Keep the normal application SELF memory-mode field at zero. 1.01 fixed the earlier invalid system-mode request; validation still rejects it. |
| UDP | Vita's BSD socket structures require `sin_len`/`sin6_len`. Set the lengths for bind/send; reject unsupported IPv6 explicitly. |
| Receive queues | The first reliable-UDP diagnostic returned invalid queue data and crashed. Use native kernel semaphores with relative timeouts, exact poll/timeout handling and checked creation/signalling. The transport diagnostic then completes. |
| SQLite | Unix path normalization mangled `ux0:/...`, and Vita lacks POSIX record locks. Preserve mounted absolute paths; select SQLite's supported `unix-none` VFS for the single-process multiplayer client. Real transaction/file/sync operations remain enabled. |
| Keyboard | SDL opened an empty keyboard and inserted accepted text into the existing field. The app now owns an asynchronous native dialog, preserves initial text, replaces accepted edits, cancels without changing the field, masks passwords, disables automatic capitalization and handles multiline fields. |
| Unicode | Convert UTF-32 `wchar_t` to Vita UTF-16 with surrogate pairs and bounded buffers; replace malformed input and never truncate in the middle of a pair. |
| Inventory | Triangle now closes in-game formspecs. Menus/inventory have a controller pointer, left/right clicks, dragging, quick transfer and back controls. Clear held input when a menu is hidden. |
| Chat | Select opens the native keyboard; accepting sends the resulting full message/command. Cancel closes chat; chat can reopen the keyboard through Cross/Select. |
| Effects | Cap Vita particle spawner amount at 512 and active particles at 1024, bound reservations, avoid server-controlled float allocation estimates, and correct active counts when a particle is dropped. Widen the spawner loop counter to avoid wrapping at 65535. |
| Hosting | Exclude local-server construction/startup and dedicated-server execution from the Vita client. Unsupported hosting produces a clear error. This also removes unused binary sections. |
| Header correctness | Forward-declare `con::PeerHandler` in its actual namespace so the connection factory does not depend on include order. |

The VPK shrank from approximately 12.9 MB in 1.01 to 11.7 MB. This is measured
package size, not a measurement of runtime memory or frame rate. Desktop platform
files remain available for upstream maintenance; platform selection and section
garbage collection remove unused executable code. Deleting those source files
would not itself improve runtime performance.

## Validation

- Release ARM build and ELF → VELF → SELF → VPK conversion pass.
- `vita/validate_package.py`: package/SELF/SFO, zero memory-mode field, assets,
  indexed LiveArea images and template references pass.
- `vita/test_crypto.py`: 13 SHA-256 vectors pass against Python hashlib.
- `vita/test_text.py`: actual keyboard conversion passes AddressSanitizer and
  UndefinedBehaviorSanitizer checks for Unicode, malformed surrogates and limits.
- `vita/test_emulator.py`: the actual packaged ARM executable passes 12 gameplay
  button mappings with press/release handling, four analog axes with direction
  reversal and neutral release, mounted SQLite transactions/binary values/reopen/
  delete, kernel RNG, SRP-2048 mutual authentication and invalid-proof rejection,
  semaphore token accounting, host UDP echo/poll timeout, and Luanti reliable UDP
  peer handshake with 8 KiB fragmentation and bidirectional channels. Python
  independently checks the SRP verifier using hashlib and modular arithmetic.
  [Recorded output](validation/headless-1.02.txt).
- Emulator `--version` exits 0. Normal launch without the shader compiler exits 1
  with the dependency explanation, before graphics initialization.

These controller tests inject standard SDL/Irrlicht events into the engine's
actual input receiver. They do not test the physical Vita buttons, rendered
pointer, inventory GUI or native keyboard dialog. The UDP peer test exercises
Luanti's transport, not the complete game-server handshake and asset loading.

## Remaining verification

A full rendered, authenticated game-server session, media downloads, physical
inventory/crafting/keyboard operation, sound, suspend/resume, and sustained FPS
and memory use remain unverified. The cloud emulator lacks `libshacccg.suprx`;
the user's installed compiler on the physical Vita does not populate the emulator.
No compiler substitute was used and no rendering success is claimed.

Use a compatible server's IPv4 address and port; HTTPS discovery is disabled.
The essential controller bindings are documented in [README.md](README.md).
There is no dedicated Vita button for every desktop/debug shortcut. Some server
mods and texture packs may exceed the hardware budget. Follow the real-device
acceptance checks in the README before treating this as a finished release.
