#!/usr/bin/env python3
"""Run actual Vita storage, bindings and UDP tests in the optional Vita3K runtime."""
from pathlib import Path
import os
import socket
import subprocess
import threading
import hashlib
import re

root = Path(__file__).resolve().parent.parent
runtime = Path(os.environ.get("LUANTI_EMULATOR_RUNTIME", "/workspace/emulator-runtime"))
report = runtime / "user/data/Vita3K/Vita3K/ux0/data/luanti/vita-selftest.log"
report.unlink(missing_ok=True)
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
    udp.bind(("127.0.0.1", 0))
    udp.settimeout(45)
    port = udp.getsockname()[1]
    received = []
    def echo():
        try:
            payload, sender = udp.recvfrom(65536)
            received.append(payload)
            udp.sendto(payload, sender)
        except OSError:
            pass
    threading.Thread(target=echo, daemon=True).start()
    result = subprocess.run(["bash", str(root / "vita/emulator-smoke.sh"),
        f"--app-args=--vita-selftest, --vita-test-port, {port}"],
        cwd=root, env={**os.environ, "LUANTI_EMULATOR_TIMEOUT": "45"},
        capture_output=True, text=True)
    if report.exists():
        text = report.read_text()
        print(text, end="")
    else:
        text = ""
    if result.returncode or "PASS all requested diagnostics" not in text or received != [b"binary\0value"]:
        print(result.stdout[-5000:])
        print(result.stderr[-5000:])
        raise SystemExit("Vita emulator diagnostics failed")
# RFC 5054 group used by the engine; arithmetic and hashing are independent.
groups = (root / "src/util/srp.cpp").read_text().split("{/* 2048 */", 1)[1].split("{/* 4096 */", 1)[0]
constants = re.findall(r'"([0-9A-F]+)"', groups)
modulus = int("".join(constants[:-1]), 16)
generator = int(constants[-1], 16)
inner = hashlib.sha256(b"vitadiagnostic:test password").digest()
exponent = int.from_bytes(hashlib.sha256(b"vita-selftest-salt" + inner).digest())
verifier = pow(generator, exponent, modulus)
expected = hashlib.sha256(verifier.to_bytes((verifier.bit_length() + 7) // 8)).hexdigest()
if f"SRP verifier SHA256 {expected}" not in text:
    raise SystemExit("Vita SRP verifier differs from independent Python reference")
print("PASS Vita SRP verifier matches independent Python SHA-256/modular arithmetic")
print("PASS emulator exited successfully; host confirmed exact UDP payload")
