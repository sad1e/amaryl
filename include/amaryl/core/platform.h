#pragma once

#include "amaryl/core/config.h"

#if PLATFORM_WINDOWS
#include "amaryl/core/platform/platform_win.h"
#elif PLATFORM_ANDROID
#include "amaryl/core/platform/platform_android.h"
#elif PLATFORM_IOS
#include "amaryl/core/platform/platform_ios.h"
#endif
