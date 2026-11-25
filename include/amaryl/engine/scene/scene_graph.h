#pragma once

#include <algorithm>
#include <vector>

#include "amaryl/core/span.h"
#include "amaryl/engine/scene/common.h"

namespace amaryl {

// CPU transform hierarchy, the source of world transforms for gameplay, physics and the
// GPU scene.
//
// - Writes are deferred: world matrices are only recomputed by `update()`, `getWorld()`
//   returns the result of the last `update()`. Use `computeWorldImmediate()` for reads
//   that cannot wait (cameras, sockets).
// - Nodes are stored densely in parent-before-child order so `update()` is one linear
//   pass.
//   Dense indices are internal and change on `setParent()` / `destroy()`, handles do not.
// - Change tracking uses monotonically increasing versions instead of dirty bits, so
//   every consumer independently asks "what changed since version N" and nothing has to
//   be reset.
// - Destroyed handles are kept in a log for at least `kDestroyLogRetention` updates so
//   consumers can release what they attached to them, see `forEachDestroyedSince()`.
// - Writes are single threaded. Reads are safe from any thread between `update()` and the
//   next write.
struct ENGINE_API SceneGraph final {
  static constexpr uint32 kChunkSize = 64;
  static constexpr uint32 kInvalidIndex = 0xffffffffu;
  // Minimum number of `update()` calls a destroyed handle stays in the destroy log.
  static constexpr uint32 kDestroyLogRetention = 64;

  explicit SceneGraph(uint32 reserve_count = 0);
  ~SceneGraph();

  NO_COPYABLE(SceneGraph);

  // Lifetime

  TransformHandle create(const TransformDesc& desc = {});
  void destroy(TransformHandle handle,
    ETransformDestroyMode mode = ETransformDestroyMode::Recursive);
  bool isAlive(TransformHandle handle) const;
  uint32 getCount() const { return static_cast<uint32>(dense_handles_.size()); }

  // Hierarchy

  // An empty `parent` detaches `child` to the root.
  void setParent(TransformHandle child, TransformHandle parent,
    EAttachRule rule = EAttachRule::KeepWorld);
  TransformHandle getParent(TransformHandle handle) const;
  TransformHandle getFirstChild(TransformHandle handle) const;
  TransformHandle getNextSibling(TransformHandle handle) const;
  uint32 getDepth(TransformHandle handle) const;
  ETransformMobility getMobility(TransformHandle handle) const;

  // Local transform, deferred until `update()`

  void setLocalPosition(TransformHandle handle, Vec3Arg position);
  void setLocalRotation(TransformHandle handle, QuatArg rotation);
  void setLocalScale(TransformHandle handle, Vec3Arg scale);
  void setLocalTRS(
    TransformHandle handle, Vec3Arg position, QuatArg rotation, Vec3Arg scale);

  Vec3 getLocalPosition(TransformHandle handle) const;
  Quat getLocalRotation(TransformHandle handle) const;
  Vec3 getLocalScale(TransformHandle handle) const;

  // Converted to local space with the parent's immediately computed world transform.
  void setWorldTRS(
    TransformHandle handle, Vec3Arg position, QuatArg rotation, Vec3Arg scale);

  // Marks a discontinuity (spawn, respawn, camera cut): consumers must not interpolate or
  // generate motion vectors across it. Propagated to descendants by `update()`.
  void teleport(TransformHandle handle);

  // World transform

  // Asserts in debug builds when the node or one of its ancestors has pending writes.
  const Mat44& getWorld(TransformHandle handle) const;
  bool isWorldStale(TransformHandle handle) const;
  // Walks the ancestor chain, O(depth). Does not modify the cached world matrix.
  Mat44 computeWorldImmediate(TransformHandle handle) const;

  // Update

  // Restores parent-before-child order if the hierarchy changed, recomputes stale world
  // matrices and returns the new world version.
  uint32 update();
  uint32 getWorldVersion() const { return world_version_; }

  // Data-oriented access for consumers, valid until the next write. Indexed by dense
  // index.

