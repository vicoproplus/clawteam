#include "moonbit.h"
#include <stdint.h>

#ifdef _WIN32
// Windows doesn't have Unix-style symlinks in a compatible way
// Return -1 to indicate not supported
MOONBIT_FFI_EXPORT
int64_t
moonbit_moonclaw_fs_readlink(
  moonbit_bytes_t path,
  moonbit_bytes_t buf,
  uint64_t bufsize
) {
  // Not supported on Windows
  return -1;
}

#else
#include <unistd.h>

MOONBIT_FFI_EXPORT
int64_t
moonbit_moonclaw_fs_readlink(
  moonbit_bytes_t path,
  moonbit_bytes_t buf,
  uint64_t bufsize
) {
  return (int64_t)readlink((const char *)path, (char *)buf, (size_t)bufsize);
}

#endif
