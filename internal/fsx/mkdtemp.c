#include "moonbit.h"
#include <errno.h>

#ifdef _WIN32
#include <stdlib.h>
#include <direct.h>
#include <string.h>
#include <io.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fs_mkdtemp(moonbit_bytes_t template) {
  errno = 0;

  // Use _mktemp_s to generate unique name
  if (_mktemp_s((char *)template, strlen((char *)template) + 1) != 0) {
    return errno;
  }

  // Create directory
  if (_mkdir((const char *)template) == 0) {
    return 0;
  }

  return errno;
}

#else
#include <stdlib.h>
#include <unistd.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fs_mkdtemp(moonbit_bytes_t template) {
  errno = 0;
  if (mkdtemp((char *)template)) {
    return 0;
  } else {
    return errno;
  }
}

#endif