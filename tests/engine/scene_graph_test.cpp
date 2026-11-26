#include "amaryl/engine/scene/scene_graph.h"

#include <random>
#include <vector>

#include "amaryl/engine/asset/asset_importer.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

void validate(const SceneGraph& graph) {
  const auto handles = graph.getDenseHandles();
  for (uint32 i = 0; i < graph.getDenseCount(); ++i) {
    const TransformHandle handle = handles[i];
    expect(graph.isAlive(handle));
    const TransformHandle parent = graph.getParent(handle);
    uint32 depth = 0;
    for (TransformHandle ancestor = parent; ancestor;
      ancestor = graph.getParent(ancestor)) {
      ++depth;
    }
    expect(graph.getDepth(handle) == depth);
    expect(graph.getWorld(handle).IsClose(graph.computeWorldImmediate(handle), 1e-6f));
    if (parent) {
      // Parents before children.
      uint32 parent_index = 0;
      while (handles[parent_index] != parent) {
        ++parent_index;
      }
      expect(parent_index < i);
      bool found = false;
      for (TransformHandle child = graph.getFirstChild(parent); child;
        child = graph.getNextSibling(child)) {
        found |= child == handle;
      }
      expect(found);
    }
  }

  // Chunk versions are upper bounds of their node versions.
  const auto world_versions = graph.getDenseWorldVersions();
  const auto chunk_versions = graph.getChunkWorldVersions();
  expect(chunk_versions.size() == (graph.getDenseCount() + 63) / 64);
  for (uint32 i = 0; i < graph.getDenseCount(); ++i) {
    expect(world_versions[i] <= chunk_versions[i / 64]);
  }
}

std::vector<TransformHandle> collectDestroyed(
  const SceneGraph& graph, uint32 since, bool& complete) {
  std::vector<TransformHandle> out;
  complete = graph.forEachDestroyedSince(
    since, [&](TransformHandle handle) { out.push_back(handle); });
  return out;
}

size_t indexOf(const std::vector<TransformHandle>& handles, TransformHandle handle) {
  for (size_t i = 0; i < handles.size(); ++i) {
    if (handles[i] == handle) {
      return i;
    }
  }
  return handles.size();
}

// Tracks transforms like RenderScene does.
struct Consumer {
  std::vector<TransformHandle> byIndex;
  uint32 synced = 0;
  uint32 updatesSinceSync = 0;
  uint32 fallbacks = 0;

  void sync(const SceneGraph& graph) {
    const bool complete =
      graph.forEachDestroyedSince(synced, [&](TransformHandle handle) {
        if (handle.index() < byIndex.size() && byIndex[handle.index()] == handle) {
          byIndex[handle.index()] = {};
        }
      });
    if (!complete) {
      ++fallbacks;
      for (TransformHandle& handle : byIndex) {
        if (handle && !graph.isAlive(handle)) {
          handle = {};
        }
      }
    }
    graph.forEachChangedSince(synced, [&](TransformHandle handle, const Mat44&, bool) {
      if (handle.index() >= byIndex.size()) {
        byIndex.resize(handle.index() + 1);
      }
      // A reused slot must have been released by the destroy pass first.
      expect(!byIndex[handle.index()] || byIndex[handle.index()] == handle);
      byIndex[handle.index()] = handle;
    });
    synced = graph.getWorldVersion();
    updatesSinceSync = 0;
  }

  void verify(const std::vector<TransformHandle>& live) const {
    size_t tracked = 0;
    for (const TransformHandle handle : byIndex) {
      tracked += handle ? 1 : 0;
    }
    expect(tracked == live.size());
    for (const TransformHandle handle : live) {
      expect(handle.index() < byIndex.size() && byIndex[handle.index()] == handle);
    }
  }
};

}  // namespace

