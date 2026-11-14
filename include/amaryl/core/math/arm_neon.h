#pragma once

#include "amaryl/core/config.h"

#if AMARYL_SIMD_NEON

#pragma once

#define ARM_NEON_INT32x4(v1, v2, v3, v4) {v1, v2, v3, v4}
#define ARM_NEON_UINT32x4(v1, v2, v3, v4) {v1, v2, v3, v4}

#define ARM_NEON_INT8x16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, \
                         v15, v16)                                                    \
  {v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16}

#define ARM_NEON_UINT8x16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, \
                          v15, v16)                                                    \
  {v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16}

#define ARM_NEON_SHUFFLE_F32x4(vec1, vec2, index1, index2, index3, index4) \
  __builtin_shufflevector(vec1, vec2, index1, index2, index3, index4)

#define ARM_NEON_SHUFFLE_U32x4(vec1, vec2, index1, index2, index3, index4) \
  __builtin_shufflevector(vec1, vec2, index1, index2, index3, index4)

#endif
