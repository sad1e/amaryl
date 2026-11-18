#include "amaryl/engine/scene/entity.h"

#include <memory>
#include <vector>

#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"
#include "amaryl/engine/render/render_scene.h"
#include "amaryl/engine/scene/model.h"
#include "amaryl/engine/scene/scene_graph.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

// One quad per section.
std::shared_ptr<const MeshAsset> makeQuads(uint32 sections) {
  const float3 positions[] = {float3{0.0f, 0.0f, 0.0f}, float3{1.0f, 0.0f, 0.0f},
                              float3{1.0f, 1.0f, 0.0f}, float3{0.0f, 1.0f, 0.0f}};
  const uint32 indices[] = {0, 1, 2, 0, 2, 3};
  const std::vector<MeshSourceSection> source(sections, {positions, {}, {}, {}, indices});
  auto asset = std::make_shared<MeshAsset>();
  asset->name = "quads";
  expect(buildMeshAsset(source, {}, *asset));
  return asset;
}

struct World {
  SceneGraph graph;
  MaterialRegistry materials;
  MeshRegistry meshes{materials};
  RenderScene scene{graph, meshes, materials};
  EntityManager entities{graph, scene};

  void sync() {
    graph.update();
    scene.sync();
  }

  RenderProxyHandle getProxy(TransformHandle transform) const {
    return entities.getRenderProxy(entities.findByTransform(transform));
  }

  bool isWorldAt(TransformHandle transform, Vec3Arg position) const {
    return graph.getWorld(transform).GetTranslation().IsClose(position, 1e-10f);
  }
};

std::vector<TransformHandle> getChildren(const SceneGraph& graph,
                                         TransformHandle parent) {
  std::vector<TransformHandle> children;
  for (TransformHandle child = graph.getFirstChild(parent); child;
       child = graph.getNextSibling(child)) {
    children.push_back(child);
  }
  return children;
}

}  // namespace

