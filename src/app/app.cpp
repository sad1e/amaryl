#include "amaryl/app/app.h"

#include <algorithm>

namespace amaryl {

namespace {

inline bool ShowErrorMessageBox(const char* title, const std::string& message,
                                SDL_Window* parent = nullptr) {
  return SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), parent);
}

}  // namespace

App::~App() { App::shutdown(); }

bool App::initialize(const AppInitInfo& info) {
  if (initialized_) {
    return true;
  }
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    ShowErrorMessageBox("b2 initialization failed", SDL_GetError());
    return false;
  }

  name_ = info.name;
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
                          SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
  window_ = SDL_CreateWindow(name_.c_str(), static_cast<int>(info.width),
                             static_cast<int>(info.height), flags);
  if (!window_) {
    ShowErrorMessageBox("b2 initialization failed", SDL_GetError());
    return false;
  }

  if (!refreshWindowMetrics()) {
    return false;
  }
  const auto requested =
      metrics_.logicalToWindow({0, 0, float(info.width), float(info.height)});
  const auto available = availableWindowSize();
  if (!SDL_SetWindowSize(
          window_, int(std::clamp(std::round(requested.w), 1.0f, float(available.x))),
          int(std::clamp(std::round(requested.h), 1.0f, float(available.y))))) {
    return false;
  }
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  if (!info.hidden) {
    SDL_ShowWindow(window_);
  }
  SDL_SyncWindow(window_);
  if (!refreshWindowMetrics()) {
    return false;
  }
  width_ = static_cast<uint32>(metrics_.pixel_width);
  height_ = static_cast<uint32>(metrics_.pixel_height);
  // InitializeEngine(window_);

  initialized_ = true;
  quit_requested_ = false;
  minimized_ = false;
  resize_pending_ = false;
  last_time_ = SDL_GetPerformanceCounter();
  return true;
}

void App::shutdown() {
  // if (GEngine) {
  //   if (initialized_) {
  //     GEngine->getRHI()->wait({});
  //   }
  //   ShutdownEngine();
  // }

  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  initialized_ = false;
  metrics_ = {};
}

bool App::refreshWindowMetrics() {
  WindowMetrics next = metrics_;
  if (!SDL_GetWindowSize(window_, &next.window_width, &next.window_height) ||
      !SDL_GetWindowSizeInPixels(window_, &next.pixel_width, &next.pixel_height)) {
    return false;
  }
  const float scale = SDL_GetWindowDisplayScale(window_);
  if (std::isfinite(scale) && scale > 0) {
    next.display_scale = scale;
  }
  if (!next.valid()) {
    return false;
  }
  metrics_ = next;
  return true;
}

SDL_Point App::availableWindowSize() const {
  SDL_Rect bounds{};
  if (!SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window_), &bounds)) {
    return {metrics_.window_width, metrics_.window_height};
  }
  int top = 0, left = 0, bottom = 0, right = 0;
  SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right);
  return {std::max(1, bounds.w - left - right - 16),
          std::max(1, bounds.h - top - bottom - 16)};
}

void App::setMinimumLogicalSize(float width, float height) {
  minimum_logical_size_ = {width, height};
  resize_pending_ = true;
}

bool App::applyMinimumLogicalSize() {
  if (minimum_logical_size_.x <= 0 || minimum_logical_size_.y <= 0) {
    return false;
  }
  const auto minimum =
      metrics_.logicalToWindow({0, 0, minimum_logical_size_.x, minimum_logical_size_.y});
  const auto available = availableWindowSize();
  const int w = int(std::clamp(std::ceil(minimum.w), 1.0f, float(available.x)));
  const int h = int(std::clamp(std::ceil(minimum.h), 1.0f, float(available.y)));
  int current_w = 0, current_h = 0;
  if (SDL_GetWindowMinimumSize(window_, &current_w, &current_h) &&
      (w != current_w || h != current_h)) {
    return SDL_SetWindowMinimumSize(window_, w, h);
  }
  return false;
}

bool App::resizeWindow(int32 width, int32 height) {
  if (width <= 0 || height <= 0 || !initialized_) {
    return false;
  }
  if (width_ == static_cast<uint32>(width) && height_ == static_cast<uint32>(height)) {
    return true;
  }
  width_ = static_cast<uint32>(width);
  height_ = static_cast<uint32>(height);

  // GEngine->processResize(width, height);

  return true;
}

void App::processEvent(const SDL_Event& event) {
  if (getQuitState(event)) {
    requestQuit();
  }
  if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST &&
      event.window.windowID == SDL_GetWindowID(window_)) {
    switch (event.type) {
      case SDL_EVENT_WINDOW_MINIMIZED:
        minimized_ = true;
        break;
      case SDL_EVENT_WINDOW_RESTORED:
      case SDL_EVENT_WINDOW_MAXIMIZED:
        minimized_ = false;
        resize_pending_ = true;
        break;
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        resize_pending_ = true;
        break;
      default:
        break;
    }
  }
}

bool App::tick() {
  if (!initialized_ || quit_requested_) {
    return true;
  }
  if (minimized_) {
    SDL_Delay(16);
    last_time_ = SDL_GetPerformanceCounter();
    return true;
  }
  if (resize_pending_) {
    if (!refreshWindowMetrics()) {
      SDL_Delay(16);
      return true;
    }
    if (applyMinimumLogicalSize()) {
      SDL_SyncWindow(window_);
      if (!refreshWindowMetrics()) {
        return true;
      }
    }
    resizeWindow(metrics_.pixel_width, metrics_.pixel_height);
    resize_pending_ = false;
  }

  const uint64 start = SDL_GetPerformanceCounter();
  const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  const float delta_time =
      std::min(static_cast<float>((start - last_time_) / frequency), 0.1f);
  last_time_ = start;

  // GEngine->beginFrame();
  // if (!prepareFrame(delta_time)) {
  //   GEngine->endFrame();
  //   return false;
  // }
  // GEngine->tick(this, delta_time);
  // if (!renderFrame(delta_time)) {
  //   GEngine->endFrame();
  //   return false;
  // }
  // GEngine->endFrame();

  const double elapsed = (SDL_GetPerformanceCounter() - start) / frequency;
  if (elapsed < 1.0 / kDefaultFPS) {
    SDL_Delay(static_cast<uint32>((1.0 / kDefaultFPS - elapsed) * 1000.0));
  }
  return true;
}

bool App::renderFrame(float delta_time) {
  // IContext& context = *GEngine->getRHI();
  // const TextureHandle target = context.getCurrentSwapchainTexture();
  // ICommandBuffer& commands = context.acquireCommandBuffer();
  // commands.cmdBeginRendering(
  //     {.color = {{.loadOp = ELoadOp::Clear, .clearColor = {0.06f, 0.07f,
  //     0.09f, 1.0f}}}},
  //     {.color = {{.texture = target}}});
  // commands.cmdEndRendering();
  // context.submit(commands, target);

  return true;
}

bool App::getQuitState(const SDL_Event& event) const {
  return event.type == SDL_EVENT_QUIT ||
         (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          event.window.windowID == SDL_GetWindowID(window_));
}

}  // namespace amaryl
