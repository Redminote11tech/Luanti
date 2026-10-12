#!/usr/bin/env python3
"""Install the pinned Linux x86_64 Vita build dependencies outside the checkout.

Archives are checked before extraction; rolling release assets may disappear.
Keep the verified cache with the environment snapshot. Requires Python 3.12,
curl, git, a native C compiler, make, Tcl and CMake on the host.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

root = Path(__file__).resolve().parent.parent
lock = json.loads((root / "vita/dependencies.lock.json").read_text())
sdk = Path(os.environ.get("VITASDK", root.parent / "vitasdk")).resolve()
cache = Path(os.environ.get("LUANTI_VITA_CACHE", root.parent / "vita-cache")).resolve()
build = Path(os.environ.get("LUANTI_VITA_DEPS_BUILD", root.parent / "vita-deps-build")).resolve()
cmake = os.environ.get("CMAKE", "cmake")
jobs = os.environ.get("LUANTI_BUILD_JOBS", "4")
cache.mkdir(parents=True, exist_ok=True)
build.mkdir(parents=True, exist_ok=True)

def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()

def run(args, cwd=None):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True,
                   env={**os.environ, "VITASDK": str(sdk)})

def download(item, name):
    archive = cache / name
    if not archive.exists():
        temporary = archive.with_suffix(archive.suffix + ".partial")
        run(["curl", "--fail", "--location", "--retry", "2", "--output", temporary, item["url"]])
        if digest(temporary) != item["sha256"]:
            raise RuntimeError(f"Checksum mismatch: {name}; do not use this archive")
        temporary.rename(archive)
    if digest(archive) != item["sha256"]:
        raise RuntimeError(f"Cached archive checksum mismatch: {archive}")
    return archive

def extract(archive, destination, strip_top=False):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as tar:
        if strip_top:
            members = tar.getmembers()
            top = members[0].name.split("/", 1)[0]
            for member in members:
                if member.name == top:
                    continue
                if not member.name.startswith(top + "/"):
                    raise RuntimeError("Archive has multiple roots")
                member.name = member.name[len(top) + 1:]
                tar.extract(member, destination, filter="data")
        else:
            tar.extractall(destination, filter="data")

sdk_archive = download(lock["sdk"], "vitasdk.tar.bz2")
if (sdk / "bin/arm-vita-eabi-gcc").exists():
    version = sdk / "version_info.txt"
    if not version.exists() or digest(version) != lock["sdk"]["version_info_sha256"]:
        raise RuntimeError("Existing SDK differs from the lock; use a separate VITASDK directory")
else:
    if sdk.exists() and any(sdk.iterdir()):
        raise RuntimeError("Refusing to overwrite a nonempty SDK directory")
    extract(sdk_archive, sdk, strip_top=True)
run([sdk / "bin/arm-vita-eabi-gcc", "--version"])

for package in lock["packages"]:
    extract(download(package, package["name"] + ".tar.xz"), sdk / "arm-vita-eabi")
    print("Verified dependency:", package["name"], flush=True)

patch = root / "vita/sdl2-vitagl.patch"
if digest(patch) != lock["sdl2"]["patch_sha256"]:
    raise RuntimeError("SDL backend patch differs from the lock")
sdl_src = build / "sdl2-source"
sdl_stamp = sdl_src / ".luanti-vita-source"
source_id = lock["sdl2"]["sha256"] + lock["sdl2"]["patch_sha256"]
if not sdl_stamp.exists():
    if sdl_src.exists() and any(sdl_src.iterdir()):
        raise RuntimeError("Refusing to overwrite an existing SDL source directory")
    extract(download(lock["sdl2"], "SDL2-2.32.8.tar.gz"), sdl_src, strip_top=True)
    run(["patch", "-p1", "-i", patch], cwd=sdl_src)
    sdl_stamp.write_text(source_id)
elif sdl_stamp.read_text() != source_id:
    raise RuntimeError("SDL source stamp differs from the lock; select a new dependency build directory")
run([cmake, "-S", sdl_src, "-B", build / "sdl2",
     "-DCMAKE_TOOLCHAIN_FILE=" + str(sdk / "share/vita.toolchain.cmake"),
     "-DCMAKE_INSTALL_PREFIX=" + str(sdk / "arm-vita-eabi"),
     "-DCMAKE_BUILD_TYPE=Release", "-DVIDEO_VITA_VGL=ON",
     "-DSDL_TEST=OFF", "-DSDL_SHARED=OFF"])
run([cmake, "--build", build / "sdl2", "--parallel", jobs])
run([cmake, "--install", build / "sdl2"])

sqlite_src = build / "sqlite-source"
if not sqlite_src.exists():
    run(["git", "init", sqlite_src])
    run(["git", "-C", sqlite_src, "fetch", "--depth=1", lock["sqlite"]["url"], lock["sqlite"]["commit"]])
    run(["git", "-C", sqlite_src, "checkout", "--detach", "FETCH_HEAD"])
commit = subprocess.check_output(["git", "-C", str(sqlite_src), "rev-parse", "HEAD"], text=True).strip()
if commit != lock["sqlite"]["commit"]:
    raise RuntimeError("Existing SQLite checkout differs from the lock")
sqlite_build = build / "sqlite"
sqlite_build.mkdir(exist_ok=True)
if not (sqlite_build / "Makefile").exists():
    run([sqlite_src / "configure", "--disable-shared", "--disable-readline"], cwd=sqlite_build)
run(["make", "-j" + jobs, "sqlite3.c"], cwd=sqlite_build)
source = (sqlite_build / "sqlite3.c").read_text()
# Supported SQLite feature exclusions for Vita's newlib. No dummy syscalls.
source = source.replace("#include <sys/ioctl.h>", "#ifndef __vita__\n#include <sys/ioctl.h>\n#endif")
source = source.replace("#define HAVE_FCHOWN 1", "#undef HAVE_FCHOWN")
source = source.replace("#define HAVE_READLINK 1", "#undef HAVE_READLINK")
vita_source = sqlite_build / "sqlite3-vita.c"
vita_source.write_text(source)
flags = ["-O2", "-DSQLITE_THREADSAFE=1", "-DSQLITE_OMIT_LOAD_EXTENSION",
         "-DSQLITE_DISABLE_LFS", "-DSQLITE_OMIT_WAL", "-DSQLITE_MAX_MMAP_SIZE=0"]
run([sdk / "bin/arm-vita-eabi-gcc", *flags, "-c", vita_source, "-o", sqlite_build / "sqlite3-vita.o"])
run([sdk / "bin/arm-vita-eabi-ar", "rcs", sdk / "arm-vita-eabi/lib/libsqlite3.a", sqlite_build / "sqlite3-vita.o"])
shutil.copy2(sqlite_build / "sqlite3.h", sdk / "arm-vita-eabi/include/sqlite3.h")
print("Vita dependency setup complete. Run vita/build.sh with the same VITASDK.")
