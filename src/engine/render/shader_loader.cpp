#include "amaryl/engine/render/shader_loader.h"

#include <fstream>

namespace amaryl {

bool ShaderLoader::readBinaryFile(
  const std::string& path, std::vector<uint8>& out_bytes) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }

  const std::streamsize size = file.tellg();
  if (size <= 0) {
    return false;
  }

  out_bytes.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(out_bytes.data()), size);

  return file.good() || file.eof();
}

Holder<ShaderModuleHandle> ShaderLoader::loadShaderModule(IContext* ctx,
  const std::string& path, EShaderStage stage, const char* debug_name,
  Result* out_result) {
  std::vector<uint8> bytes;
  if (!readBinaryFile(path, bytes)) {
    Result::setResult(
      out_result, Result::Code::RuntimeError, "Failed to read shader file");
    return {};
  }

  return ctx->createShaderModule(
    ShaderModuleDesc(bytes.data(), bytes.size(), stage, debug_name), out_result);
}

}  // namespace amaryl
