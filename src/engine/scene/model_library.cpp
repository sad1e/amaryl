#include "amaryl/engine/scene/model_library.h"

#include "amaryl/core/logger.h"
#include "amaryl/engine/render/texture_registry.h"
#include "amaryl/engine/scene/world.h"

namespace amaryl {

ModelLibrary::ModelLibrary(World& world, TextureRegistry* textures)
    : world_(world), textures_(textures) {}

ModelLibrary::~ModelLibrary() { releaseUnused(); }

const LoadedModel* ModelLibrary::load(const std::string& path) {
  if (const LoadedModel* loaded = find(path)) {
    return loaded;
  }

  // The importer references hold the textures until the materials reference them.
  std::vector<TextureDescriptorIndex> acquired;
  GltfImportOptions options;
  options.meshBuild = meshBuild;
  if (textures_) {
    options.loadImage = [&](const GltfImageDesc& image) {
      TextureDescriptorIndex texture{};
      if (!image.data.empty()) {
        texture = textures_->acquire(path + "#image" + std::to_string(image.index),
                                     image.data, image.srgb);
      } else if (!image.path.empty()) {
        texture = textures_->acquireFile(image.path, image.srgb);
      }
      if (texture.value != 0) {
        acquired.push_back(texture);
      }
      return texture;
    };
    options.createSampler = [this](const GltfSamplerDesc& sampler) {
      return textures_->getSampler(sampler);
    };
  }

  GltfImportResult result;
  const bool imported = importGltf(path.c_str(), options, result);

  LoadedModel* loaded = nullptr;
  if (imported) {
    auto model = std::make_unique<LoadedModel>();
    model->path = path;
    model->model = registerModel(result.model, world_.meshes, world_.materials);
    model->scene = std::move(result.scene);
    loaded = models_.emplace(path, std::move(model)).first->second.get();
  } else {
    ALOGW("ModelLibrary: cannot import '%s'", path.c_str());
  }

  for (const TextureDescriptorIndex texture : acquired) {
    textures_->release(texture);
  }
  return loaded;
}

const LoadedModel* ModelLibrary::find(const std::string& path) const {
  const auto it = models_.find(path);
  return it != models_.end() ? it->second.get() : nullptr;
}

bool ModelLibrary::instantiate(const std::string& path, const SceneNodesDesc& desc,
                               std::vector<EntityHandle>& out) {
  const LoadedModel* loaded = load(path);
  if (!loaded) {
    out.clear();
    return false;
  }
  return addSceneNodes(loaded->scene, loaded->model, world_.entities, desc, out);
}

uint32 ModelLibrary::releaseUnused() {
  uint32 released = 0;
  for (auto it = models_.begin(); it != models_.end();) {
    if (isUnused(it->second->model)) {
      releaseModel(it->second->model, world_.meshes, world_.materials);
      it = models_.erase(it);
      ++released;
    } else {
      ++it;
    }
  }
  return released;
}

bool ModelLibrary::isUnused(const Model& model) const {
  // Material references held by the model meshes, the other ones are proxies (e.g. a
  // material assigned to another model).
  std::unordered_map<uint32, uint32> mesh_references;
  for (const MeshHandle mesh : model.meshes) {
    if (!mesh) {
      continue;
    }
    if (world_.meshes.getRefCount(mesh) != 0) {
      return false;
    }
    for (uint32 section = 0; section < world_.meshes.getSectionCount(mesh); ++section) {
      ++mesh_references[world_.meshes.getDefaultMaterial(mesh, section).index()];
    }
  }
  for (const MaterialHandle material : model.materials) {
    const auto it = mesh_references.find(material.index());
    const uint32 expected = it != mesh_references.end() ? it->second : 0;
    if (world_.materials.getRefCount(material) != expected) {
      return false;
    }
  }
  return true;
}

}  // namespace amaryl
