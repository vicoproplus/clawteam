///|
/// File truncation function.
/// Note: This function is only available on Unix-like systems.
/// On Windows, this is not supported.
#cfg(not(platform="windows"))
extern "c" fn fsx_ftruncate(fd : Int, length : Int64) -> Int = "moonbit_moonclaw_fsx_ftruncate"

///|
/// Truncate `file` to the given `length` in bytes.
///
/// On failure this raises an `@errno.Errno` populated from `@errno.get()`.
/// Note: Not available on Windows.
#cfg(not(platform="windows"))
pub fn truncate(file : @fs.File, length : Int64) -> Unit raise {
  let result = fsx_ftruncate(file.fd(), length)
  if result != 0 {
    raise @errno.Errno(@errno.get())
  }
}

///|
/// Truncate `file` to the given `length` in bytes.
/// On Windows, this raises an error as it's not supported.
#cfg(platform="windows")
pub fn truncate(file : @fs.File, length : Int64) -> Unit raise {
  ignore(file)
  ignore(length)
  // Windows truncate requires different implementation
  raise @errno.Errno(0) // Not supported on Windows
}
