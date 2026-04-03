#include <moonbit.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>

MOONBIT_FFI_EXPORT
moonbit_bytes_t *
moonbit_moonclaw_backtrace(int32_t n) {
  void **addresses = malloc(sizeof(void *) * n);
  USHORT size = CaptureStackBackTrace(0, (DWORD)n, addresses, NULL);

  moonbit_bytes_t *result =
    (moonbit_bytes_t *)moonbit_make_ref_array(size, NULL);

  HANDLE process = GetCurrentProcess();
  SymInitialize(process, NULL, TRUE);

  for (USHORT i = 0; i < size; i++) {
    DWORD64 address = (DWORD64)(addresses[i]);
    char buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    if (SymFromAddr(process, address, 0, symbol)) {
      result[i] = moonbit_make_bytes(strlen(symbol->Name), 0);
      memcpy(result[i], symbol->Name, strlen(symbol->Name));
    } else {
      char addr_str[32];
      snprintf(addr_str, sizeof(addr_str), "0x%llx", address);
      result[i] = moonbit_make_bytes(strlen(addr_str), 0);
      memcpy(result[i], addr_str, strlen(addr_str));
    }
  }

  free(addresses);
  return result;
}

#else
#include <execinfo.h>

MOONBIT_FFI_EXPORT
moonbit_bytes_t *
moonbit_moonclaw_backtrace(int32_t n) {
  void **addresses = malloc(sizeof(void *) * n);
  int size = backtrace(addresses, n);
  char **symbols = backtrace_symbols(addresses, size);
  moonbit_bytes_t *result =
    (moonbit_bytes_t *)moonbit_make_ref_array(size, NULL);
  for (int i = 0; i < size; i++) {
    result[i] = moonbit_make_bytes(strlen(symbols[i]), 0);
    memcpy(result[i], symbols[i], strlen(symbols[i]));
  }
  free(symbols);
  return result;
}

#endif