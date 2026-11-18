#include "amaryl/engine/scene/model.h"

#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"

namespace amaryl {

Model registerModel(const GltfImportResult& imported, MeshRegistry& meshes,
                    MaterialRegistry& materials) {
  Model model;
  model.materials.reserve(imported.materials.size());
  for (const MaterialAsset& material : imported.materials) {
    model.materials.push_back(materials.create(material));
  }

  model.meshes.reserve(imported.meshes.size());
  std::vector<MaterialHandle> section_materials;
  for (const GltfMesh& mesh : imported.meshes) {
    MeshHandle handle{};
    if (mesh.asset) {
      section_materials.assign(mesh.sectionMaterials.size(), MaterialHandle{});
      for (size_t i = 0; i < section_materials.size(); ++i) {
        const int32 material = mesh.sectionMaterials[i];
        if (material >= 0 && static_cast<size_t>(material) < model.materials.size()) {
          section_materials[i] = model.materials[material];
        }
      }
      handle = meshes.create(mesh.asset, section_materials);
    }
    model.meshes.push_back(handle);
  }

  model.nodes = imported.nodes;
  return model;
}

void releaseModel(Model& model, MeshRegistry& meshes, MaterialRegistry& materials) {
  // Meshes first, they reference the materials.
  for (const MeshHandle mesh : model.meshes) {
    if (mesh) {
      meshes.destroy(mesh);
    }
  }
  for (const MaterialHandle material : model.materials) {
    materials.destroy(material);
  }
  model = {};
}

EntityHandle instantiateModel(const Model& model, EntityManager& entities,
                              const TransformDesc& root) {
  const EntityHandle root_entity = entities.create({root});
  if (!root_entity) {
    return {};
  }

  // Parents come first, their transforms are known when the children are created.
  std::vector<TransformHandle> transforms(model.nodes.size());
  for (size_t i = 0; i < model.nodes.size(); ++i) {
    const GltfNode& node = model.nodes[i];
    EntityDesc desc;
    desc.transform.parent =
        node.parent >= 0 ? transforms[node.parent] : entities.getTransform(root_entity);
    desc.transform.position = node.position;
    desc.transform.rotation = node.rotation;
    desc.transform.scale = node.scale;
    desc.transform.mobility = root.mobility;
    if (node.mesh >= 0) {
      desc.mesh = model.meshes[node.mesh];
    }

    const EntityHandle entity = entities.create(desc);
    if (!entity) {
      entities.destroy(root_entity);
      return {};
    }
    transforms[i] = entities.getTransform(entity);
  }
  return root_entity;
}

}  // namespace amaryl
