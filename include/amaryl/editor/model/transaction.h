#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "amaryl/editor/model/object_registry.h"

namespace amaryl::editor {

enum class ECommandKind : uint8 {
  SetProperty,
  CreateObject,
  DestroyObject,
};

// One reversible change. Commands are data only, there is no virtual do/undo: the inverse
// comes from the data.
struct Command {
  ECommandKind kind = ECommandKind::SetProperty;
  ObjectId object{kInvalidObjectId};
  uint32 property{0};
  PropertyValue before{};
  PropertyValue after{};
  // CreateObject / DestroyObject: the state of the object while it exists.
  std::shared_ptr<const ObjectState> state;

  Command inverted() const {
    Command result = *this;
    switch (kind) {
      case ECommandKind::SetProperty:
        std::swap(result.before, result.after);
        break;
      case ECommandKind::CreateObject:
        result.kind = ECommandKind::DestroyObject;
        break;
      case ECommandKind::DestroyObject:
        result.kind = ECommandKind::CreateObject;
        break;
    }
    return result;
  }
};

struct Transaction {
  std::string description;
  std::vector<Command> commands;
  // Consecutive transactions with the same non zero key merge into one undo entry.
  uint64 merge_key{0};

  bool empty() const { return commands.empty(); }

  Transaction inverted() const {
    Transaction result{description, {}, merge_key};
    result.commands.reserve(commands.size());
    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
      result.commands.push_back(it->inverted());
    }
    return result;
  }
};

}  // namespace amaryl::editor
