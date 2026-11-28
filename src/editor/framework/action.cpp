#include "amaryl/editor/framework/action.h"

#include <imgui_internal.h>

#include <algorithm>

namespace amaryl::editor {

namespace {

constexpr std::string_view kMenuOrder[] = {"File", "Edit", "View", "Window", "Help"};

std::string_view TopMenu(std::string_view path) { return path.substr(0, path.find('/')); }

}  // namespace

ActionRegistry::ActionRegistry() = default;
ActionRegistry::~ActionRegistry() = default;

void ActionRegistry::add(Action action) {
  const auto it = std::find_if(actions_.begin(), actions_.end(),
    [&action](const Action& existing) { return existing.id == action.id; });
  if (it != actions_.end()) {
    *it = std::move(action);
  } else {
    actions_.push_back(std::move(action));
  }
}

void ActionRegistry::remove(std::string_view id) {
  std::erase_if(actions_, [id](const Action& action) { return action.id == id; });
}

void ActionRegistry::removeWithPrefix(std::string_view prefix) {
  std::erase_if(
    actions_, [prefix](const Action& action) { return action.id.starts_with(prefix); });
}

const Action* ActionRegistry::find(std::string_view id) const {
  for (const Action& action : actions_) {
    if (action.id == id) {
      return &action;
    }
  }
  return nullptr;
}

bool ActionRegistry::run(std::string_view id) const {
  const Action* action = find(id);
  if (!action || !action->execute || !action->enabled()) {
    return false;
  }
  // A copy, because the action can remove itself.
  const std::function<void()> execute = action->execute;
  execute();
  return true;
}

void ActionRegistry::processShortcuts() const {
  std::vector<std::function<void()>> triggered;
  for (const Action& action : actions_) {
    if (action.shortcut != ImGuiKey_None && action.execute &&
        ImGui::Shortcut(action.shortcut, ImGuiInputFlags_RouteGlobal) && action.enabled()) {
      triggered.push_back(action.execute);
    }
  }
  for (const std::function<void()>& execute : triggered) {
    execute();
  }
}

bool ActionRegistry::hasMenu(std::string_view top_menu) const {
  return std::any_of(actions_.begin(), actions_.end(), [top_menu](const Action& action) {
    return !action.menu.empty() && TopMenu(action.menu) == top_menu;
  });
}

void ActionRegistry::drawMenus() const {
  std::vector<std::string_view> top_menus;
  for (const std::string_view name : kMenuOrder) {
    if (hasMenu(name)) {
      top_menus.push_back(name);
    }
  }
  for (const Action& action : actions_) {
    const std::string_view top_menu = TopMenu(action.menu);
    if (!top_menu.empty() &&
        std::find(top_menus.begin(), top_menus.end(), top_menu) == top_menus.end()) {
      top_menus.push_back(top_menu);
    }
  }

  std::function<void()> clicked;
  for (const std::string_view top_menu : top_menus) {
    const std::string name(top_menu);
    if (ImGui::BeginMenu(name.c_str())) {
      drawMenuLevel(top_menu, clicked);
      ImGui::EndMenu();
    }
  }
  if (clicked) {
    clicked();
  }
}

void ActionRegistry::drawMenuLevel(
  std::string_view path, std::function<void()>& clicked) const {
  std::vector<const Action*> items;
  std::vector<std::string_view> submenus;
  for (const Action& action : actions_) {
    const std::string_view menu = action.menu;
    if (menu == path) {
      items.push_back(&action);
    } else if (menu.size() > path.size() && menu.starts_with(path) &&
               menu[path.size()] == '/') {
      const std::string_view rest = menu.substr(path.size() + 1);
      const std::string_view child = rest.substr(0, rest.find('/'));
      if (std::find(submenus.begin(), submenus.end(), child) == submenus.end()) {
        submenus.push_back(child);
      }
    }
  }
  std::stable_sort(items.begin(), items.end(),
    [](const Action* a, const Action* b) { return a->order < b->order; });

  int32 group = items.empty() ? 0 : items.front()->order / 100;
  for (const Action* action : items) {
    if (action->order / 100 != group) {
      ImGui::Separator();
      group = action->order / 100;
    }
    const char* shortcut =
      action->shortcut != ImGuiKey_None ? ImGui::GetKeyChordName(action->shortcut) : nullptr;
    const bool checked = action->isChecked && action->isChecked();
    if (ImGui::MenuItem(action->label.c_str(), shortcut, checked, action->enabled()) &&
        action->execute) {
      clicked = action->execute;
    }
  }

  if (!items.empty() && !submenus.empty()) {
    ImGui::Separator();
  }
  for (const std::string_view child : submenus) {
    const std::string name(child);
    if (ImGui::BeginMenu(name.c_str())) {
      drawMenuLevel(std::string(path) + "/" + name, clicked);
      ImGui::EndMenu();
    }
  }
}

}  // namespace amaryl::editor
