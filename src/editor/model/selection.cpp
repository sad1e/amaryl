#include "amaryl/editor/model/selection.h"

#include <algorithm>

namespace amaryl::editor {

void Selection::set(ObjectId object) {
  if (object == kInvalidObjectId) {
    clear();
    return;
  }
  if (objects_.size() == 1 && objects_.front() == object) {
    return;
  }
  objects_.assign(1, object);
  onChanged.emit();
}

void Selection::add(ObjectId object) {
  if (object == kInvalidObjectId) {
    return;
  }
  std::erase(objects_, object);
  objects_.push_back(object);
  onChanged.emit();
}

void Selection::remove(ObjectId object) {
  if (std::erase(objects_, object) > 0) {
    onChanged.emit();
  }
}

void Selection::toggle(ObjectId object) {
  if (contains(object)) {
    remove(object);
  } else {
    add(object);
  }
}

void Selection::clear() {
  if (!objects_.empty()) {
    objects_.clear();
    onChanged.emit();
  }
}

void Selection::prune(const ObjectRegistry& objects) {
  if (std::erase_if(objects_, [&objects](ObjectId id) { return !objects.isAlive(id); }) > 0) {
    onChanged.emit();
  }
}

bool Selection::contains(ObjectId object) const {
  return std::find(objects_.begin(), objects_.end(), object) != objects_.end();
}

}  // namespace amaryl::editor
