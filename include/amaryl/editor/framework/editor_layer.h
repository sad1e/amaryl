#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <functional>
#include <memory>

#include "amaryl/editor/framework/common.h"

namespace amaryl {
struct EditorScene;
}  // namespace amaryl

namespace amaryl::editor {

struct Editor;

struct EditorLayerDesc {
  SDL_Window* window{nullptr};
  float display_scale{1.0f};
  // Called by the "File/Exit" action.
  std::function<void()> request_quit;
};

// The editor part of the application: the ImGui context and backends, the editor
// framework and the editor scene. The application owns the main loop and calls the layer.
struct EDITOR_UI_API EditorLayer final {
  EditorLayer();
  ~EditorLayer();

  NO_COPYABLE_AND_MOVEABLE(EditorLayer);

  // The engine must be initialized.
  bool init(const EditorLayerDesc& desc);
  // Must be called before the engine shuts down. Safe to call twice.
  void shutdown();

  void processEvent(const SDL_Event& event);
  // Builds the UI of the frame, before Engine::update().
  void buildFrame(float delta_time);
  // Valid after buildFrame() until the next buildFrame(), null when not initialized.
  ImDrawData* getDrawData() const;

 private:
  bool initImGui(const EditorLayerDesc& desc);
  void shutdownImGui();
  void initEditor();

  std::function<void()> request_quit_;
  std::unique_ptr<EditorScene> scene_;
  std::unique_ptr<Editor> editor_;
  bool imgui_initialized_{false};
};

}  // namespace amaryl::editor
