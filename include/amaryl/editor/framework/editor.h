#pragma once

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "amaryl/editor/framework/action.h"
#include "amaryl/editor/framework/editor_context.h"
#include "amaryl/editor/model/history.h"
#include "amaryl/editor/model/selection.h"

namespace amaryl {
struct World;
}  // namespace amaryl

namespace amaryl::editor {

// Owns the data model that the contexts share, and runs the UI frame. One document for
// now: one history and one selection for all contexts.
struct EDITOR_UI_API Editor final {
  explicit Editor(World& world);
  ~Editor();

  NO_COPYABLE_AND_MOVEABLE(Editor);

  // Context ids must be unique. The first context becomes active.
  template <typename T, typename... Args>
  T& addContext(Args&&... args) {
    auto context = std::make_unique<T>(std::forward<Args>(args)...);
    T& result = *context;
    adoptContext(std::move(context));
    return result;
  }
  // Takes effect at the start of the next frame.
  void activate(std::string_view context_id);
  EditorContext* getActiveContext() const { return active_; }

  // Between ImGui::NewFrame() and ImGui::Render().
  void drawFrame(float delta_time);
  void shutdown();

  World& getWorld() const { return world_; }
  TypeRegistry& getTypes() { return types_; }
  ObjectRegistry& getObjects() { return objects_; }
  History& getHistory() { return history_; }
  Selection& getSelection() { return selection_; }
  ActionRegistry& getActions() { return actions_; }

 private:
  void adoptContext(std::unique_ptr<EditorContext> context);
  void applyPendingContext();
  void registerActions();
  void registerPanelActions(EditorContext& context);
  void drawMainMenu();
  void drawDockspaces();

  World& world_;
  TypeRegistry types_;
  ObjectRegistry objects_;
  History history_{objects_};
  Selection selection_;
  ActionRegistry actions_;
  std::vector<std::unique_ptr<EditorContext>> contexts_;
  EditorContext* active_{nullptr};
  EditorContext* pending_{nullptr};
};

}  // namespace amaryl::editor
