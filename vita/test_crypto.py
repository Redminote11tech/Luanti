#!/usr/bin/env python3
"""Check the actual Vita SHA-256 byte-order branch against independent vectors.

This native test exercises byte order and padding, not Vita kernel RNG or SRP.
"""
import ctypes
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    (tmp / "cmake_config.h").write_text("/* Vita has no endian.h */\n")
    so = tmp / "sha256.so"
    subprocess.run([
        os.environ.get("CC", "cc"), "-shared", "-fPIC", "-O2", "-D__vita__",
        "-I" + str(tmp), str(root / "lib/sha256/sha256.c"), "-o", str(so),
    ], check=True)
    sha = ctypes.CDLL(str(so)).SHA256
    sha.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
    sha.restype = ctypes.c_void_p
    vectors = [b"", b"abc", b"a" * 1_000_000]
    vectors += [bytes(i % 256 for i in range(length))
                for length in (1, 55, 56, 63, 64, 65, 127, 128, 129, 4096)]
    for data in vectors:
        output = (ctypes.c_ubyte * 32)()
        sha(data, len(data), output)
        assert bytes(output) == hashlib.sha256(data).digest(), len(data)
    print(f"Vita SHA-256 byte-order branch: {len(vectors)} vectors passed")
