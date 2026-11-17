#pragma once

#include <plog/Helpers/HexDump.h>
#include <plog/Helpers/PrintVar.h>
#include <plog/Log.h>

#include "amaryl/core/amaryl.h"

namespace amaryl {

CORE_API void InitializeLogger();

}

#define ENABLE_DEV_LOG 1

// #define LOGGER_FORMAT "[%^%l%$] %v"
#define LOGGER_FORMAT "%v"
#define PROJECT_NAME "amaryl"

// Mainly for IDEs
#ifndef ROOT_PATH_SIZE
#define ROOT_PATH_SIZE 0
#endif

#define __FILENAME__ (static_cast<const char*>(__FILE__) + ROOT_PATH_SIZE)

#define ALOGI(FMT, ...) PLOG_INFO.printf(FMT, __VA_ARGS__)

#define ALOGW(FMT, ...) PLOG_WARNING.printf(FMT, __VA_ARGS__)

#define ALOGE(FMT, ...)                                       \
  PLOG_ERROR.printf("\n\n[%s:%d]\n", __FILENAME__, __LINE__); \
  PLOG_ERROR.printf(FMT, ##__VA_ARGS__)

#define ALOGD(FMT, ...) PLOG_DEBUG.printf(FMT, __VA_ARGS__)

#if ENABLE_DEV_LOG
#define DEV_LOG(FMT, ...) PLOG_INFO.printf(FMT, __VA_ARGS__)
#else
#define DEV_LOG(FMT, ...) ((void)0)
#endif
