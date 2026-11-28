#include "amaryl/editor/scene/entity_type.h"

#include <cstdio>

namespace amaryl::editor {

namespace {

TransformHandle GetTransform(const World& world, NativeHandle native) {
  return world.entities.getTransform(FromNative<EntityHandle>(native));
}

PropertyDesc MakeProperty(std::string_view name, float speed) {
  PropertyDesc property;
  property.name = std::string(name);
  property.id = HashName(name);
  property.hints.speed = speed;
  return property;
}

}  // namespace

const TypeDesc& RegisterEntityType(
  TypeRegistry& types, World& world, EntityNameLookup lookup_name) {
  TypeDesc type;
  type.name = std::string(kEntityTypeName);
  type.id = HashName(kEntityTypeName);
  type.isAlive = [&world](NativeHandle native) {
    return world.entities.isAlive(FromNative<EntityHandle>(native));
  };
  type.getDisplayName = [&world, lookup_name = std::move(lookup_name)](NativeHandle native) {
    const EntityHandle entity = FromNative<EntityHandle>(native);
    if (lookup_name) {
      const std::string_view name = lookup_name(entity);
      if (!name.empty()) {
        return std::string(name);
      }
    }
    const bool has_mesh = world.entities.getRenderProxy(entity).valid();
    char name[64];
    std::snprintf(name, sizeof(name), "%s %u", has_mesh ? "Mesh" : "Entity",
      static_cast<unsigned>(entity.index()));
    return std::string(name);
  };

  // Writes to a static transform assert.
  const auto is_static = [&world](NativeHandle native) {
    return world.graph.getMobility(GetTransform(world, native)) == ETransformMobility::Static;
  };

  PropertyDesc position = MakeProperty("Position", 0.01f);
  position.get = [&world](NativeHandle native) -> PropertyValue {
    return world.graph.getLocalPosition(GetTransform(world, native));
  };
  position.set = [&world](NativeHandle native, const PropertyValue& value) {
    const Vec3* local_position = std::get_if<Vec3>(&value);
    if (!local_position) {
      return false;
    }
    world.graph.setLocalPosition(GetTransform(world, native), *local_position);
    return true;
  };
  position.isReadOnly = is_static;

  PropertyDesc rotation = MakeProperty("Rotation", 0.5f);
  rotation.get = [&world](NativeHandle native) -> PropertyValue {
    return world.graph.getLocalRotation(GetTransform(world, native));
  };
  rotation.set = [&world](NativeHandle native, const PropertyValue& value) {
    const Quat* local_rotation = std::get_if<Quat>(&value);
    if (!local_rotation) {
      return false;
    }
    world.graph.setLocalRotation(GetTransform(world, native), local_rotation->Normalized());
    return true;
  };
  rotation.isReadOnly = is_static;

  PropertyDesc scale = MakeProperty("Scale", 0.01f);
  scale.get = [&world](NativeHandle native) -> PropertyValue {
    return world.graph.getLocalScale(GetTransform(world, native));
  };
  scale.set = [&world](NativeHandle native, const PropertyValue& value) {
    const Vec3* local_scale = std::get_if<Vec3>(&value);
    if (!local_scale) {
      return false;
    }
    world.graph.setLocalScale(GetTransform(world, native), *local_scale);
    return true;
  };
  scale.isReadOnly = is_static;

  type.properties = {std::move(position), std::move(rotation), std::move(scale)};
  return types.add(std::move(type));
}

}  // namespace amaryl::editor
