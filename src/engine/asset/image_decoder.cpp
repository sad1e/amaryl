#include "amaryl/engine/asset/image_decoder.h"

#include <limits>

#include "stb_image.h"

namespace amaryl {

bool decodeImageRGBA8(Span<const uint8> encoded, DecodedImage& out) {
  if (encoded.empty() || encoded.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* data = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()),
                                        &width, &height, &channels, 4);
  if (!data) {
    return false;
  }

  out.width = static_cast<uint32>(width);
  out.height = static_cast<uint32>(height);
  out.pixels.assign(data, data + static_cast<size_t>(width) * height * 4);
  stbi_image_free(data);
  return true;
}

}  // namespace amaryl