int main() {
  "entities own their transform and render proxy"_test = [] {
    World w;
    const MeshHandle mesh = w.meshes.create(makeQuads(1));
    const EntityHandle parent =
        w.entities.create({.transform = {.position = Vec3(1, 0, 0)}, .mesh = mesh});
    expect(parent.valid()) << fatal;
    const TransformHandle parent_transform = w.entities.getTransform(parent);

    // Transforms without entity may sit between two entities.
    const TransformHandle raw = w.graph.create({.parent = parent_transform});
    const EntityHandle child = w.entities.create(
        {.transform = {.parent = raw, .position = Vec3(0, 2, 0)}, .mesh = mesh});
    expect(child.valid()) << fatal;
    const TransformHandle child_transform = w.entities.getTransform(child);
    const EntityHandle leaf =
        w.entities.create({.transform = {.parent = child_transform}});
    expect(leaf.valid()) << fatal;

    expect(w.entities.getCount() == 3u && w.scene.getCount() == 2u);
    expect(w.graph.getCount() == 4u && w.graph.getParent(child_transform) == raw);
    expect(w.entities.isAlive(parent) && w.entities.isAlive(child) &&
           w.entities.isAlive(leaf));
    expect(w.entities.findByTransform(child_transform) == child);
    expect(w.entities.findByTransform(w.entities.getTransform(leaf)) == leaf);
    expect(!w.entities.findByTransform(raw));

    const RenderProxyHandle parent_proxy = w.entities.getRenderProxy(parent);
    const RenderProxyHandle child_proxy = w.entities.getRenderProxy(child);
    expect(!w.entities.getRenderProxy(leaf) && w.meshes.getRefCount(mesh) == 2u);
    expect(w.scene.getTransform(child_proxy) == child_transform);
    expect(w.scene.getMesh(child_proxy) == mesh);
    w.graph.update();
    expect(w.isWorldAt(child_transform, Vec3(1, 2, 0)));

    // The descendant entities are destroyed with their components.
    w.entities.destroy(child);
    expect(!w.entities.isAlive(child) && !w.entities.isAlive(leaf));
    expect(w.entities.isAlive(parent) && w.entities.getCount() == 1u);
    expect(!w.scene.isAlive(child_proxy) && w.scene.isAlive(parent_proxy));
    expect(w.meshes.getRefCount(mesh) == 1u);
    expect(w.graph.getCount() == 2u && w.graph.isAlive(raw));
    w.entities.destroy(child);
    w.entities.destroy(leaf);
    expect(w.entities.getCount() == 1u);

    w.entities.destroy(parent);
    expect(w.entities.getCount() == 0u && w.scene.getCount() == 0u);
    expect(w.graph.getCount() == 0u && w.meshes.getRefCount(mesh) == 0u);
  };

  "entities whose transform is destroyed directly"_test = [] {
    World w;
    const MeshHandle mesh = w.meshes.create(makeQuads(1));
    const EntityHandle a = w.entities.create({.mesh = mesh});
    expect(a.valid()) << fatal;
    const TransformHandle a_transform = w.entities.getTransform(a);
    const RenderProxyHandle a_proxy = w.entities.getRenderProxy(a);

    // The record stays until destroy(), the proxy until the next sync.
    w.graph.destroy(a_transform);
    expect(!w.entities.isAlive(a) && w.entities.getCount() == 1u);
    expect(w.entities.getTransform(a) == a_transform);
    expect(w.scene.isAlive(a_proxy));
    w.sync();
    expect(!w.scene.isAlive(a_proxy) && w.meshes.getRefCount(mesh) == 0u);

    // The transform and proxy slots are reused before the dead entity is released.
    const EntityHandle b = w.entities.create({.mesh = mesh});
    expect(b.valid()) << fatal;
    const TransformHandle b_transform = w.entities.getTransform(b);
    const RenderProxyHandle b_proxy = w.entities.getRenderProxy(b);
    expect(b_transform.index() == a_transform.index());
    expect(b_proxy.index() == a_proxy.index());
    expect(w.entities.findByTransform(b_transform) == b);
    w.entities.destroy(a);
    expect(w.entities.getCount() == 1u && w.entities.isAlive(b));
    expect(w.scene.isAlive(b_proxy) && w.entities.findByTransform(b_transform) == b);

    // Released before the sync, the proxy is destroyed immediately and the stale
    // transform does not take the proxy of the entity reusing it.
    w.graph.destroy(b_transform);
    const EntityHandle c = w.entities.create({.mesh = mesh});
    expect(c.valid()) << fatal;
    const TransformHandle c_transform = w.entities.getTransform(c);
    expect(c_transform.index() == b_transform.index() && w.scene.getCount() == 2u);
    w.entities.destroy(b);
    expect(!w.scene.isAlive(b_proxy) && w.scene.getCount() == 1u);
    expect(w.entities.findByTransform(c_transform) == c);
    w.sync();
    expect(w.entities.isAlive(c) && w.scene.isAlive(w.entities.getRenderProxy(c)));

    w.entities.destroy(c);
    expect(w.entities.getCount() == 0u && w.scene.getCount() == 0u);
    expect(w.graph.getCount() == 0u && w.meshes.getRefCount(mesh) == 0u);
  };

  "invalid entities are rejected without leaking"_test = [] {
    World w;
    const MeshHandle mesh = w.meshes.create(makeQuads(2));
    const TransformHandle dead_parent = w.graph.create();
    w.graph.destroy(dead_parent);
    const MeshHandle dead_mesh = w.meshes.create(makeQuads(1));
    w.meshes.destroy(dead_mesh);
    const MaterialHandle dead_material = w.materials.create({});
    w.materials.destroy(dead_material);
    const MaterialHandle materials[] = {MaterialHandle{}, dead_material};

    expect(!w.entities.create({.transform = {.parent = dead_parent}}));
    expect(!w.entities.create({.mesh = dead_mesh}));
    expect(!w.entities.create({.mesh = mesh, .materials = materials}));
    expect(w.entities.getCount() == 0u && w.scene.getCount() == 0u);
    expect(w.graph.getCount() == 0u && w.meshes.getRefCount(mesh) == 0u);
  };

  "models are registered, instantiated and released"_test = [] {
    World w;
    GltfImportResult imported;
    imported.materials.resize(2);
    imported.materials[0].name = "m0";
    imported.materials[1].name = "m1";
    auto malformed = std::make_shared<MeshAsset>(*makeQuads(1));
    malformed->indices[0] = malformed->getVertexCount();
    // Mesh 1 failed to import, mesh 2 is rejected by the registry.
    imported.meshes = {{makeQuads(2), {0, -1}}, {}, {malformed, {1}}};
    imported.nodes = {
        {.name = "a", .parent = -1, .position = Vec3(1, 0, 0), .mesh = 0},
        {.name = "b", .parent = 0, .position = Vec3(0, 2, 0)},
        {.name = "c", .parent = 1, .position = Vec3(0, 0, 3), .mesh = 0},
        {.name = "d", .parent = -1, .mesh = 2},
    };

    Model model = registerModel(imported, w.meshes, w.materials);
    expect(model.materials.size() == 2u && model.meshes.size() == 3u) << fatal;
    expect(model.nodes.size() == 4u && model.nodes[2].name == "c");
    expect(w.materials.get(model.materials[1]).name == "m1");
    expect(w.meshes.isAlive(model.meshes[0]) && !model.meshes[1] && !model.meshes[2]);
    expect(w.meshes.getDefaultMaterial(model.meshes[0], 0) == model.materials[0]);
    expect(w.meshes.getDefaultMaterial(model.meshes[0], 1) == w.materials.getDefault());
    expect(w.materials.getRefCount(model.materials[0]) == 1u);
    expect(w.materials.getRefCount(model.materials[1]) == 0u);

    const EntityHandle root = instantiateModel(
        model, w.entities,
        {.position = Vec3(0, 0, 10), .mobility = ETransformMobility::Static});
    expect(root.valid()) << fatal;
    expect(w.entities.getCount() == 5u && w.scene.getCount() == 2u);
    expect(w.graph.getCount() == 5u);

    // The glTF roots are parented to the root entity.
    const TransformHandle root_transform = w.entities.getTransform(root);
    const std::vector<TransformHandle> roots = getChildren(w.graph, root_transform);
    expect(roots.size() == 2u) << fatal;
    const TransformHandle a = w.graph.getFirstChild(roots[0]) ? roots[0] : roots[1];
    const TransformHandle d = a == roots[0] ? roots[1] : roots[0];
    const TransformHandle b = w.graph.getFirstChild(a);
    expect(b.valid()) << fatal;
    const TransformHandle c = w.graph.getFirstChild(b);
    expect(c.valid()) << fatal;
    expect(!w.graph.getFirstChild(c) && !w.graph.getFirstChild(d));
    expect(!w.graph.getNextSibling(b) && !w.graph.getNextSibling(c));

    w.graph.update();
    expect(w.isWorldAt(root_transform, Vec3(0, 0, 10)) && w.isWorldAt(a, Vec3(1, 0, 10)));
    expect(w.isWorldAt(b, Vec3(1, 2, 10)) && w.isWorldAt(c, Vec3(1, 2, 13)));
    expect(w.isWorldAt(d, Vec3(0, 0, 10)));
    for (const TransformHandle transform : {root_transform, a, b, c, d}) {
      expect(w.entities.findByTransform(transform).valid());
      expect(w.graph.getMobility(transform) == ETransformMobility::Static);
    }
    expect(!w.getProxy(root_transform) && !w.getProxy(b) && !w.getProxy(d));
    expect(w.scene.getMesh(w.getProxy(a)) == model.meshes[0]);
    expect(w.scene.getMesh(w.getProxy(c)) == model.meshes[0]);
    expect(w.scene.getMaterial(w.getProxy(c), 0) == model.materials[0]);
    expect(w.meshes.getRefCount(model.meshes[0]) == 2u);

    const EntityHandle movable = instantiateModel(model, w.entities);
    expect(movable.valid()) << fatal;
    const TransformHandle movable_transform = w.entities.getTransform(movable);
    expect(w.entities.getCount() == 10u && w.scene.getCount() == 4u);
    expect(w.graph.getMobility(movable_transform) == ETransformMobility::Movable);
    expect(w.graph.getMobility(w.graph.getFirstChild(movable_transform)) ==
           ETransformMobility::Movable);

    // A failed instantiation destroys the entities it created.
    const MeshHandle stale = w.meshes.create(makeQuads(1));
    w.meshes.destroy(stale);
    Model broken = model;
    broken.meshes.push_back(stale);
    broken.nodes[2].mesh = 3;
    const uint32 transform_count = w.graph.getCount();
    expect(!instantiateModel(broken, w.entities));
    const TransformHandle dead_parent = w.graph.create();
    w.graph.destroy(dead_parent);
    expect(!instantiateModel(model, w.entities, {.parent = dead_parent}));
    expect(w.entities.getCount() == 10u && w.scene.getCount() == 4u);
    expect(w.graph.getCount() == transform_count);
    expect(w.meshes.getRefCount(model.meshes[0]) == 4u);

    w.entities.destroy(root);
    expect(!w.entities.isAlive(root) && w.entities.getCount() == 5u);
    expect(w.scene.getCount() == 2u && w.graph.getCount() == 5u);
    w.entities.destroy(movable);
    expect(w.entities.getCount() == 0u && w.scene.getCount() == 0u);

    releaseModel(model, w.meshes, w.materials);
    expect(model.meshes.empty() && model.materials.empty() && model.nodes.empty());
    expect(w.meshes.getCount() == 0u && w.materials.getCount() == 1u);
    expect(w.materials.getRefCount(w.materials.getDefault()) == 0u);
  };

  return 0;
}
