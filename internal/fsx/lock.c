#include <moonbit.h>

#ifdef _WIN32
#include <windows.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fsx_lock_file(int32_t fd) {
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    return -1;
  }

  OVERLAPPED overlapped = {0};
  overlapped.Offset = 0;
  overlapped.OffsetHigh = 0;

  // Lock entire file
  DWORD lenLow = 0xFFFFFFFF;
  DWORD lenHigh = 0xFFFFFFFF;

  if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, lenLow, lenHigh, &overlapped)) {
    return -1;
  }
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fsx_get_lock_owner(int32_t fd) {
  // Windows doesn't have direct equivalent to F_GETLK
  // Try to lock and unlock to check if it's locked
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    return -1;
  }

  OVERLAPPED overlapped = {0};
  overlapped.Offset = 0;
  overlapped.OffsetHigh = 0;

  // Try non-blocking lock
  if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlapped)) {
    // Lock failed - file is already locked by another process
    return 1; // Return non-zero PID to indicate locked
  }

  // We got the lock, so it was unlocked - release it
  UnlockFileEx(h, 0, 1, 0, &overlapped);
  return 0;
}

#else
#include <fcntl.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fsx_lock_file(int32_t fd) {
  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0; // lock the whole file
  fl.l_pid = 0;
  return fcntl(fd, F_SETLK, &fl);
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_fsx_get_lock_owner(int32_t fd) {
  struct flock fl;
  fl.l_type = F_WRLCK; // or F_RDLCK, doesn't matter for F_GETLK
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0; // check the whole file
  fl.l_pid = 0;
  if (fcntl(fd, F_GETLK, &fl) == -1) {
    return -1; // error
  }
  if (fl.l_type == F_UNLCK) {
    return 0; // not locked
  }
  return fl.l_pid; // return the PID of the lock owner
}

#endif