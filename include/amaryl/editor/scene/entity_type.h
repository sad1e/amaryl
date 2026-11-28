#pragma once

#include <functional>
#include <string_view>

#include "amaryl/editor/model/property.h"
#include "amaryl/engine/scene/world.h"

namespace amaryl::editor {

inline constexpr std::string_view kEntityTypeName = "Entity";

// Returns an empty name when the entity has no name.
using EntityNameLookup = std::function<std::string_view(EntityHandle)>;

// Registers the editor type of the entities of `world` (local transform). `world` must
// outlive the registry. Without a name, the display name is "Mesh <index>" or
// "Entity <index>".
EDITOR_API const TypeDesc& RegisterEntityType(
  TypeRegistry& types, World& world, EntityNameLookup lookup_name = {});

}  // namespace amaryl::editor
