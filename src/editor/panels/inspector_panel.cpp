#include "amaryl/editor/panels/inspector_panel.h"

#include <algorithm>

#include "amaryl/editor/framework/editor.h"

namespace amaryl::editor {

InspectorPanel::InspectorPanel(Editor& editor)
    : Panel("inspector", "Inspector"), editor_(editor) {}

void InspectorPanel::draw() {
  const Selection& selection = editor_.getSelection();
  const ObjectRegistry& objects = editor_.getObjects();
  const ObjectId object = selection.getPrimary();
  if (!objects.isAlive(object)) {
    edit_.reset();
    ImGui::TextDisabled("Nothing selected");
    return;
  }
  if (edit_ && edit_->object != object) {
    edit_.reset();
  }

  const TypeDesc* type = objects.getType(object);
  const NativeHandle native = objects.getNative(object);
  ImGui::TextUnformatted(objects.getDisplayName(object).c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", type->name.c_str());
  if (selection.get().size() > 1) {
    ImGui::TextDisabled("%zu objects selected, editing the last one", selection.get().size());
  }
  ImGui::Separator();

  if (ImGui::BeginTable(
        "##properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn(
      "Name", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    for (const PropertyDesc& property : type->properties) {
      if (property.get && !EnumHasAnyFlags(property.flags, EPropertyFlags::Hidden)) {
        drawProperty(object, native, property);
      }
    }
    ImGui::EndTable();
  }
}

void InspectorPanel::drawProperty(
  ObjectId object, NativeHandle native, const PropertyDesc& property) {
  const auto is_edited = [&] {
    return edit_ && edit_->object == object && edit_->property == property.id;
  };

  const PropertyValue before = property.get(native);
  PropertyValue value = before;
  Vec3 euler_degrees = is_edited() ? edit_->euler_degrees : Vec3::sZero();

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(property.name.c_str());
  ImGui::TableNextColumn();
  ImGui::PushID(static_cast<int>(property.id));
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::BeginDisabled(!property.canWrite(native));
  const bool changed = DrawValue(property, value, is_edited(), euler_degrees);
  ImGui::EndDisabled();
  ImGui::PopID();

  if (ImGui::IsItemActivated()) {
    edit_ = ActiveEdit{object, property.id, before, euler_degrees};
  } else if (is_edited()) {
    edit_->euler_degrees = euler_degrees;
  }
  if (changed) {
    property.set(native, value);
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    const PropertyValue original = is_edited() ? edit_->before : before;
    PropertyValue after = property.get(native);
    if (!(after == original)) {
      Transaction transaction{"Set " + property.name, {}, 0};
      transaction.commands.push_back({.kind = ECommandKind::SetProperty,
        .object = object,
        .property = property.id,
        .before = original,
        .after = std::move(after)});
      editor_.getHistory().record(std::move(transaction));
    }
    edit_.reset();
  } else if (ImGui::IsItemDeactivated()) {
    edit_.reset();
  }
}

bool InspectorPanel::DrawValue(
  const PropertyDesc& property, PropertyValue& value, bool use_euler, Vec3& euler_degrees) {
  const PropertyHints& hints = property.hints;
  if (bool* bool_value = std::get_if<bool>(&value)) {
    return ImGui::Checkbox("##value", bool_value);
  }
  if (int32* int_value = std::get_if<int32>(&value)) {
    return ImGui::DragInt("##value", int_value, std::max(hints.speed, 0.1f),
      static_cast<int>(hints.min), static_cast<int>(hints.max));
  }
  if (float* float_value = std::get_if<float>(&value)) {
    return ImGui::DragFloat("##value", float_value, hints.speed, hints.min, hints.max);
  }
  if (Vec3* vector_value = std::get_if<Vec3>(&value)) {
    float xyz[3] = {vector_value->GetX(), vector_value->GetY(), vector_value->GetZ()};
    const bool changed = hints.widget == EPropertyWidget::Color
      ? ImGui::ColorEdit3("##value", xyz, ImGuiColorEditFlags_Float)
      : ImGui::DragFloat3("##value", xyz, hints.speed, hints.min, hints.max);
    if (changed) {
      *vector_value = Vec3(xyz[0], xyz[1], xyz[2]);
    }
    return changed;
  }
  if (Quat* rotation_value = std::get_if<Quat>(&value)) {
    if (!use_euler) {
      euler_degrees = rotation_value->GetEulerAngles() * RAD_TO_DEG;
    }
    float xyz[3] = {euler_degrees.GetX(), euler_degrees.GetY(), euler_degrees.GetZ()};
    if (!ImGui::DragFloat3("##value", xyz, hints.speed, 0.0f, 0.0f, "%.2f")) {
      return false;
    }
    euler_degrees = Vec3(xyz[0], xyz[1], xyz[2]);
    *rotation_value = Quat::sEulerAngles(euler_degrees * DEG_TO_RAD);
    return true;
  }
  if (std::string* string_value = std::get_if<std::string>(&value)) {
    char buffer[256];
    const size_t size = std::min(string_value->size(), sizeof(buffer) - 1);
    string_value->copy(buffer, size);
    buffer[size] = '\0';
    if (!ImGui::InputText("##value", buffer, sizeof(buffer))) {
      return false;
    }
    *string_value = buffer;
    return true;
  }
  return false;
}

}  // namespace amaryl::editor
