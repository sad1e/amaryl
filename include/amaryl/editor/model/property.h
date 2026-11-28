#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "amaryl/core/math/math.h"
#include "amaryl/editor/common.h"

namespace amaryl::editor {

// An engine handle packed with Handle::handleAsVoid(), 0 is no object.
using NativeHandle = uint64;

template <typename HandleType>
NativeHandle ToNative(HandleType handle) {
  return static_cast<NativeHandle>(reinterpret_cast<uintptr_t>(handle.handleAsVoid()));
}

template <typename HandleType>
HandleType FromNative(NativeHandle native) {
  return HandleType(reinterpret_cast<void*>(static_cast<uintptr_t>(native)));
}

// FNV-1a.
constexpr uint32 HashName(std::string_view name) {
  uint32 hash = 2166136261u;
  for (const char c : name) {
    hash = (hash ^ static_cast<uint8>(c)) * 16777619u;
  }
  return hash;
}

using PropertyValue = std::variant<bool, int32, float, Vec3, Quat, std::string>;

enum class EPropertyFlags : uint8 {
  None = 0,
  ReadOnly = 1 << 0,
  Hidden = 1 << 1,
};
MAKE_ENUM_FLAGS(EPropertyFlags);

enum class EPropertyWidget : uint8 {
  Default,
  Color,
};

// Presentation hints only. No clamp when min == max. Quat values show as Euler degrees.
struct PropertyHints {
  EPropertyWidget widget = EPropertyWidget::Default;
  float speed = 0.01f;
  float min = 0.0f;
  float max = 0.0f;
};

struct PropertyDesc {
  std::string name;
  // HashName(name) when 0.
  uint32 id{0};
  EPropertyFlags flags = EPropertyFlags::None;
  PropertyHints hints{};
  std::function<PropertyValue(NativeHandle)> get;
  // Returns false and changes nothing when the value cannot be written.
  std::function<bool(NativeHandle, const PropertyValue&)> set;
  std::function<bool(NativeHandle)> isReadOnly;

  bool canWrite(NativeHandle native) const {
    return set && !EnumHasAnyFlags(flags, EPropertyFlags::ReadOnly) &&
           !(isReadOnly && isReadOnly(native));
  }
};

// The data TypeDesc::restore needs to create a destroyed object again.
struct ObjectState {
  std::vector<std::pair<uint32, PropertyValue>> properties;
  // Type specific data that the properties do not hold (mesh, materials, children...).
  std::shared_ptr<const void> payload;
};

// Editor view of an engine object type (Decima's EditorNode). All editor reads and writes
// go through it, the engine type does not know it.
struct TypeDesc {
  std::string name;
  // HashName(name) when 0.
  uint32 id{0};
  std::vector<PropertyDesc> properties;

  std::function<bool(NativeHandle)> isAlive;
  std::function<std::string(NativeHandle)> getDisplayName;

  // Optional. Without them, transactions cannot create or destroy objects of this type.
  std::function<ObjectState(NativeHandle)> save;
  std::function<NativeHandle(const ObjectState&)> restore;
  std::function<void(NativeHandle)> destroy;

  bool supportsStructuralEdits() const { return save && restore && destroy; }

  const PropertyDesc* findProperty(uint32 property_id) const {
    for (const PropertyDesc& property : properties) {
      if (property.id == property_id) {
        return &property;
      }
    }
    return nullptr;
  }
};

struct EDITOR_API TypeRegistry final {
  TypeRegistry() = default;
  ~TypeRegistry();

  NO_COPYABLE(TypeRegistry);

  // Replaces the type with the same id. The reference stays valid until the registry is
  // destroyed.
  TypeDesc& add(TypeDesc type);
  const TypeDesc* find(uint32 type_id) const;
  const TypeDesc* find(std::string_view name) const { return find(HashName(name)); }

 private:
  std::vector<std::unique_ptr<TypeDesc>> types_;
};

}  // namespace amaryl::editor
