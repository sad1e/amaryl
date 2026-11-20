#pragma once

#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"
#include "amaryl/engine/render/render_scene.h"
#include "amaryl/engine/scene/entity.h"
#include "amaryl/engine/scene/scene_graph.h"

namespace amaryl {

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

}  // namespace amaryl
