#include "amaryl/editor/framework/editor.h"

namespace amaryl::editor {

Editor::Editor(World& world) : world_(world) {
  // Undo of a creation destroys objects that can be selected.
  history_.onChanged.connect(
    [this](const Transaction&, EHistoryEvent) { selection_.prune(objects_); });
  registerActions();
}

Editor::~Editor() { shutdown(); }

void Editor::shutdown() {
  if (active_) {
    active_->onDeactivate();
    actions_.removeWithPrefix(active_->getId() + ".");
    active_ = nullptr;
  }
  pending_ = nullptr;
  for (auto it = contexts_.rbegin(); it != contexts_.rend(); ++it) {
    (*it)->onDetach();
  }
  contexts_.clear();
  selection_.clear();
  history_.clear();
}

void Editor::adoptContext(std::unique_ptr<EditorContext> context) {
  context->onAttach(*this);
  if (!active_ && !pending_) {
    pending_ = context.get();
  }
  contexts_.push_back(std::move(context));
}

void Editor::activate(std::string_view context_id) {
  for (const std::unique_ptr<EditorContext>& context : contexts_) {
    if (context->getId() == context_id) {
      pending_ = context.get();
      return;
    }
  }
  ALOGW("Unknown editor context '%.*s'", static_cast<int>(context_id.size()), context_id.data());
}

void Editor::applyPendingContext() {
  EditorContext* next = pending_;
  pending_ = nullptr;
  if (!next || next == active_) {
    return;
  }
  if (active_) {
    active_->onDeactivate();
    actions_.removeWithPrefix(active_->getId() + ".");
  }
  active_ = next;
  active_->registerActions(actions_);
  registerPanelActions(*active_);
  active_->onActivate();
}

void Editor::registerActions() {
  actions_.add({.id = "edit.undo",
    .label = "Undo",
    .menu = "Edit",
    .order = 0,
    .shortcut = ImGuiMod_Ctrl | ImGuiKey_Z,
    .execute = [this] { history_.undo(); },
    .isEnabled = [this] { return history_.canUndo(); }});
  actions_.add({.id = "edit.redo",
    .label = "Redo",
    .menu = "Edit",
    .order = 1,
    .shortcut = ImGuiMod_Ctrl | ImGuiKey_Y,
    .execute = [this] { history_.redo(); },
    .isEnabled = [this] { return history_.canRedo(); }});
  actions_.add({.id = "window.reset_layout",
    .label = "Reset Layout",
    .menu = "Window",
    .order = 900,
    .execute = [this] { active_->resetLayout(); },
    .isEnabled = [this] { return active_ != nullptr; }});
}

void Editor::registerPanelActions(EditorContext& context) {
  int32 order = 0;
  for (const std::unique_ptr<Panel>& owned_panel : context.getPanels()) {
    Panel* panel = owned_panel.get();
    actions_.add({.id = context.getId() + ".panel." + panel->getId(),
      .label = panel->getTitle(),
      .menu = "Window",
      .order = order++,
      .execute = [panel] { panel->setOpen(!panel->isOpen()); },
      .isChecked = [panel] { return panel->isOpen(); }});
  }
}

void Editor::drawFrame(float delta_time) {
  applyPendingContext();
  actions_.processShortcuts();
  drawMainMenu();
  drawDockspaces();
  if (active_) {
    active_->update(delta_time);
    active_->drawPanels();
  }
}

void Editor::drawMainMenu() {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }
  actions_.drawMenus();
  if (!contexts_.empty()) {
    ImGui::Separator();
    for (const std::unique_ptr<EditorContext>& context : contexts_) {
      if (ImGui::MenuItem(context->getTitle().c_str(), nullptr, context.get() == active_)) {
        pending_ = context.get();
      }
    }
  }
  ImGui::EndMainMenuBar();
}

void Editor::drawDockspaces() {
  // After the main menu bar, so the work area does not include it.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  constexpr ImGuiWindowFlags kHostFlags = ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
    ImGuiWindowFlags_NoSavedSettings;
  ImGui::Begin("###amaryl.editor.host", nullptr, kHostFlags);
  ImGui::PopStyleVar(3);
  for (const std::unique_ptr<EditorContext>& context : contexts_) {
    context->drawDockspace(context.get() == active_);
  }
  ImGui::End();
}

}  // namespace amaryl::editor
