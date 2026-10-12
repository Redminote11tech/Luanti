#!/usr/bin/env python3
"""Validate VPK contents and metadata; this does not emulate the application."""
from pathlib import Path
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile

def validate(path):
    with zipfile.ZipFile(path) as vpk:
        assert vpk.testzip() is None, "Corrupt ZIP member"
        names = vpk.namelist()
        assert len(names) == len(set(names)), "Duplicate ZIP paths"
        assert all(not n.startswith("/") and ".." not in n.split("/") for n in names)
        for name in ("eboot.bin", "sce_sys/param.sfo", "sce_sys/icon0.png",
                     "builtin/init.lua", "builtin/mainmenu/init.lua",
                     "client/shaders/Irrlicht/Solid.vsh",
                     "fonts/Arimo-Regular.ttf", "LICENSE.txt"):
            assert name in names, f"Missing {name}"
            assert len(vpk.read(name)) > 0, f"Empty {name}"
        executable = vpk.read("eboot.bin")
        assert executable[:4] == b"SCE\x00", "Invalid SELF header"
        # VitaSDK SELF control-info 6 carries loader memory-mode settings.
        # Nonzero total_memsize selects a system-mode budget, which can cause
        # C1-2609-7 before main() and is invalid for this normal game build.
        control_offset, control_size = struct.unpack_from("<QQ", executable, 0x68)
        assert control_offset + control_size <= len(executable), "Invalid control-info bounds"
        cursor, end, found_memory = control_offset, control_offset + control_size, False
        while cursor < end:
            kind, size = struct.unpack_from("<II", executable, cursor)
            assert size >= 16 and cursor + size <= end, "Invalid control-info record"
            if kind == 6:
                assert size >= 32, "Truncated memory control-info"
                _, attributes, physical, total = struct.unpack_from("<4I", executable, cursor + 16)
                assert (attributes, physical, total) == (0, 0, 0), \
                    "SELF requests system-mode memory; expected normal application mode"
                found_memory = True
            cursor += size
        assert found_memory, "Missing memory control-info"
        sfo = vpk.read("sce_sys/param.sfo")
        assert sfo[:4] == b"\x00PSF", "Invalid SFO header"
        _, _, keys, values, count = struct.unpack_from("<5I", sfo)
        fields = {}
        for i in range(count):
            key, fmt, length, maximum, offset = struct.unpack_from("<HHIII", sfo, 20 + i * 16)
            key_end = sfo.index(b"\0", keys + key)
            fields[sfo[keys + key:key_end].decode()] = sfo[values + offset:values + offset + length]
        assert fields["TITLE_ID"].rstrip(b"\0") == b"LUANTI001"
        assert fields["TITLE"].rstrip(b"\0") == b"Luanti Vita"
        assert fields["CATEGORY"].rstrip(b"\0") == b"gd"
        pngs = {"sce_sys/icon0.png": (128, 128),
                "sce_sys/livearea/contents/bg.png": (840, 500),
                "sce_sys/livearea/contents/startup.png": (280, 158)}
        for name, size in pngs.items():
            png = vpk.read(name)
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack_from(">II", png, 16) == size
            assert png[24:26] == b"\x08\x03", "LiveArea requires indexed PNG8"
        template = ET.fromstring(vpk.read("sce_sys/livearea/contents/template.xml"))
        for image in template.findall(".//image") + template.findall(".//startup-image"):
            assert "sce_sys/livearea/contents/" + image.text in names
        assert not any(n.startswith(("games/", "android/", "unix/", ".git/")) for n in names)
    print(f"VPK structure, SELF, SFO, LiveArea and runtime assets passed: {path}")

if __name__ == "__main__":
    validate(Path(sys.argv[1]))
