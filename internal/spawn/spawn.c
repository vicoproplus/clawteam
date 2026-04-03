#include "moonbit.h"
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_process_getpid(void) {
  return (int32_t)_getpid();
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_process_getppid(void) {
  // Windows doesn't have a direct equivalent to getppid
  // Return 0 as a placeholder
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_process_kill(int32_t pid, int32_t sig) {
  HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
  if (hProcess == NULL) {
    return GetLastError();
  }

  BOOL result = TerminateProcess(hProcess, (UINT)sig);
  CloseHandle(hProcess);

  if (!result) {
    return GetLastError();
  }
  return 0;
}

#else
#include <signal.h>
#include <unistd.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_process_getpid(void) {
  return (int32_t)getpid();
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_process_getppid(void) {
  return (int32_t)getppid();
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_process_kill(int32_t pid, int32_t sig) {
  int32_t result = kill((pid_t)pid, sig);
  if (result == -1) {
    return errno;
  } else {
    return 0;
  }
}

#endif