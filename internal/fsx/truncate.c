#include <moonbit.h>

#ifdef _WIN32
#include <io.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fsx_ftruncate(int32_t fd, int64_t length) {
  return _chsize_s(fd, length);
}

#else
#include <unistd.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fsx_ftruncate(int32_t fd, int64_t length) {
  return ftruncate(fd, length);
}

#endif