#pragma once

#include <string>
#include <utility>
#include <vector>

#include "amaryl/editor/model/history.h"

namespace amaryl::editor {

// Decima's DiffUtil: snapshots the properties of the tracked objects, the code changes
// them through the engine API, commit() records the differences as one transaction.
//
//   EditScope scope(history, "Align to ground");
//   scope.track(object);
//   world.graph.setLocalPosition(transform, ground);
//   scope.commit();
struct EDITOR_API EditScope final {
  EditScope(History& history, std::string description, uint64 merge_key = 0);
  // Commits when neither commit() nor cancel() was called.
  ~EditScope();

  NO_COPYABLE_AND_MOVEABLE(EditScope);

  // Call before the change.
  void track(ObjectId object);
  // Returns false when nothing changed.
  bool commit();
  // Writes the snapshots back and records nothing.
  void cancel();

 private:
  struct Snapshot {
    ObjectId object{kInvalidObjectId};
    std::vector<std::pair<uint32, PropertyValue>> values;
  };

  History& history_;
  std::string description_;
  uint64 merge_key_{0};
  std::vector<Snapshot> snapshots_;
  bool done_{false};
};

}  // namespace amaryl::editor
