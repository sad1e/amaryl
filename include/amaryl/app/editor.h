#pragma once

#include <optional>

#include "amaryl/app/app.h"
#include "amaryl/engine/project/project.h"

namespace amaryl {

struct EditorApp : App {
  // Mounted as project:// and cache:// on initialize, the editor runs without project
  // when empty.
  std::optional<ProjectInfo> project;

  bool initialize(const AppInitInfo& info) override;
  void shutdown() override;
  void processEvent(const SDL_Event& event) override;

 protected:
  bool mountFileSystem() override;
  bool prepareFrame(float delta_time) override;
  bool renderFrame(float delta_time) override;

 private:
  bool initImGui();
  void shutdownImGui();
  void drawMenubar();

  bool imgui_initialized_{false};
  bool show_demo_window_{false};
};

}  // namespace amaryl
