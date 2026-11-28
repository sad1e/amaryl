#pragma once

#include <string>
#include <unordered_map>

#include "amaryl/editor/model/property.h"

namespace amaryl::editor {

// Stable editor identity of an engine object. When undo creates an object again, its
// handle changes but its ObjectId does not. Transactions and selections keep ObjectIds.
using ObjectId = uint64;
constexpr ObjectId kInvalidObjectId = 0;

struct EDITOR_API ObjectRegistry final {
  ObjectRegistry() = default;
  ~ObjectRegistry();

  NO_COPYABLE(ObjectRegistry);

  // Assigns an id on first use. kInvalidObjectId for 0.
  ObjectId getOrAdd(const TypeDesc& type, NativeHandle native);
  ObjectId find(const TypeDesc& type, NativeHandle native) const;

  void rebind(ObjectId id, NativeHandle native);
  void unbind(ObjectId id) { rebind(id, 0); }

  const TypeDesc* getType(ObjectId id) const;
  NativeHandle getNative(ObjectId id) const;
  bool isAlive(ObjectId id) const;
  std::string getDisplayName(ObjectId id) const;

 private:
  struct Entry {
    const TypeDesc* type{nullptr};
    NativeHandle native{0};
  };

  struct NativeKey {
    uint32 type{0};
    NativeHandle native{0};

    bool operator==(const NativeKey&) const = default;
  };

  struct NativeKeyHash {
    size_t operator()(const NativeKey& key) const {
      return static_cast<size_t>((key.native * 0x9E3779B97F4A7C15ull) ^ key.type);
    }
  };

  const Entry* findEntry(ObjectId id) const;

  std::unordered_map<ObjectId, Entry> objects_;
  std::unordered_map<NativeKey, ObjectId, NativeKeyHash> by_native_;
  ObjectId next_id_{1};
};

}  // namespace amaryl::editor
