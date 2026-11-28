#include "amaryl/editor/panels/history_panel.h"

#include <optional>

namespace amaryl::editor {

HistoryPanel::HistoryPanel(History& history) : Panel("history", "History"), history_(history) {
  listener_ = history_.onChanged.connect(
    [this](const Transaction&, EHistoryEvent) { scroll_to_current_ = true; });
}

HistoryPanel::~HistoryPanel() { history_.onChanged.disconnect(listener_); }

void HistoryPanel::draw() {
  ImGui::BeginDisabled(!history_.canUndo());
  if (ImGui::Button("Undo")) {
    history_.undo();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!history_.canRedo());
  if (ImGui::Button("Redo")) {
    history_.redo();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled(history_.isDirty() ? "Modified" : "Saved");
  ImGui::Separator();

  std::optional<size_t> jump_target;
  if (ImGui::BeginChild("##entries")) {
    const Span<const Transaction> entries = history_.getEntries();
    const size_t applied = history_.getAppliedCount();
    const ImVec4 redo_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    // Row 0 is the state before the first entry.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(entries.size() + 1));
    if (scroll_to_current_) {
      clipper.IncludeItemByIndex(static_cast<int>(applied));
    }
    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const size_t index = static_cast<size_t>(row);
        const char* label = "<initial state>";
        if (index > 0) {
          const std::string& description = entries[index - 1].description;
          label = description.empty() ? "Edit" : description.c_str();
        }
        ImGui::PushID(row);
        if (index > applied) {
          ImGui::PushStyleColor(ImGuiCol_Text, redo_color);
        }
        if (ImGui::Selectable(label, index == applied)) {
          jump_target = index;
        }
        if (index > applied) {
          ImGui::PopStyleColor();
        }
        if (index == applied && scroll_to_current_) {
          ImGui::SetScrollHereY();
          scroll_to_current_ = false;
        }
        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();

  if (jump_target) {
    history_.jumpTo(*jump_target);
  }
}

}  // namespace amaryl::editor
