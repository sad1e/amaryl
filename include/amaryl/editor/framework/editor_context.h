#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "amaryl/editor/framework/panel.h"

namespace amaryl::editor {

struct ActionRegistry;
struct Editor;

// A discipline specific workspace (Decima's editor context): its panels, its dock layout
// and the actions that are available only while it is active.
struct EDITOR_UI_API EditorContext {
  EditorContext(std::string id, std::string title);
  virtual ~EditorContext();

  NO_COPYABLE_AND_MOVEABLE(EditorContext);

  // Once, when added to the editor. Create the panels here.
  virtual void onAttach(Editor& editor) { (void)editor; }
  // Once, before the editor shuts down.
  virtual void onDetach() {}
  virtual void onActivate() {}
  virtual void onDeactivate() {}
  // Action ids must start with "<context id>.", deactivation removes them.
  virtual void registerActions(ActionRegistry& actions) { (void)actions; }
  // Every frame while active, before the panels.
  virtual void update(float delta_time) { (void)delta_time; }
  // When no layout is saved, and on reset.
  virtual void buildDefaultLayout(ImGuiID dockspace_id) { (void)dockspace_id; }
  // The central dock node stays transparent and shows the scene behind the UI.
  virtual bool hasViewport() const { return false; }

  template <typename T, typename... Args>
  T& addPanel(Args&&... args) {
    auto panel = std::make_unique<T>(std::forward<Args>(args)...);
    T& result = *panel;
    adoptPanel(std::move(panel));
    return result;
  }
  Panel* findPanel(std::string_view id) const;
  const std::vector<std::unique_ptr<Panel>>& getPanels() const { return panels_; }

  const std::string& getId() const { return id_; }
  const std::string& getTitle() const { return title_; }
  ImGuiID getDockspaceId() const { return dockspace_id_; }
  // Screen rectangle of the central dock node in this frame.
  bool getCentralRect(ImVec2& min, ImVec2& max) const;
  void resetLayout() { reset_layout_ = true; }

 private:
  friend struct Editor;

  void adoptPanel(std::unique_ptr<Panel> panel);
  void drawDockspace(bool active);
  void drawPanels();

  std::string id_;
  std::string title_;
  std::vector<std::unique_ptr<Panel>> panels_;
  ImGuiID dockspace_id_{0};
  ImVec2 central_min_{};
  ImVec2 central_max_{};
  bool has_central_{false};
  bool reset_layout_{false};
  // An empty kept alive dock node would prevent the default layout.
  bool was_active_{false};
};

}  // namespace amaryl::editor
