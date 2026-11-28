#include "amaryl/editor/model/edit_scope.h"

namespace amaryl::editor {

EditScope::EditScope(History& history, std::string description, uint64 merge_key)
    : history_(history), description_(std::move(description)), merge_key_(merge_key) {}

EditScope::~EditScope() {
  if (!done_) {
    commit();
  }
}

void EditScope::track(ObjectId object) {
  const ObjectRegistry& objects = history_.getObjects();
  if (done_ || !objects.isAlive(object)) {
    return;
  }
  for (const Snapshot& snapshot : snapshots_) {
    if (snapshot.object == object) {
      return;
    }
  }

  const TypeDesc* type = objects.getType(object);
  const NativeHandle native = objects.getNative(object);
  Snapshot snapshot{object, {}};
  for (const PropertyDesc& property : type->properties) {
    if (property.get) {
      snapshot.values.emplace_back(property.id, property.get(native));
    }
  }
  snapshots_.push_back(std::move(snapshot));
}

bool EditScope::commit() {
  if (done_) {
    return false;
  }
  done_ = true;

  const ObjectRegistry& objects = history_.getObjects();
  Transaction transaction{std::move(description_), {}, merge_key_};
  for (const Snapshot& snapshot : snapshots_) {
    if (!objects.isAlive(snapshot.object)) {
      continue;
    }
    const TypeDesc* type = objects.getType(snapshot.object);
    const NativeHandle native = objects.getNative(snapshot.object);
    for (const auto& [property_id, before] : snapshot.values) {
      PropertyValue after = type->findProperty(property_id)->get(native);
      if (!(after == before)) {
        transaction.commands.push_back({.kind = ECommandKind::SetProperty,
          .object = snapshot.object,
          .property = property_id,
          .before = before,
          .after = std::move(after)});
      }
    }
  }
  if (transaction.empty()) {
    return false;
  }
  history_.record(std::move(transaction));
  return true;
}

void EditScope::cancel() {
  if (done_) {
    return;
  }
  done_ = true;

  const ObjectRegistry& objects = history_.getObjects();
  for (const Snapshot& snapshot : snapshots_) {
    if (!objects.isAlive(snapshot.object)) {
      continue;
    }
    const TypeDesc* type = objects.getType(snapshot.object);
    const NativeHandle native = objects.getNative(snapshot.object);
    for (const auto& [property_id, before] : snapshot.values) {
      const PropertyDesc* property = type->findProperty(property_id);
      if (property->canWrite(native)) {
        property->set(native, before);
      }
    }
  }
}

}  // namespace amaryl::editor
