#include "amaryl/editor/model/property.h"

namespace amaryl::editor {

TypeRegistry::~TypeRegistry() = default;

TypeDesc& TypeRegistry::add(TypeDesc type) {
  if (type.id == 0) {
    type.id = HashName(type.name);
  }
  for (PropertyDesc& property : type.properties) {
    if (property.id == 0) {
      property.id = HashName(property.name);
    }
  }
  for (std::unique_ptr<TypeDesc>& existing : types_) {
    if (existing->id == type.id) {
      *existing = std::move(type);
      return *existing;
    }
  }
  return *types_.emplace_back(std::make_unique<TypeDesc>(std::move(type)));
}

const TypeDesc* TypeRegistry::find(uint32 type_id) const {
  for (const std::unique_ptr<TypeDesc>& type : types_) {
    if (type->id == type_id) {
      return type.get();
    }
  }
  return nullptr;
}

}  // namespace amaryl::editor