int main() {
  "destroy log reports destroyed handles"_test = [] {
    SceneGraph graph;
    bool complete = false;
    const TransformHandle a = graph.create();
    const TransformHandle b = graph.create({.parent = a});
    const TransformHandle c = graph.create({.parent = b});
    const TransformHandle d = graph.create({.parent = a});
    const TransformHandle e = graph.create();
    const uint32 v0 = graph.update();

    // Recursive destroys log every node, children first, visible after update().
    graph.destroy(a);
    expect(collectDestroyed(graph, v0, complete).empty() && complete);
    const uint32 v1 = graph.update();
    std::vector<TransformHandle> destroyed = collectDestroyed(graph, v0, complete);
    expect(complete && destroyed.size() == 4u);
    expect(indexOf(destroyed, c) < indexOf(destroyed, b));
    expect(indexOf(destroyed, b) < indexOf(destroyed, a));
    expect(indexOf(destroyed, d) < indexOf(destroyed, a));
    expect(indexOf(destroyed, a) < 4u);
    expect(collectDestroyed(graph, v1, complete).empty() && complete);

    // ReparentChildren only logs the destroyed node.
    const TransformHandle f = graph.create({.parent = e});
    const TransformHandle h = graph.create({.parent = f});
    const uint32 v2 = graph.update();
    graph.destroy(f, ETransformDestroyMode::ReparentChildren);
    graph.update();
    destroyed = collectDestroyed(graph, v2, complete);
    expect(complete && destroyed.size() == 1u && destroyed[0] == f);
    expect(graph.isAlive(h) && graph.getParent(h) == e);

    // A slot reused in the same frame only reports the old handle.
    const uint32 v3 = graph.getWorldVersion();
    graph.destroy(h);
    const TransformHandle reused = graph.create();
    expect(reused.index() == h.index() && reused != h);
    graph.update();
    destroyed = collectDestroyed(graph, v3, complete);
    expect(complete && destroyed.size() == 1u && destroyed[0] == h);
    expect(graph.isAlive(reused) && !graph.isAlive(h));

    // Created and destroyed between two updates, still logged.
    const uint32 v4 = graph.getWorldVersion();
    const TransformHandle ghost = graph.create();
    graph.destroy(ghost);
    graph.update();
    destroyed = collectDestroyed(graph, v4, complete);
    expect(complete && destroyed.size() == 1u && destroyed[0] == ghost);
  };

  "destroy log is trimmed by whole updates"_test = [] {
    constexpr uint32 kRetention = SceneGraph::kDestroyLogRetention;
    SceneGraph graph;
    bool complete = false;
    const TransformHandle x = graph.create();
    const TransformHandle y = graph.create();
    graph.update();
    graph.destroy(x);
    const uint32 vx = graph.update();
    for (uint32 i = 0; i < kRetention; ++i) {
      graph.update();
    }
    graph.destroy(y);
    graph.update();

    // Synced right before x was destroyed, kRetention + 1 updates ago: nothing dropped
    // yet.
    std::vector<TransformHandle> destroyed = collectDestroyed(graph, vx - 1, complete);
    expect(complete && destroyed.size() == 2u && destroyed[0] == x && destroyed[1] == y);
    while (graph.getWorldVersion() < vx + 2 * kRetention - 1) {
      graph.update();
    }
    expect(collectDestroyed(graph, vx - 1, complete).size() == 2u && complete);

    // The oldest entry is 2 * kRetention old: every entry at least kRetention old is
    // dropped, y is kept.
    graph.update();
    expect(collectDestroyed(graph, vx - 1, complete).empty() && !complete);
    destroyed = collectDestroyed(graph, vx, complete);
    expect(complete && destroyed.size() == 1u && destroyed[0] == y);
    expect(
      collectDestroyed(graph, graph.getWorldVersion(), complete).empty() && complete);
  };

  "world transforms follow the hierarchy"_test = [] {
    SceneGraph graph;
    const Quat r90 = Quat::sRotation(Vec3::sAxisZ(), 1.5707963f);
    const TransformHandle a = graph.create({.position = Vec3(1, 0, 0), .rotation = r90});
    const TransformHandle b = graph.create({.parent = a, .position = Vec3(1, 0, 0)});
    const TransformHandle c =
      graph.create({.parent = b, .position = Vec3(0, 2, 0), .scale = Vec3(2, 2, 2)});
    expect(graph.isWorldStale(c));
    const uint32 v1 = graph.update();
    expect(!graph.isWorldStale(c));
    // a * T(1, 0, 0) = T(1, 0, 0) R90 T(1, 0, 0) -> (1, 1, 0)
    expect(graph.getWorld(b).GetTranslation().IsClose(Vec3(1, 1, 0), 1e-10f));
    validate(graph);

    // New nodes are reported as teleported.
    int changed = 0;
    graph.forEachChangedSince(0, [&](TransformHandle, const Mat44&, bool teleported) {
      ++changed;
      expect(teleported);
    });
    expect(changed == 3);

    // Moving a changes every descendant.
    graph.setLocalPosition(a, Vec3(5, 0, 0));
    const uint32 v2 = graph.update();
    changed = 0;
    graph.forEachChangedSince(v1, [&](TransformHandle, const Mat44&, bool teleported) {
      ++changed;
      expect(!teleported);
    });
    expect(changed == 3);
    validate(graph);

    // Teleports propagate to the descendants.
    graph.teleport(b);
    graph.update();
    int teleports = 0;
    graph.forEachChangedSince(v2,
      [&](TransformHandle, const Mat44&, bool teleported) { teleports += teleported; });
    expect(teleports == 2);

    // Reparenting to a later node with KeepWorld reorders the nodes.
    const TransformHandle d = graph.create({.position = Vec3(3, 4, 5),
      .rotation = Quat::sRotation(Vec3::sAxisY(), 0.7f),
      .scale = Vec3(1.5f, 1.5f, 1.5f)});
    graph.update();
    const Mat44 world_b = graph.getWorld(b);
    Mat44 world_c = graph.getWorld(c);
    graph.setParent(b, d, EAttachRule::KeepWorld);
    expect(graph.computeWorldImmediate(b).IsClose(world_b, 1e-8f));
    graph.update();
    expect(graph.getWorld(b).IsClose(world_b, 1e-8f));
    expect(graph.getWorld(c).IsClose(world_c, 1e-8f));
    expect(graph.getDepth(c) == 2u);
    validate(graph);

    graph.setWorldTRS(c, Vec3(7, 8, 9), Quat::sIdentity(), Vec3(1, 1, 1));
    graph.update();
    expect(graph.getWorld(c).IsClose(Mat44::sTranslation(Vec3(7, 8, 9)), 1e-8f));

    // ReparentChildren keeps the world transform of the children.
    world_c = graph.getWorld(c);
    graph.destroy(b, ETransformDestroyMode::ReparentChildren);
    expect(!graph.isAlive(b));
    expect(graph.getParent(c) == d);
    graph.update();
    expect(graph.getWorld(c).IsClose(world_c, 1e-8f));
    validate(graph);
  };

  "random operations keep consumers in sync"_test = [] {
    SceneGraph graph;
    std::mt19937 rng(42);
    std::vector<TransformHandle> live;
    for (int i = 0; i < 3; ++i) {
      live.push_back(graph.create({.position = Vec3(float(i), 0, 0)}));
    }

    Consumer every;  // Syncs after every update.
    Consumer eager;  // Syncs every kDestroyLogRetention updates, never falls back.
    Consumer lazy;   // Syncs every 3 * kDestroyLogRetention updates, falls back.
    const auto update = [&] {
      graph.update();
      for (Consumer* consumer : {&every, &eager, &lazy}) {
        ++consumer->updatesSinceSync;
      }
      every.sync(graph);
      every.verify(live);
      if (eager.updatesSinceSync == SceneGraph::kDestroyLogRetention) {
        eager.sync(graph);
        eager.verify(live);
      }
      if (lazy.updatesSinceSync == 3 * SceneGraph::kDestroyLogRetention) {
        lazy.sync(graph);
        lazy.verify(live);
      }
    };
    graph.update();
    for (Consumer* consumer : {&every, &eager, &lazy}) {
      consumer->sync(graph);
      consumer->verify(live);
    }

    for (int iteration = 0; iteration < 6000; ++iteration) {
      const uint32 op = rng() % 10;
      if (op < 4 || live.size() < 3) {
        TransformDesc desc;
        if (!live.empty() && rng() % 3) {
          desc.parent = live[rng() % live.size()];
        }
        desc.position = Vec3(float(rng() % 10), float(rng() % 10), float(rng() % 10));
        desc.rotation = Quat::sRotation(Vec3::sAxisX(), float(rng() % 100) * 0.01f);
        live.push_back(graph.create(desc));
      } else if (op < 6) {
        const TransformHandle child = live[rng() % live.size()];
        const TransformHandle parent =
          rng() % 4 ? live[rng() % live.size()] : TransformHandle{};
        bool cycle = false;
        for (TransformHandle node = parent; node; node = graph.getParent(node)) {
          cycle |= node == child;
        }
        if (!cycle) {
          update();
          const Mat44 world = graph.getWorld(child);
          graph.setParent(child, parent, EAttachRule::KeepWorld);
          update();
          expect(graph.getWorld(child).IsClose(world, 1e-4f));
        }
      } else if (op < 8) {
        graph.destroy(
          live[rng() % live.size()], rng() % 2 ? ETransformDestroyMode::Recursive
                                               : ETransformDestroyMode::ReparentChildren);
        std::erase_if(
          live, [&](TransformHandle handle) { return !graph.isAlive(handle); });
      } else {
        graph.setLocalPosition(live[rng() % live.size()], Vec3(float(rng() % 10), 0, 0));
      }
      expect(graph.getCount() == live.size());
      if (iteration % 50 == 0) {
        update();
        validate(graph);
      }
    }
    update();
    validate(graph);
    expect(every.fallbacks == 0u);
    expect(eager.fallbacks == 0u);
    expect(lazy.fallbacks > 0u);
  };

  "import gltf as scenegraph"_test = [] {
    MeshBuildOptions mesh_build_options;
    GltfImportOptions gltf_import_options;
    GltfImportResult result;

    importGltf("../../EngineAssets/Bistro/bistro.gltf", gltf_import_options, result);

    for (int32 i = 0; i < result.scene.nodes.size() && i < 10; ++i) {
      const SceneNodeAsset& node = result.scene.nodes[i];
      printf("%s:\n\tparent: %d\n\tposition: %.3f, %.3f, %.3f\n\tmesh: %d\n",
        node.name.c_str(), node.parent, node.position.GetX(), node.position.GetY(),
        node.position.GetZ(), node.mesh);
    }
  };

  return 0;
}