  uint32 getDenseCount() const { return static_cast<uint32>(dense_handles_.size()); }
  Span<const TransformHandle> getDenseHandles() const { return dense_handles_; }
  Span<const Mat44> getDenseWorlds() const { return worlds_; }
  Span<const uint32> getDenseWorldVersions() const { return world_versions_; }
  Span<const uint32> getDenseTeleportVersions() const { return teleport_versions_; }
  // One entry per kChunkSize nodes: the highest world version inside the chunk.
  Span<const uint32> getChunkWorldVersions() const { return chunk_world_versions_; }

  // Visits nodes whose world matrix changed after `since_version`, skipping whole chunks.
  // fn(TransformHandle handle, const Mat44& world, bool teleported)
  template <typename Fn>
  void forEachChangedSince(uint32 since_version, Fn&& fn) const;

  // Visits handles destroyed after `since_version`, in destruction order. Like world
  // changes, destroys become visible at the next `update()`. The slot of a destroyed
  // handle can already be reused by a new node, so match full handles, not `index()`.
  // Returns false without visiting anything when entries after `since_version` were
  // already dropped from the log, the consumer must then check every handle it holds with
  // `isAlive()`. Consumers that sync at least once every kDestroyLogRetention updates
  // never miss an entry.
  // fn(TransformHandle handle)
  template <typename Fn>
  [[nodiscard]] bool forEachDestroyedSince(uint32 since_version, Fn&& fn) const;

 private:
  uint32 toDense(TransformHandle handle) const;
  uint32 getPendingVersion() const { return world_version_ + 1; }
  void markLocalChanged(uint32 dense_index);
  void restoreHierarchyOrder();
  void trimDestroyLog();

  // Handle -> dense index.
  Pool<TransformTag, uint32> dense_lookup_;

  // Hot data, read by `update()`.
  std::vector<uint32> parents_;
  std::vector<Vec3> local_positions_;
  std::vector<Quat> local_rotations_;
  std::vector<Vec3> local_scales_;
  std::vector<Mat44> worlds_;
  std::vector<uint32> local_versions_;
  std::vector<uint32> world_versions_;
  std::vector<uint32> teleport_versions_;

  // Per chunk, lets `update()` and consumers skip untouched chunks.
  std::vector<uint32> chunk_local_versions_;
  std::vector<uint32> chunk_world_versions_;

  // Cold data.
  std::vector<TransformHandle> dense_handles_;
  std::vector<uint32> first_children_;
  std::vector<uint32> next_siblings_;
  std::vector<uint16> depths_;
  std::vector<ETransformMobility> mobilities_;

  // Destroy log, sorted by version. Entries with a version up to
  // `destroy_log_trimmed_version_` were dropped.
  std::vector<TransformHandle> destroyed_handles_;
  std::vector<uint32> destroyed_versions_;
  uint32 destroy_log_trimmed_version_{0};

  uint32 world_version_{0};
  bool hierarchy_order_dirty_{false};
};

template <typename Fn>
void SceneGraph::forEachChangedSince(uint32 since_version, Fn&& fn) const {
  const uint32 count = getDenseCount();
  const uint32 chunk_count = static_cast<uint32>(chunk_world_versions_.size());
  for (uint32 chunk = 0; chunk < chunk_count; ++chunk) {
    if (chunk_world_versions_[chunk] <= since_version) {
      continue;
    }

    const uint32 end = std::min(count, (chunk + 1) * kChunkSize);
    for (uint32 i = chunk * kChunkSize; i < end; ++i) {
      if (world_versions_[i] > since_version) {
        fn(dense_handles_[i], worlds_[i], teleport_versions_[i] > since_version);
      }
    }
  }
}

template <typename Fn>
bool SceneGraph::forEachDestroyedSince(uint32 since_version, Fn&& fn) const {
  if (since_version < destroy_log_trimmed_version_) {
    return false;
  }

  const auto begin = std::upper_bound(
    destroyed_versions_.begin(), destroyed_versions_.end(), since_version);
  const size_t count = destroyed_versions_.size();
  // Entries stamped with the pending version are published by the next `update()`.
  for (size_t i = static_cast<size_t>(begin - destroyed_versions_.begin());
    i < count && destroyed_versions_[i] <= world_version_; ++i) {
    fn(destroyed_handles_[i]);
  }
  return true;
}

}  // namespace amaryl
