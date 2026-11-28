#include "amaryl/editor/framework/panel.h"

namespace amaryl::editor {

Panel::Panel(std::string id, std::string title, bool open_by_default)
    : id_(std::move(id)),
      title_(std::move(title)),
      window_name_(title_ + "###" + id_),
      open_(open_by_default),
      open_by_default_(open_by_default) {}

Panel::~Panel() = default;

}  // namespace amaryl::editor
