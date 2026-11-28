#include "amaryl/editor/level/outliner_panel.h"

#include <cstdio>
#include <string>

#include "amaryl/editor/framework/editor.h"

namespace amaryl::editor {

OutlinerPanel::OutlinerPanel(Editor& editor, const TypeDesc& entity_type)
    : Panel("outliner", "Outliner"), editor_(editor), entity_type_(entity_type) {}

void OutlinerPanel::draw() {
  filter_.Draw("##filter", -FLT_MIN);

  if (ImGui::BeginChild("##tree")) {
    const SceneGraph& graph = editor_.getWorld().graph;
    for (const TransformHandle transform : graph.getDenseHandles()) {
      if (filter_.IsActive()) {
        drawNode(transform, false);
      } else if (!graph.getParent(transform).valid()) {
        drawNode(transform, true);
      }
    }
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsAnyItemHovered()) {
      editor_.getSelection().clear();
    }
  }
  ImGui::EndChild();
}

void OutlinerPanel::drawNode(TransformHandle transform, bool hierarchy) {
  const SceneGraph& graph = editor_.getWorld().graph;
  const ObjectId object = getObject(transform);

  std::string name;
  if (object != kInvalidObjectId) {
    name = editor_.getObjects().getDisplayName(object);
  } else {
    char buffer[32];
    std::snprintf(
      buffer, sizeof(buffer), "Transform %u", static_cast<unsigned>(transform.index()));
    name = buffer;
  }
  if (!hierarchy && !filter_.PassFilter(name.c_str())) {
    return;
  }

  const TransformHandle first_child = graph.getFirstChild(transform);
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
    ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
  if (!hierarchy || !first_child.valid()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }
  if (object != kInvalidObjectId && editor_.getSelection().contains(object)) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  const bool open = ImGui::TreeNodeEx(transform.handleAsVoid(), flags, "%s", name.c_str());
  if (object != kInvalidObjectId && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    select(object);
  }
  if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
    for (TransformHandle child = first_child; child.valid();
         child = graph.getNextSibling(child)) {
      drawNode(child, true);
    }
    ImGui::TreePop();
  }
}

ObjectId OutlinerPanel::getObject(TransformHandle transform) const {
  const EntityHandle entity = editor_.getWorld().entities.findByTransform(transform);
  return entity.valid() ? editor_.getObjects().getOrAdd(entity_type_, ToNative(entity))
                        : kInvalidObjectId;
}

void OutlinerPanel::select(ObjectId object) {
  Selection& selection = editor_.getSelection();
  if (ImGui::GetIO().KeyCtrl) {
    selection.toggle(object);
  } else {
    selection.set(object);
  }
}

}  // namespace amaryl::editor
