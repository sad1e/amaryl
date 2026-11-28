#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "amaryl/editor/framework/common.h"

namespace amaryl::editor {

// A user command. It shows in the main menu when `menu` is set, and its shortcut runs it.
// Contexts and panels register actions, they do not draw menu items.
struct Action {
  // Unique, e.g. "edit.undo".
  std::string id;
  std::string label;
  // '/' separates sub menus ("View/Debug"). Empty: not in the main menu.
  std::string menu;
  // Sort key in the menu. A separator divides different hundreds.
  int32 order{0};
  ImGuiKeyChord shortcut{ImGuiKey_None};
  std::function<void()> execute;
  // Null: always enabled.
  std::function<bool()> isEnabled;
  // Null: not a toggle.
  std::function<bool()> isChecked;

  bool enabled() const { return !isEnabled || isEnabled(); }
};

struct EDITOR_UI_API ActionRegistry final {
  ActionRegistry();
  ~ActionRegistry();

  NO_COPYABLE_AND_MOVEABLE(ActionRegistry);

  // Replaces the action with the same id.
  void add(Action action);
  void remove(std::string_view id);
  void removeWithPrefix(std::string_view prefix);
  const Action* find(std::string_view id) const;
  bool run(std::string_view id) const;

  // Once per frame. The global route lets a focused text field keep its own Ctrl+Z.
  void processShortcuts() const;
  // Between BeginMainMenuBar() and EndMainMenuBar().
  void drawMenus() const;

 private:
  bool hasMenu(std::string_view top_menu) const;
  void drawMenuLevel(std::string_view path, std::function<void()>& clicked) const;

  std::vector<Action> actions_;
};

}  // namespace amaryl::editor
