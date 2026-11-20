#pragma once

#include <SDL3/SDL.h>

#include <bitset>

#include "amaryl/engine/engine.h"

namespace amaryl {

struct InputFrame {
  // SDL mouse button number start from 1, the 0 is unused.
  static constexpr uint16 MOUSE_BUTTON_COUNT = 33;

  std::bitset<MOUSE_BUTTON_COUNT> mouseDown;
  std::bitset<MOUSE_BUTTON_COUNT> mousePressed;
  std::bitset<MOUSE_BUTTON_COUNT> mouseReleased;

  std::bitset<SDL_SCANCODE_COUNT> keysDown;
  std::bitset<SDL_SCANCODE_COUNT> keysPressed;
  std::bitset<SDL_SCANCODE_COUNT> keysReleased;
  std::bitset<SDL_SCANCODE_COUNT> keysRepeated;

  float mouseX{0.0f};
  float mouseY{0.0f};
  float mouseDx{0.0f};
  float mouseDy{0.0f};
  float wheelX{0.0f};
  float wheelY{0.0f};

  uint64 lastInputTimestampNs{0u};
  bool focused{true};
};

struct InputSystem {
  explicit InputSystem(SDL_WindowID window_id) noexcept : window_id_(window_id) {}

  [[nodiscard]] ENGINE_API bool processEvents(const SDL_Event& evt) noexcept;

  ENGINE_API void beginFrame() noexcept;

  [[nodiscard]] ENGINE_API const InputFrame& frame() const noexcept { return frame_; }

 private:
  SDL_WindowID window_id_{0};
  InputFrame pending_{};
  InputFrame frame_{};
};

}  // namespace amaryl
