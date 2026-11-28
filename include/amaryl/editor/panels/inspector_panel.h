#pragma once

#include <optional>

#include "amaryl/editor/framework/panel.h"
#include "amaryl/editor/model/object_registry.h"

namespace amaryl::editor {

struct Editor;

// Edits the properties of the primary selection through its TypeDesc. A widget
// interaction is applied live and recorded as one transaction when it ends.
struct EDITOR_UI_API InspectorPanel final : Panel {
  explicit InspectorPanel(Editor& editor);

  void draw() override;

 private:
  struct ActiveEdit {
    ObjectId object{kInvalidObjectId};
    uint32 property{0};
    PropertyValue before{};
    // Conversion from the quaternion in each frame would flip the angles.
    Vec3 euler_degrees{Vec3::sZero()};
  };

  void drawProperty(ObjectId object, NativeHandle native, const PropertyDesc& property);
  static bool DrawValue(
    const PropertyDesc& property, PropertyValue& value, bool use_euler, Vec3& euler_degrees);

  Editor& editor_;
  std::optional<ActiveEdit> edit_;
};

}  // namespace amaryl::editor
