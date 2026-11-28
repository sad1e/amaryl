#include "amaryl/editor/level/level_context.h"

#include <imgui_internal.h>

#include "amaryl/editor/framework/editor.h"
#include "amaryl/editor/level/outliner_panel.h"
#include "amaryl/editor/panels/history_panel.h"
#include "amaryl/editor/panels/inspector_panel.h"

namespace amaryl::editor {

LevelEditorContext::LevelEditorContext(EntityNameLookup lookup_name)
    : EditorContext(std::string(kId), "Level"), lookup_name_(std::move(lookup_name)) {}

void LevelEditorContext::onAttach(Editor& editor) {
  editor_ = &editor;
  const TypeDesc* entity_type = editor.getTypes().find(kEntityTypeName);
  if (!entity_type) {
    entity_type = &RegisterEntityType(editor.getTypes(), editor.getWorld(), lookup_name_);
  }
  addPanel<OutlinerPanel>(editor, *entity_type);
  addPanel<InspectorPanel>(editor);
  addPanel<HistoryPanel>(editor.getHistory());
}

void LevelEditorContext::registerActions(ActionRegistry& actions) {
  actions.add({.id = getId() + ".select_none",
    .label = "Select None",
    .menu = "Edit",
    .order = 100,
    .shortcut = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A,
    .execute = [this] { editor_->getSelection().clear(); },
    .isEnabled = [this] { return !editor_->getSelection().empty(); }});
}

void LevelEditorContext::buildDefaultLayout(ImGuiID dockspace_id) {
  ImGuiID center = dockspace_id;
  ImGuiID right_top =
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
  const ImGuiID right_bottom =
    ImGui::DockBuilderSplitNode(right_top, ImGuiDir_Down, 0.6f, nullptr, &right_top);
  const ImGuiID bottom =
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

  ImGui::DockBuilderDockWindow(findPanel("outliner")->getWindowName(), right_top);
  ImGui::DockBuilderDockWindow(findPanel("inspector")->getWindowName(), right_bottom);
  ImGui::DockBuilderDockWindow(findPanel("history")->getWindowName(), bottom);
}

}  // namespace amaryl::editor
