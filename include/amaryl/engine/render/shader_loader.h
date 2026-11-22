#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "amaryl/engine/common.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

// Minimal helpers for loading precompiled SPIR-V shader modules produced by the
// `amaryl-shaders` CMake target (see `cmakes/amaryl-shaders.cmake`).
struct ENGINE_API ShaderLoader {
  // Reads the whole file at `path` into `out_bytes`. Returns false on failure.
  static bool readBinaryFile(const std::string& path, std::vector<uint8>& out_bytes);

  // Reads the SPIR-V module at `path` and creates a shader module. Returns an empty
  // holder and sets `out_result` on failure.
  static Holder<ShaderModuleHandle> loadShaderModule(IContext* ctx,
                                                     const std::string& path,
                                                     EShaderStage stage,
                                                     const char* debug_name,
                                                     Result* out_result = nullptr);
};

}  // namespace amaryl
