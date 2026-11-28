#include "amaryl/editor/framework/editor_context.h"

#include <imgui_internal.h>

namespace amaryl::editor {

EditorContext::EditorContext(std::string id, std::string title)
    : id_(std::move(id)), title_(std::move(title)) {}

EditorContext::~EditorContext() = default;

void EditorContext::adoptPanel(std::unique_ptr<Panel> panel) {
  panel->window_name_ = panel->title_ + "###" + id_ + "." + panel->id_;
  panels_.push_back(std::move(panel));
}

Panel* EditorContext::findPanel(std::string_view id) const {
  for (const std::unique_ptr<Panel>& panel : panels_) {
    if (panel->getId() == id) {
      return panel.get();
    }
  }
  return nullptr;
}

bool EditorContext::getCentralRect(ImVec2& min, ImVec2& max) const {
  min = central_min_;
  max = central_max_;
  return has_central_;
}

void EditorContext::drawDockspace(bool active) {
  dockspace_id_ = ImGui::GetID(id_.c_str());
  has_central_ = false;
  if (!active) {
    if (was_active_) {
      ImGui::DockSpace(dockspace_id_, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly);
    }
    return;
  }
  was_active_ = true;

  if (reset_layout_ || !ImGui::DockBuilderGetNode(dockspace_id_)) {
    reset_layout_ = false;
    for (std::unique_ptr<Panel>& panel : panels_) {
      panel->open_ = panel->open_by_default_;
    }
    ImGui::DockBuilderRemoveNode(dockspace_id_);
    ImGui::DockBuilderAddNode(dockspace_id_, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id_, ImGui::GetContentRegionAvail());
    buildDefaultLayout(dockspace_id_);
    ImGui::DockBuilderFinish(dockspace_id_);
  }

  ImGui::DockSpace(dockspace_id_, ImVec2(0.0f, 0.0f),
    hasViewport() ? ImGuiDockNodeFlags_PassthruCentralNode : ImGuiDockNodeFlags_None);

  if (const ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspace_id_)) {
    central_min_ = central->Pos;
    central_max_ = ImVec2(central->Pos.x + central->Size.x, central->Pos.y + central->Size.y);
    has_central_ = true;
  }
}

void EditorContext::drawPanels() {
  for (std::unique_ptr<Panel>& panel : panels_) {
    if (!panel->open_) {
      continue;
    }
    if (ImGui::Begin(panel->getWindowName(), &panel->open_, panel->getWindowFlags())) {
      panel->draw();
    }
    ImGui::End();
  }
}

}  // namespace amaryl::editor
