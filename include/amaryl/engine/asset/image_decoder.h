#pragma once

#include <vector>

#include "amaryl/core/span.h"
#include "amaryl/engine/common.h"

namespace amaryl {

struct DecodedImage {
  uint32 width = 0;
  uint32 height = 0;
  // Tightly packed RGBA8 rows, top row first.
  std::vector<uint8> pixels;
};

// Decodes a PNG, JPEG, TGA, BMP, PSD, GIF, HDR or PNM file in memory to RGBA8, HDR images
// are tone mapped. Returns false and leaves `out` untouched on failure.
ENGINE_API bool decodeImageRGBA8(Span<const uint8> encoded, DecodedImage& out);

}  // namespace amaryl
