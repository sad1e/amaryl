#pragma once

#include "amaryl/editor/framework/panel.h"
#include "amaryl/editor/model/history.h"

namespace amaryl::editor {

// The undo stack. A click on an entry undoes or redoes up to it.
struct EDITOR_UI_API HistoryPanel final : Panel {
  explicit HistoryPanel(History& history);
  ~HistoryPanel() override;

  void draw() override;

 private:
  History& history_;
  uint32 listener_{0};
  bool scroll_to_current_{false};
};

}  // namespace amaryl::editor
