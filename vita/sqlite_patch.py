"""Adapt the pinned Unix VFS to Vita mount names and supported syscalls."""

def patch(source):
    source = source.replace("#include <sys/ioctl.h>", "#ifndef __vita__\n#include <sys/ioctl.h>\n#endif")
    source = source.replace("#define HAVE_FCHOWN 1", "#undef HAVE_FCHOWN")
    source = source.replace("#define HAVE_READLINK 1", "#undef HAVE_READLINK")
    anchor = "  DbPath path;\n  UNUSED_PARAMETER(pVfs);"
    if source.count(anchor) != 1:
        raise RuntimeError("SQLite unixFullPathname changed; review the Vita patch")
    return source.replace(anchor, '''#ifdef __vita__
  /* Vita paths are absolute at the mount, rather than at a Unix root. */
  if( strchr(zPath, ':') && strchr(zPath, ':')[1]=='/' ){
    if( strlen(zPath)>=(size_t)nOut ) return SQLITE_CANTOPEN_BKPT;
    sqlite3_snprintf(nOut, zOut, "%s", zPath);
    return SQLITE_OK;
  }
#endif
''' + anchor)

# The multiplayer client has one process and serialized SQLite connections.
# newlib does not implement POSIX record locks. Never use this build for a
# database shared with another process, or for a standalone/local server.
FLAGS = ["-O2", "-DSQLITE_THREADSAFE=1", "-DSQLITE_OMIT_LOAD_EXTENSION",
         "-DSQLITE_DISABLE_LFS", "-DSQLITE_OMIT_WAL", "-DSQLITE_MAX_MMAP_SIZE=0",
         '-DSQLITE_DEFAULT_UNIX_VFS="unix-none"']
