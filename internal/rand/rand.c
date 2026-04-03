#include "moonbit.h"
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_rand_bytes(moonbit_bytes_t buf) {
  NTSTATUS status = BCryptGenRandom(
    NULL,
    (PUCHAR)buf,
    (ULONG)Moonbit_array_length(buf),
    BCRYPT_USE_SYSTEM_PREFERRED_RNG
  );
  if (status != 0) {
    return EIO;
  }
  return 0;
}

#else
#include <sys/random.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_rand_bytes(moonbit_bytes_t buf) {
  int32_t result = getentropy(buf, Moonbit_array_length(buf));
  if (result == -1) {
    return errno;
  } else {
    return 0;
  }
}

#endif