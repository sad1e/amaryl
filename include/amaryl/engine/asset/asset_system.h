#pragma once

#include <unordered_map>

#include "amaryl/engine/asset/asset_importer.h"
#include "amaryl/engine/asset/texture_asset.h"
#include "amaryl/engine/common.h"

namespace amaryl {

/**
 * @brief The asset system of whole game/editor.
 *
 */
struct AssetSystem {
  AssetSystem();
  ~AssetSystem();

  void ImportGLTF(std::string file_path);

 private:
  std::unordered_map<Uuid, Asset*> assets_;
  std::vector<MeshAsset> mesh_assets_;
  std::vector<MaterialAsset> material_assets_;
  std::vector<TextureAsset> texture_assets_;
};

}  // namespace amaryl
