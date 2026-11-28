#include "amaryl/editor/model/object_registry.h"

#include <cstdio>

namespace amaryl::editor {

ObjectRegistry::~ObjectRegistry() = default;

ObjectId ObjectRegistry::getOrAdd(const TypeDesc& type, NativeHandle native) {
  if (native == 0) {
    return kInvalidObjectId;
  }
  const auto [it, inserted] = by_native_.try_emplace(NativeKey{type.id, native}, next_id_);
  if (inserted) {
    objects_.emplace(next_id_, Entry{&type, native});
    ++next_id_;
  }
  return it->second;
}

ObjectId ObjectRegistry::find(const TypeDesc& type, NativeHandle native) const {
  const auto it = by_native_.find(NativeKey{type.id, native});
  return it == by_native_.end() ? kInvalidObjectId : it->second;
}

void ObjectRegistry::rebind(ObjectId id, NativeHandle native) {
  const auto it = objects_.find(id);
  if (it == objects_.end()) {
    return;
  }
  Entry& entry = it->second;
  if (entry.native != 0) {
    by_native_.erase(NativeKey{entry.type->id, entry.native});
  }
  entry.native = native;
  if (native != 0) {
    by_native_[NativeKey{entry.type->id, native}] = id;
  }
}

const ObjectRegistry::Entry* ObjectRegistry::findEntry(ObjectId id) const {
  const auto it = objects_.find(id);
  return it == objects_.end() ? nullptr : &it->second;
}

const TypeDesc* ObjectRegistry::getType(ObjectId id) const {
  const Entry* entry = findEntry(id);
  return entry ? entry->type : nullptr;
}

NativeHandle ObjectRegistry::getNative(ObjectId id) const {
  const Entry* entry = findEntry(id);
  return entry ? entry->native : 0;
}

bool ObjectRegistry::isAlive(ObjectId id) const {
  const Entry* entry = findEntry(id);
  return entry && entry->native != 0 &&
         (!entry->type->isAlive || entry->type->isAlive(entry->native));
}

std::string ObjectRegistry::getDisplayName(ObjectId id) const {
  const Entry* entry = findEntry(id);
  if (!entry) {
    return "<unknown>";
  }
  if (entry->type->getDisplayName && isAlive(id)) {
    return entry->type->getDisplayName(entry->native);
  }
  char name[96];
  std::snprintf(name, sizeof(name), "%s #%llu", entry->type->name.c_str(),
    static_cast<unsigned long long>(id));
  return name;
}

}  // namespace amaryl::editor
