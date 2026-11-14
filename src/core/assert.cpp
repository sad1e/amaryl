#include "amaryl/core/assert.h"

#include <DbgHelp.h>
#include <tchar.h>

#include <cstdio>

namespace amaryl {

void OutputDebugStringFormat(const wchar_t* format, ...) {
  va_list args;
  SYSTEMTIME time;
  TCHAR tmp_buf[1920];
  TCHAR msg_bug[2048];

  GetLocalTime(&time);

  va_start(args, format);
  _vstprintf_s(tmp_buf, format, args);
  _stprintf_s(msg_bug, TEXT("[%02d:%02d:%02d DEBUG] %s"), time.wHour, time.wMinute,
              time.wSecond, tmp_buf);
  OutputDebugString(msg_bug);
  va_end(args);
}

void PrintStack() {
  constexpr size_t frames_to_capture = 100u;
  uint32 i;
  void* stack_frames[frames_to_capture];
  HANDLE process = GetCurrentProcess();

  SymInitialize(process, nullptr, TRUE);

  uint32 frames_count =
      CaptureStackBackTrace(0, frames_to_capture, stack_frames, nullptr);
  SYMBOL_INFO* symbol =
      (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + (MAX_PATH) * sizeof(char), 1u);
  symbol->MaxNameLen = MAX_PATH;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  amaryl::Printf("Crash callstack: \n");
  for (uint32 i = 0; i < frames_count; ++i) {
    SymFromAddr(process, (DWORD64)(stack_frames[i]), nullptr, symbol);
    amaryl::Printf("\t%3i[0x%0X]: %s\n", frames_count - i - 1, symbol->Address,
                   symbol->Name);
  }

  free(symbol);
}

}  // namespace amaryl
