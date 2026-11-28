#pragma once

#include <string>

#include "amaryl/editor/framework/common.h"

namespace amaryl::editor {

// A dockable window of an editor context.
struct EDITOR_UI_API Panel {
  Panel(std::string id, std::string title, bool open_by_default = true);
  virtual ~Panel();

  NO_COPYABLE_AND_MOVEABLE(Panel);

  // Every frame while open and visible, inside the panel window.
  virtual void draw() = 0;
  virtual ImGuiWindowFlags getWindowFlags() const { return ImGuiWindowFlags_None; }

  const std::string& getId() const { return id_; }
  const std::string& getTitle() const { return title_; }
  // "<title>###<context id>.<panel id>".
  const char* getWindowName() const { return window_name_.c_str(); }
  bool isOpen() const { return open_; }
  void setOpen(bool open) { open_ = open; }

 private:
  friend struct EditorContext;

  std::string id_;
  std::string title_;
  std::string window_name_;
  bool open_{true};
  bool open_by_default_{true};
};

}  // namespace amaryl::editor
