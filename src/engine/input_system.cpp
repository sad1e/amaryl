#include "amaryl/engine/input_system.h"

namespace amaryl {

bool InputSystem::processEvents(const SDL_Event& evt) noexcept {
#define CHECK_WINDOWID                     \
  if (evt.window.windowID != window_id_) { \
    return false;                          \
  } else

  switch (evt.type) {
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_DESTROYED:
      CHECK_WINDOWID {
        pending_ = {};
        pending_.focused = false;
        return true;
      }
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      CHECK_WINDOWID {
        pending_.focused = true;
        return true;
      }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      CHECK_WINDOWID {
        if (!pending_.focused) {
          return true;
        }

        const int code = static_cast<int>(evt.key.scancode);
        if (code < 0 || code >= SDL_SCANCODE_COUNT) {
          return true;
        }

        const uint32 index = static_cast<uint32>(code);
        if (evt.type == SDL_EVENT_KEY_DOWN) {
          if (evt.key.repeat) {
            pending_.keysRepeated.set(index);
          } else if (!pending_.keysDown.test(index)) {
            pending_.keysDown.set(index);
            pending_.keysPressed.set(index);
          }
        } else if (pending_.keysDown.test(index)) {
          pending_.keysDown.reset(index);
          pending_.keysReleased.set(index);
        }

        pending_.lastInputTimestampNs = evt.common.timestamp;
        return true;
      }

    case SDL_EVENT_MOUSE_MOTION:
      CHECK_WINDOWID {
        if (!pending_.focused) {
          return true;
        }

        if (evt.motion.which == SDL_TOUCH_MOUSEID ||
            evt.motion.which == SDL_PEN_MOUSEID) {
          return false;
        }

        pending_.mouseX = evt.motion.x;
        pending_.mouseY = evt.motion.y;
        pending_.mouseDx += evt.motion.xrel;
        pending_.mouseDy += evt.motion.yrel;

        pending_.lastInputTimestampNs = evt.common.timestamp;
        return true;
      }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      CHECK_WINDOWID {
        if (!pending_.focused) {
          return true;
        }

        if (evt.button.which == SDL_TOUCH_MOUSEID ||
            evt.button.which == SDL_PEN_MOUSEID) {
          return false;
        }

        const uint32 button = evt.button.button;
        if (button == 0 || button >= InputFrame::MOUSE_BUTTON_COUNT) {
          return true;
        }

        pending_.mouseX = evt.button.x;
        pending_.mouseY = evt.button.y;

        if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          if (!pending_.mouseDown.test(button)) {
            pending_.mouseDown.set(button);
            pending_.mousePressed.set(button);
          }
        } else if (pending_.mouseDown.test(button)) {
          pending_.mouseDown.reset(button);
          pending_.mouseReleased.set(button);
        }

        pending_.lastInputTimestampNs = evt.common.timestamp;
        return true;
      }

    case SDL_EVENT_MOUSE_WHEEL:
      CHECK_WINDOWID {
        if (!pending_.focused) {
          return true;
        }

        pending_.wheelX += evt.wheel.x;
        pending_.wheelY += evt.wheel.y;
        pending_.mouseX = evt.wheel.mouse_x;
        pending_.mouseY = evt.wheel.mouse_y;

        pending_.lastInputTimestampNs = evt.common.timestamp;
        return true;
      }

    default:
      return false;

#undef CHECK_WINDOWID
  }
}

void InputSystem::beginFrame() noexcept {
  frame_ = pending_;

  pending_.keysPressed.reset();
  pending_.keysReleased.reset();
  pending_.keysRepeated.reset();
  pending_.mousePressed.reset();
  pending_.mouseReleased.reset();

  pending_.mouseDx = 0.0f;
  pending_.mouseDy = 0.0f;
  pending_.wheelX = 0.0f;
  pending_.wheelY = 0.0f;

  pending_.lastInputTimestampNs = 0;
}

}  // namespace amaryl
