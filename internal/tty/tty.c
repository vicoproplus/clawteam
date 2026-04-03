#include "moonbit.h"
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_tty_is_a_tty(int32_t fd) {
  return _isatty(fd);
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_tty_get_win_size(int32_t *size) {
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdin == INVALID_HANDLE_VALUE) {
    return GetLastError();
  }

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(hStdin, &csbi)) {
    return GetLastError();
  }

  size[0] = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  size[1] = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_tty_set_raw_mode(int32_t fd) {
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdin == INVALID_HANDLE_VALUE) {
    return GetLastError();
  }

  DWORD mode;
  if (!GetConsoleMode(hStdin, &mode)) {
    return GetLastError();
  }

  mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
  mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

  if (!SetConsoleMode(hStdin, mode)) {
    return GetLastError();
  }

  return 0;
}

#else
#include <fcntl.h>
#include <sys/ioctl.h>
#if defined(__APPLE__)
#include <sys/ttycom.h>
#endif
#include <termios.h>
#include <unistd.h>

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_tty_is_a_tty(int32_t fd) {
  return isatty(fd);
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_tty_get_win_size(int32_t *size) {
  struct winsize ws;
  int result = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
  if (result == -1) {
    return errno;
  }
  size[0] = ws.ws_row;
  size[1] = ws.ws_col;
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t
moonbit_moonclaw_tty_set_raw_mode(int32_t fd) {
  int32_t flags = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1) {
    return errno;
  }
  struct termios term;
  if (tcgetattr(fd, &term) == -1) {
    return errno;
  }
  term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  term.c_oflag |= (ONLCR);
  term.c_cflag |= (CS8);
  term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  term.c_cc[VMIN] = 1;
  term.c_cc[VTIME] = 0;
  if (tcsetattr(fd, TCSADRAIN, &term) == -1) {
    return errno;
  }
  return 0;
}

#endif