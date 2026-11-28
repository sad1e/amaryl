#pragma once

#include "amaryl/editor/framework/panel.h"
#include "amaryl/editor/model/object_registry.h"
#include "amaryl/engine/scene/world.h"

namespace amaryl::editor {

struct Editor;

// The scene graph hierarchy. Click selects, Ctrl+click toggles.
struct EDITOR_UI_API OutlinerPanel final : Panel {
  OutlinerPanel(Editor& editor, const TypeDesc& entity_type);

  void draw() override;

 private:
  // A flat list when `hierarchy` is false.
  void drawNode(TransformHandle transform, bool hierarchy);
  ObjectId getObject(TransformHandle transform) const;
  void select(ObjectId object);

  Editor& editor_;
  const TypeDesc& entity_type_;
  ImGuiTextFilter filter_;
};

}  // namespace amaryl::editor
