#pragma once

#include "amaryl/core/assert.h"
#include "amaryl/core/bit.h"
#include "amaryl/core/config.h"
#include "amaryl/core/logger.h"
#include "amaryl/core/macros.h"
#include "amaryl/core/scope_exit.h"
#include "amaryl/core/templates.h"
#include "amaryl/core/types.h"

#if AMARYL_USE_SSE
#include <immintrin.h>
#endif
