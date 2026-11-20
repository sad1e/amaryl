#pragma once

#include <SDL3/SDL.h>

#include <cmath>

namespace amaryl {

// SDL window coordinates, drawable pixels, and UI logical units are distinct coordinate
// spaces.
struct WindowMetrics {
  int window_width{1};
  int window_height{1};
  int pixel_width{1};
  int pixel_height{1};
  float display_scale{1};

  bool operator==(const WindowMetrics&) const = default;

  bool valid() const {
    return window_width > 0 && window_height > 0 && pixel_width > 0 && pixel_height > 0 &&
           std::isfinite(display_scale) && display_scale > 0;
  }

  SDL_FPoint logicalSize(float ui_scale = 1) const {
    const float scale = display_scale * ui_scale;
    return {pixel_width / scale, pixel_height / scale};
  }

  SDL_FPoint windowToLogical(SDL_FPoint point, float ui_scale = 1) const {
    const auto size = logicalSize(ui_scale);
    return {point.x * size.x / window_width, point.y * size.y / window_height};
  }

  SDL_FRect logicalToWindow(SDL_FRect rect, float ui_scale = 1) const {
    const auto size = logicalSize(ui_scale);
    const float sx = window_width / size.x;
    const float sy = window_height / size.y;
    return {rect.x * sx, rect.y * sy, rect.w * sx, rect.h * sy};
  }
};

}  // namespace amaryl
