#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "amaryl/editor/scene/editor_scene.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

// Quad positions @0, normals @48, uvs @96 and uint16 indices @128.
std::vector<uint8> makeBuffer() {
  const float positions[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const float normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
  const float uvs[] = {0, 1, 1, 1, 1, 0, 0, 0};
  const uint16 indices[] = {0, 1, 2, 0, 2, 3};
  std::vector<uint8> buffer(140);
  std::memcpy(buffer.data(), positions, sizeof(positions));
  std::memcpy(buffer.data() + 48, normals, sizeof(normals));
  std::memcpy(buffer.data() + 96, uvs, sizeof(uvs));
  std::memcpy(buffer.data() + 128, indices, sizeof(indices));
  return buffer;
}

std::string encodeBase64(const std::vector<uint8>& bytes) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32 b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
    const uint32 b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
    const uint32 triple = (uint32{bytes[i]} << 16) | (b1 << 8) | b2;
    out += kAlphabet[(triple >> 18) & 63];
    out += kAlphabet[(triple >> 12) & 63];
    out += i + 1 < bytes.size() ? kAlphabet[(triple >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[triple & 63] : '=';
  }
  return out;
}

// Two nodes sharing the quad mesh and its material.
std::string makeGltf() {
  std::string json = R"json({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "root", "children": [1, 2]},
    {"name": "left", "mesh": 0, "translation": [-1, 0, 0]},
    {"name": "right", "mesh": 0, "translation": [1, 0, 0]}
  ],
  "meshes": [{"name": "quad", "primitives": [
    {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
     "material": 0}]}],
  "materials": [{"name": "white", "pbrMetallicRoughness": {"metallicFactor": 0.5}}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
    {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48},
    {"buffer": 0, "byteOffset": 48, "byteLength": 48},
    {"buffer": 0, "byteOffset": 96, "byteLength": 32},
    {"buffer": 0, "byteOffset": 128, "byteLength": 12}
  ],
  "buffers": [{"byteLength": 140, "uri": "data:application/octet-stream;base64,@BUFFER@"}]
})json";
  const std::string marker = "@BUFFER@";
  json.replace(json.find(marker), marker.size(), encodeBase64(makeBuffer()));
  return json;
}

std::string writeGltf() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "amaryl_editor_scene_test.gltf";
  std::ofstream(path, std::ios::binary) << makeGltf();
  return path.generic_string();
}

}  // namespace

int main() {
  const std::string path = writeGltf();

  "editor scene imports, edits and releases models"_test = [&] {
    World world;
    ModelLibrary models(world, nullptr);
    EditorScene editor(world, models);

    expect(!editor.importModel(path + ".missing"));
    expect(world.entities.getCount() == 0u && models.getCount() == 0u);

    const EntityHandle first = editor.importModel(path);
    const EntityHandle second = editor.importModel(path);
    expect(first.valid() && second.valid());
    // One wrapper root per import plus the 3 nodes, the model is imported once.
    expect(world.entities.getCount() == 8u && models.getCount() == 1u);
    expect(world.scene.getCount() == 4u && world.meshes.getCount() == 1u);

    // Proxy of the first node of the first import.
    const TransformHandle gltf_root =
        world.graph.getFirstChild(world.entities.getTransform(first));
    const EntityHandle node = world.entities.findByTransform(world.graph.getFirstChild(gltf_root));
    const RenderProxyHandle proxy = world.entities.getRenderProxy(node);
    expect(proxy.valid());

    const EntityHandle left = editor.selectByInstanceId(RenderScene::getInstanceId(proxy));
    expect(left == node && editor.getSelection() == left);
    expect(!editor.selectByInstanceId(100u) && !editor.getSelection());

    const MaterialHandle shared = editor.getMaterial(left, 0);
    expect(shared.valid() && !editor.isOverride(shared));
    expect(world.materials.get(shared).metallicFactor == 0.5f);

    // Copy-on-write: the other proxies keep the shared material.
    MaterialAsset red = world.materials.get(shared);
    red.baseColorFactor = float4{1.0f, 0.0f, 0.0f, 1.0f};
    const MaterialHandle override_material = editor.editMaterial(left, 0, red);
    expect(override_material.valid() && override_material != shared);
    expect(editor.isOverride(override_material) && editor.getOverrideCount() == 1u);
    expect(editor.getMaterial(left, 0) == override_material);
    expect(world.materials.get(shared).baseColorFactor[0] == 1.0f &&
           world.materials.get(shared).baseColorFactor[1] == 1.0f);
    expect(world.materials.getRefCount(override_material) == 1u);

    // Later edits update the override in place.
    red.roughnessFactor = 0.25f;
    expect(editor.editMaterial(left, 0, red) == override_material);
    expect(world.materials.get(override_material).roughnessFactor == 0.25f);

    editor.resetMaterial(left, 0);
    expect(editor.getMaterial(left, 0) == shared && editor.getOverrideCount() == 0u);
    expect(!world.materials.isAlive(override_material));

    // Destroying an import releases its overrides, the model stays for the other one.
    const MaterialHandle second_override = editor.editMaterial(left, 0, red);
    editor.select(first);
    editor.destroyEntity(first);
    expect(!editor.getSelection());
    expect(!world.materials.isAlive(second_override) && editor.getOverrideCount() == 0u);
    expect(world.scene.getCount() == 2u && models.getCount() == 1u);

    // The last instance releases the model.
    editor.destroyEntity(second);
    expect(world.entities.getCount() == 0u && models.getCount() == 0u);
    expect(world.meshes.getCount() == 0u && world.materials.getCount() == 1u);
  };

  "editor scene moves entities"_test = [&] {
    World world;
    ModelLibrary models(world, nullptr);
    EditorScene editor(world, models);

    const EntityHandle root = editor.importModel(path);
    world.sync();
    editor.setLocalTRS(root, Vec3(0, 0, 5), Quat::sIdentity(), Vec3::sOne());
    world.sync();
    const TransformHandle gltf_root =
        world.graph.getFirstChild(world.entities.getTransform(root));
    uint32 count = 0;
    for (TransformHandle node = world.graph.getFirstChild(gltf_root); node;
         node = world.graph.getNextSibling(node)) {
      const Vec3 position = world.graph.getWorld(node).GetTranslation();
      expect(std::abs(position.GetZ() - 5.0f) < 1e-6f);
      expect(std::abs(std::abs(position.GetX()) - 1.0f) < 1e-6f);
      ++count;
    }
    expect(count == 2u);
  };

  std::filesystem::remove(path);
  return 0;
}
