#include "amaryl/engine/scene/entity.h"

#include <memory>
#include <vector>

#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"
#include "amaryl/engine/render/render_scene.h"
#include "amaryl/engine/scene/model.h"
#include "amaryl/engine/scene/scene_graph.h"
#include "amaryl/engine/scene/scene_loader.h"
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

  "models are registered and released"_test = [] {
    World w;
    ModelAsset asset;
    asset.materials.resize(2);
    asset.materials[0].name = "m0";
    asset.materials[1].name = "m1";
    auto malformed = std::make_shared<MeshAsset>(*makeQuads(1));
    malformed->indices[0] = malformed->getVertexCount();
    // Mesh 1 failed to import, mesh 2 is rejected by the registry.
    asset.meshes = {{makeQuads(2), {0, -1}}, {}, {malformed, {1}}};

    Model model = registerModel(asset, w.meshes, w.materials);
    expect(model.materials.size() == 2u && model.meshes.size() == 3u) << fatal;
    expect(w.materials.get(model.materials[1]).name == "m1");
    expect(w.meshes.isAlive(model.meshes[0]) && !model.meshes[1] && !model.meshes[2]);
    expect(w.meshes.getDefaultMaterial(model.meshes[0], 0) == model.materials[0]);
    expect(w.meshes.getDefaultMaterial(model.meshes[0], 1) == w.materials.getDefault());
    expect(w.materials.getRefCount(model.materials[0]) == 1u);
    expect(w.materials.getRefCount(model.materials[1]) == 0u);

    releaseModel(model, w.meshes, w.materials);
    expect(model.meshes.empty() && model.materials.empty());
    expect(w.meshes.getCount() == 0u && w.materials.getCount() == 1u);
    expect(w.materials.getRefCount(w.materials.getDefault()) == 0u);
  };

  "scene nodes are added into the existing graph"_test = [] {
    World w;
    ModelAsset asset;
    asset.materials.resize(1);
    asset.meshes = {{makeQuads(1), {0}}, {}};
    Model model = registerModel(asset, w.meshes, w.materials);

    SceneAsset scene;
    scene.nodes = {
        {.name = "a", .parent = -1, .position = Vec3(1, 0, 0), .mesh = 0},
        {.name = "b", .parent = 0, .position = Vec3(0, 2, 0), .mesh = 1},
        {.name = "c", .parent = 1, .position = Vec3(0, 0, 3), .mesh = 0},
        {.name = "cam", .parent = -1, .position = Vec3(0, 0, 5), .camera = 0},
        {.name = "sun", .parent = -1, .light = 0},
        {.name = "lamp", .parent = 3, .light = 1},
    };
    scene.cameras.push_back({.name = "persp", .fovY = 0.8f, .aspect = 2.0f});
    scene.lights.push_back({.name = "sun", .color = Vec3(1, 0, 0), .intensity = 5.0f});
    scene.lights.push_back({.name = "lamp", .type = ELightType::Point});

    // Existing hierarchy, the nodes go directly under `parent` without wrapper root.
    const EntityHandle anchor =
        w.entities.create({.transform = {.position = Vec3(0, 0, 10),
                                         .mobility = ETransformMobility::Static}});
    const EntityHandle sibling =
        w.entities.create({.transform = {.parent = w.entities.getTransform(anchor),
                                         .mobility = ETransformMobility::Static}});
    expect(anchor.valid() && sibling.valid()) << fatal;
    const TransformHandle anchor_transform = w.entities.getTransform(anchor);

    std::vector<EntityHandle> nodes;
    expect(addSceneNodes(
        scene, model, w.entities,
        {.parent = anchor_transform, .mobility = ETransformMobility::Static}, nodes))
        << fatal;
    expect(nodes.size() == scene.nodes.size()) << fatal;
    expect(w.entities.getCount() == 8u && w.graph.getCount() == 8u);
    expect(w.scene.getCount() == 2u && w.entities.isAlive(sibling));

    std::vector<TransformHandle> t;
    for (const EntityHandle node : nodes) {
      expect(w.entities.isAlive(node));
      t.push_back(w.entities.getTransform(node));
      expect(w.graph.getMobility(t.back()) == ETransformMobility::Static);
    }
    expect(getChildren(w.graph, anchor_transform).size() == 4u);
    expect(w.graph.getParent(t[0]) == anchor_transform);
    expect(w.graph.getParent(t[1]) == t[0] && w.graph.getParent(t[2]) == t[1]);
    expect(w.graph.getParent(t[3]) == anchor_transform);
    expect(w.graph.getParent(t[4]) == anchor_transform &&
           w.graph.getParent(t[5]) == t[3]);

    w.graph.update();
    w.entities.updateComponents();
    expect(w.isWorldAt(t[0], Vec3(1, 0, 10)) && w.isWorldAt(t[2], Vec3(1, 2, 13)));
    expect(w.scene.getMesh(w.entities.getRenderProxy(nodes[0])) == model.meshes[0]);
    expect(!w.entities.getRenderProxy(nodes[1]));
    expect(w.scene.getMesh(w.entities.getRenderProxy(nodes[2])) == model.meshes[0]);

    const Camera* camera = w.entities.getCamera(nodes[3]);
    expect(camera != nullptr) << fatal;
    expect(camera->getProjectionType() == ECameraProjection::Perspective);
    expect(camera->getFovY() == 0.8f && camera->getAspect() == 2.0f);
    expect(camera->getPosition().IsClose(Vec3(0, 0, 15), 1e-10f));
    const Light* sun = w.entities.getLight(nodes[4]);
    expect(sun != nullptr) << fatal;
    expect(sun->getType() == ELightType::Directional);
    expect(sun->getColor() == Vec3(1, 0, 0) && sun->getIntensity() == 5.0f);
    // Unsupported lights only keep their transform.
    expect(!w.entities.getLight(nodes[5]) && !w.entities.getCamera(nodes[4]));

    // Static nodes cannot go under a movable parent, nothing is added.
    const EntityHandle movable = w.entities.create({});
    expect(!addSceneNodes(scene, model, w.entities,
                          {.parent = w.entities.getTransform(movable),
                           .mobility = ETransformMobility::Static},
                          nodes));
    expect(nodes.empty() && w.entities.getCount() == 9u && w.graph.getCount() == 9u);

    // A failure after some nodes were added destroys them.
    const MeshHandle stale = w.meshes.create(makeQuads(1));
    w.meshes.destroy(stale);
    Model broken = model;
    broken.meshes.push_back(stale);
    SceneAsset broken_scene = scene;
    broken_scene.nodes[5].mesh = 2;
    expect(!addSceneNodes(broken_scene, broken, w.entities, {}, nodes));
    // Out of range indices are rejected before adding anything.
    broken_scene.nodes[5].mesh = 3;
    expect(!addSceneNodes(broken_scene, broken, w.entities, {}, nodes));
    broken_scene.nodes[5] = {.parent = 5};
    expect(!addSceneNodes(broken_scene, model, w.entities, {}, nodes));
    expect(nodes.empty() && w.entities.getCount() == 9u && w.graph.getCount() == 9u);
    expect(w.scene.getCount() == 2u && w.meshes.getRefCount(model.meshes[0]) == 2u);

    // Movable by default, the scene roots become graph roots.
    expect(addSceneNodes(scene, model, w.entities, {}, nodes)) << fatal;
    expect(!w.graph.getParent(w.entities.getTransform(nodes[0])));
    expect(w.graph.getMobility(w.entities.getTransform(nodes[0])) ==
           ETransformMobility::Movable);
    expect(w.entities.getCount() == 15u && w.scene.getCount() == 4u);

    w.entities.destroy(anchor);
    expect(w.entities.getCount() == 7u && w.scene.getCount() == 2u);
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (scene.nodes[i].parent < 0) {
        w.entities.destroy(nodes[i]);
      }
    }
    w.entities.destroy(movable);
    expect(w.entities.getCount() == 0u && w.scene.getCount() == 0u);
    expect(w.graph.getCount() == 0u);
    releaseModel(model, w.meshes, w.materials);
  };

  "cameras and lights follow their entity"_test = [] {
    World w;
    const EntityHandle parent = w.entities.create(
        {.transform = {.position = Vec3(0, 0, 4), .scale = Vec3(2, 2, 2)}});
    const EntityHandle child =
        w.entities.create({.transform = {.parent = w.entities.getTransform(parent),
                                         .position = Vec3(1, 0, 0)}});
    expect(parent.valid() && child.valid()) << fatal;
    expect(!w.entities.getCamera(child) && !w.entities.getLight(child));

    // The world transform is set when attached, without scale.
    Camera* camera = w.entities.setCamera(child, std::make_unique<Camera>());
    expect(camera != nullptr && w.entities.getCamera(child) == camera) << fatal;
    expect(camera->getPosition().IsClose(Vec3(2, 0, 4), 1e-10f));
    expect(camera->getRight().IsClose(Vec3(1, 0, 0), 1e-10f));
    Light* light = w.entities.setLight(child, std::make_unique<DirectionalLight>());
    expect(light != nullptr && w.entities.getLight(child) == light) << fatal;
    expect(light->getDirection().IsClose(Vec3(0, 0, -1), 1e-10f));

    // Updated after the graph.
    const TransformHandle parent_transform = w.entities.getTransform(parent);
    w.graph.setLocalRotation(parent_transform,
                             Quat::sRotation(Vec3::sAxisY(), 0.5f * PI));
    w.graph.update();
    w.entities.updateComponents();
    expect(camera->getPosition().IsClose(Vec3(0, 0, 2), 1e-10f));
    expect(camera->getForward().IsClose(Vec3(-1, 0, 0), 1e-10f));
    expect(light->getDirection().IsClose(Vec3(-1, 0, 0), 1e-10f));

    // Null detaches, destroying the entity releases its components.
    expect(w.entities.setCamera(child, nullptr) == nullptr);
    expect(!w.entities.getCamera(child) && w.entities.getLight(child) == light);
    w.entities.destroy(parent);
    const EntityHandle reused = w.entities.create({});
    expect(reused.index() == child.index() || reused.index() == parent.index());
    expect(!w.entities.getCamera(reused) && !w.entities.getLight(reused));
    w.entities.updateComponents();
    w.entities.destroy(reused);
  };

  "static entities cannot have a movable parent"_test = [] {
    World w;
    const EntityHandle movable = w.entities.create({});
    expect(movable.valid()) << fatal;
    expect(!w.entities.create({.transform = {.parent = w.entities.getTransform(movable),
                                             .mobility = ETransformMobility::Static}}));
    expect(w.entities.getCount() == 1u && w.graph.getCount() == 1u);
    w.entities.destroy(movable);
  };

  return 0;
}
