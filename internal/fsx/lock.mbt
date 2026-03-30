///|
/// Advisory file locking functions.
/// Note: These functions are only available on Unix-like systems.
/// On Windows, file locking works differently and these functions are not supported.
#cfg(not(platform="windows"))
extern "c" fn fsx_lock_file(fd : Int) -> Int = "moonbit_moonclaw_fsx_lock_file"

///|
/// Acquire an advisory lock on `file`.
///
/// On error this raises an `@errno.Errno` populated from `@errno.get()`.
/// Note: Not available on Windows.
#cfg(not(platform="windows"))
pub fn lock_file(file : @fs.File) -> Unit raise {
  let fd = file.fd()
  let ret = fsx_lock_file(fd)
  if ret == -1 {
    raise @errno.Errno(@errno.get())
  }
}

///|
/// Acquire an advisory lock on `file`.
/// On Windows, this is a no-op.
#cfg(platform="windows")
pub fn lock_file(file : @fs.File) -> Unit {
  ignore(file)
  // Windows file locking works differently, no-op for now
}

///|
#cfg(not(platform="windows"))
extern "c" fn fsx_get_lock_owner(fd : Int) -> Int = "moonbit_moonclaw_fsx_get_lock_owner"

///|
/// Return the process id that currently holds the lock for `file`.
///
/// - Returns `Some(pid)` when the file is locked.
/// - Returns `None` when the file is currently unlocked.
/// - Raises `@errno.Errno` on error.
/// Note: Not available on Windows.
#cfg(not(platform="windows"))
pub fn get_lock_owner(file : @fs.File) -> Int? raise {
  let fd = file.fd()
  let pid = fsx_get_lock_owner(fd)
  if pid == -1 {
    raise @errno.Errno(@errno.get())
  } else if pid == 0 {
    None
  } else {
    Some(pid)
  }
}

///|
/// Return the process id that currently holds the lock for `file`.
/// On Windows, always returns None.
#cfg(platform="windows")
pub fn get_lock_owner(file : @fs.File) -> Int? {
  ignore(file)
  // Windows file locking works differently, return None for now
  None
}
