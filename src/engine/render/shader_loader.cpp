#include "amaryl/engine/render/shader_loader.h"

#include "amaryl/core/filesystem/vfs.h"

namespace amaryl {

bool ShaderLoader::readBinaryFile(
  const std::string& path, std::vector<uint8>& out_bytes) {
  return GetVirtualFileSystem().readFile(path, out_bytes) && !out_bytes.empty();
}

Holder<ShaderModuleHandle> ShaderLoader::loadShaderModule(IContext* ctx,
  const std::string& path, EShaderStage stage, const char* debug_name,
  Result* out_result) {
  std::vector<uint8> bytes;
  if (!readBinaryFile(path, bytes)) {
    ALOGE("Failed to read the shader '%s'", path.c_str());
    Result::setResult(
      out_result, Result::Code::RuntimeError, "Failed to read shader file");
    return {};
  }

  return ctx->createShaderModule(
    ShaderModuleDesc(bytes.data(), bytes.size(), stage, debug_name), out_result);
}

}  // namespace amaryl
