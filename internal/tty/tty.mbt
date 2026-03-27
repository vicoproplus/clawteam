///|
extern "c" fn tty_is_a_tty(fd : Int) -> Int = "moonbit_moonclaw_tty_is_a_tty"

///|
#callsite(autofill(loc))
pub fn isatty(fd : Int, loc~ : SourceLoc) -> Bool raise {
  let result = tty_is_a_tty(fd)
  if result == 0 {
    let errno = @errno.get()
    if errno == @errno.ebadf {
      abort("\{loc}: Not a valid file descriptor: \{fd}")
    } else if errno == @errno.enotty {
      return false
    } else {
      raise @errno.Errno(errno)
    }
  }
  return true
}

///|
#borrow(size)
extern "c" fn tty_get_win_size(size : FixedArray[Int]) -> Int = "moonbit_moonclaw_tty_get_win_size"

///|
pub struct Size {
  row : Int
  col : Int
} derive(ToJson)

///|
pub fn window_size() -> Size raise {
  let buf : FixedArray[Int] = [0, 0]
  let result = tty_get_win_size(buf)
  if result != 0 {
    raise @errno.Errno(result)
  }
  Size::{ row: buf[0], col: buf[1] }
}

///|
extern "c" fn tty_set_raw_mode(fd : Int) -> Int = "moonbit_moonclaw_tty_set_raw_mode"

///|
pub fn set_raw_mode(fd : Int) -> Unit raise {
  let result = tty_set_raw_mode(fd)
  if result != 0 {
    raise @errno.Errno(result)
  }
}

///|
#cfg(platform="windows")
pub fn set_raw_mode_async(_input : @stdio.Input) -> Unit {
  // On Windows, terminal raw mode is handled by the async runtime
  // No-op for compatibility
}

///|
#cfg(not(platform="windows"))
pub fn set_raw_mode_async(input : @stdio.Input) -> Unit raise {
  // On Unix, we need to get the raw fd and set raw mode
  // Note: This is a simplified version; full implementation would need Fd->Int conversion
  ignore(input)
}
