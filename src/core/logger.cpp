#include "amaryl/core/logger.h"

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/DebugOutputAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

namespace amaryl {

void InitializeLogger() {
  static plog::ColorConsoleAppender<plog::TxtFormatter> s_console_adapter;
  static plog::DebugOutputAppender<plog::TxtFormatter> s_debug_output_adapter;

  plog::init(plog::verbose, &s_console_adapter).addAppender(&s_debug_output_adapter);
}

}  // namespace amaryl
