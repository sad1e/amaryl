#pragma once

#include <string_view>

#include "amaryl/editor/framework/editor_context.h"
#include "amaryl/editor/scene/entity_type.h"

namespace amaryl::editor {

// World editing. The scene shows through the central dock node.
struct EDITOR_UI_API LevelEditorContext final : EditorContext {
  static constexpr std::string_view kId = "level";

  explicit LevelEditorContext(EntityNameLookup lookup_name = {});

  void onAttach(Editor& editor) override;
  void registerActions(ActionRegistry& actions) override;
  void buildDefaultLayout(ImGuiID dockspace_id) override;
  bool hasViewport() const override { return true; }

 private:
  Editor* editor_{nullptr};
  EntityNameLookup lookup_name_;
};

}  // namespace amaryl::editor
