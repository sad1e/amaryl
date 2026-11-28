#pragma once

#include <vector>

#include "amaryl/core/span.h"
#include "amaryl/editor/model/object_registry.h"
#include "amaryl/editor/model/signal.h"

namespace amaryl::editor {

struct EDITOR_API Selection final {
  // kInvalidObjectId clears the selection.
  void set(ObjectId object);
  // Selecting again makes the object the primary one.
  void add(ObjectId object);
  void remove(ObjectId object);
  void toggle(ObjectId object);
  void clear();
  // Removes the objects that are not alive.
  void prune(const ObjectRegistry& objects);

  bool contains(ObjectId object) const;
  bool empty() const { return objects_.empty(); }
  // The last selected object.
  ObjectId getPrimary() const { return objects_.empty() ? kInvalidObjectId : objects_.back(); }
  Span<const ObjectId> get() const { return objects_; }

  Signal<> onChanged;

 private:
  std::vector<ObjectId> objects_;
};

}  // namespace amaryl::editor
