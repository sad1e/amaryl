#include <algorithm>
#include <iterator>
#include <vector>

#include "amaryl/engine/asset/image_decoder.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

void put32(std::vector<uint8>& out, size_t offset, uint32 value) {
  for (uint32 i = 0; i < 4; ++i) {
    out[offset + i] = static_cast<uint8>(value >> (8 * i));
  }
}

// 2x2 24-bit BMP, rows are stored bottom-up in BGR order and padded to 4 bytes.
std::vector<uint8> makeBmp() {
  constexpr uint32 kHeaderSize = 54;
  constexpr uint32 kRowSize = 8;
  std::vector<uint8> bmp(kHeaderSize + 2 * kRowSize, 0);
  bmp[0] = 'B';
  bmp[1] = 'M';
  put32(bmp, 2, static_cast<uint32>(bmp.size()));
  put32(bmp, 10, kHeaderSize);
  put32(bmp, 14, 40);
  put32(bmp, 18, 2);
  put32(bmp, 22, 2);
  bmp[26] = 1;
  bmp[28] = 24;

  // Bottom row: blue, white. Top row: red, green.
  const uint8 bottom[] = {255, 0, 0, 255, 255, 255};
  const uint8 top[] = {0, 0, 255, 0, 255, 0};
  std::copy(std::begin(bottom), std::end(bottom), bmp.begin() + kHeaderSize);
  std::copy(std::begin(top), std::end(top), bmp.begin() + kHeaderSize + kRowSize);
  return bmp;
}

}  // namespace

int main() {
  "decodes to top-down RGBA8"_test = [] {
    DecodedImage image;
    expect(decodeImageRGBA8(makeBmp(), image));
    expect(image.width == 2u && image.height == 2u && image.pixels.size() == 16u);
    const std::vector<uint8> expected = {
        255, 0,   0,   255, 0,   255, 0,   255,  // red, green
        0,   0,   255, 255, 255, 255, 255, 255,  // blue, white
    };
    expect(image.pixels == expected);
  };

  "rejects invalid data"_test = [] {
    DecodedImage image;
    image.width = 7;
    const std::vector<uint8> garbage = {1, 2, 3, 4};
    expect(!decodeImageRGBA8(garbage, image));
    expect(!decodeImageRGBA8({}, image));
    expect(image.width == 7u && image.pixels.empty());
  };

  return 0;
}
