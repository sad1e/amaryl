#include "amaryl/engine/scene/model.h"

#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"

namespace amaryl {

Model registerModel(const ModelAsset& asset, MeshRegistry& meshes,
                    MaterialRegistry& materials) {
  Model model;
  model.materials.reserve(asset.materials.size());
  for (const MaterialAsset& material : asset.materials) {
    model.materials.push_back(materials.create(material));
  }

  model.meshes.reserve(asset.meshes.size());
  std::vector<MaterialHandle> section_materials;
  for (const ModelMesh& mesh : asset.meshes) {
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

}  // namespace amaryl
