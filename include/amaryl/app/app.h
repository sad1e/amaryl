#pragma once

#include <SDL3/SDL.h>

#include "amaryl/app/window_metrics.h"
#include "amaryl/core/amaryl.h"

namespace amaryl {

constexpr uint32 kDefaultFPS = 60u;
constexpr uint32 kDefaultWidth = 1280u;
constexpr uint32 kDefaultHeight = 720u;

struct AppInitInfo {
  std::string name{"amaryl"};
  // Initial client area in UI logical units, constrained to the display's usable work
  // area.
  uint32 width{kDefaultWidth};
  uint32 height{kDefaultHeight};
  bool hidden{false};
};

struct App {
  App() = default;
  virtual ~App();
  NO_COPYABLE_AND_MOVEABLE(App);

  virtual bool initialize(const AppInitInfo& info);
  virtual void shutdown();
  virtual void processEvent(const SDL_Event& event);
  virtual bool tick();

  bool resizeWindow(int32 width, int32 height);
  bool getQuitState(const SDL_Event& event) const;
  void requestQuit() { quit_requested_ = true; }
  bool isQuitRequested() const { return quit_requested_; }
  bool isInitialized() const { return initialized_; }

  const std::string& getName() const { return name_; }
  uint32 getWidth() const { return width_; }
  uint32 getHeight() const { return height_; }
  SDL_Window* getWindow() const { return window_; }
  const WindowMetrics& getWindowMetrics() const { return metrics_; }
  virtual uint32 getSceneWidth() const { return width_; }
  virtual uint32 getSceneHeight() const { return height_; }

 protected:
  void setMinimumLogicalSize(float width, float height);
  virtual bool prepareFrame(float delta_time) { return true; }
  virtual bool renderFrame(float delta_time);

 private:
  bool refreshWindowMetrics();
  bool applyMinimumLogicalSize();
  SDL_Point availableWindowSize() const;
  SDL_Window* window_{nullptr};
  std::string name_;
  uint32 width_{kDefaultWidth};
  uint32 height_{kDefaultHeight};
  uint64 last_time_{};
  bool initialized_{false};
  bool quit_requested_{false};
  bool minimized_{false};
  bool resize_pending_{false};
  WindowMetrics metrics_;
  SDL_FPoint minimum_logical_size_{};
};

}  // namespace amaryl
