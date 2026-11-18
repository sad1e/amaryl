#include "amaryl/engine/scene/scene_graph.h"

namespace amaryl {

namespace {

constexpr uint32 kInvalid = SceneGraph::kInvalidIndex;

FORCE_INLINE Mat44 composeTRS(Vec3Arg position, QuatArg rotation, Vec3Arg scale) {
  return Mat44::sRotationTranslation(rotation, position).PreScaled(scale);
}

void decomposeTRS(Mat44Arg matrix, Vec3& position, Quat& rotation, Vec3& scale) {
  rotation = matrix.Decompose(scale).GetRotation().GetQuaternion().Normalized();
  position = matrix.GetTranslation();
}

FORCE_INLINE uint32 getChunkCount(uint32 count) {
  return (count + SceneGraph::kChunkSize - 1) / SceneGraph::kChunkSize;
}

FORCE_INLINE uint32 remapIndex(const std::vector<uint32>& old_to_new, uint32 index) {
  return index == kInvalid ? kInvalid : old_to_new[index];
}

// Pre-order walk over `root` and its descendants, without recursion or extra memory.
template <typename Fn>
void visitSubtree(const std::vector<uint32>& parents,
                  const std::vector<uint32>& first_children,
                  const std::vector<uint32>& next_siblings, uint32 root, Fn&& fn) {
  uint32 node = root;
  while (true) {
    fn(node);
    if (first_children[node] != kInvalid) {
      node = first_children[node];
      continue;
    }

    while (node != root && next_siblings[node] == kInvalid) {
      node = parents[node];
    }
    if (node == root) {
      return;
    }
    node = next_siblings[node];
  }
}

// Post-order walk over `root` and its descendants, siblings in list order. Removing nodes
// in this order always removes the first child of its parent.
template <typename Fn>
void visitSubtreePostOrder(const std::vector<uint32>& parents,
                           const std::vector<uint32>& first_children,
                           const std::vector<uint32>& next_siblings, uint32 root,
                           Fn&& fn) {
  auto descend = [&](uint32 node) {
    while (first_children[node] != kInvalid) {
      node = first_children[node];
    }
    return node;
  };

  uint32 node = descend(root);
  while (true) {
    fn(node);
    if (node == root) {
      return;
    }
    node = next_siblings[node] != kInvalid ? descend(next_siblings[node]) : parents[node];
  }
}

void linkChild(std::vector<uint32>& first_children, std::vector<uint32>& next_siblings,
               uint32 parent, uint32 child) {
  next_siblings[child] = first_children[parent];
  first_children[parent] = child;
}

// Replaces `child` in the child list of `parent` by `replacement`, which may be
// kInvalidIndex to unlink `child`.
void replaceChild(std::vector<uint32>& first_children, std::vector<uint32>& next_siblings,
                  uint32 parent, uint32 child, uint32 replacement) {
  const uint32 next = next_siblings[child];
  if (replacement != kInvalid) {
    next_siblings[replacement] = next;
  }
  const uint32 link = replacement != kInvalid ? replacement : next;

  if (first_children[parent] == child) {
    first_children[parent] = link;
    return;
  }

  uint32 prev = first_children[parent];
  while (prev != kInvalid && next_siblings[prev] != child) {
    prev = next_siblings[prev];
  }
  ASSERT(prev != kInvalid, "Child is not linked to its parent");
  if (prev != kInvalid) {
    next_siblings[prev] = link;
  }
}

void unlinkChild(std::vector<uint32>& first_children, std::vector<uint32>& next_siblings,
                 uint32 parent, uint32 child) {
  replaceChild(first_children, next_siblings, parent, child, kInvalid);
  next_siblings[child] = kInvalid;
}

void updateSubtreeDepths(const std::vector<uint32>& parents,
                         const std::vector<uint32>& first_children,
                         const std::vector<uint32>& next_siblings,
                         std::vector<uint16>& depths, uint32 root) {
  visitSubtree(parents, first_children, next_siblings, root, [&](uint32 node) {
    const uint32 parent = parents[node];
    const uint32 depth = parent != kInvalid ? depths[parent] + 1u : 0u;
    ASSERT(depth <= 0xffffu, "Transform hierarchy is too deep");
    depths[node] = static_cast<uint16>(depth);
  });
}

// Moves `child` (and its subtree) under `parent`, which may be kInvalidIndex.
void relink(std::vector<uint32>& parents, std::vector<uint32>& first_children,
            std::vector<uint32>& next_siblings, std::vector<uint16>& depths, uint32 child,
            uint32 parent) {
  const uint32 old_parent = parents[child];
  if (old_parent != kInvalid) {
    unlinkChild(first_children, next_siblings, old_parent, child);
  }

  parents[child] = parent;
  if (parent != kInvalid) {
    linkChild(first_children, next_siblings, parent, child);
  }

  updateSubtreeDepths(parents, first_children, next_siblings, depths, child);
}

template <typename T>
void permute(std::vector<T>& values, const std::vector<uint32>& new_to_old) {
  std::vector<T> result;
  result.reserve(values.size());
  for (const uint32 old_index : new_to_old) {
    result.push_back(values[old_index]);
  }
  values.swap(result);
}

void permuteIndices(std::vector<uint32>& values, const std::vector<uint32>& new_to_old,
                    const std::vector<uint32>& old_to_new) {
  std::vector<uint32> result;
  result.reserve(values.size());
  for (const uint32 old_index : new_to_old) {
    result.push_back(remapIndex(old_to_new, values[old_index]));
  }
  values.swap(result);
}

}  // namespace

SceneGraph::SceneGraph(uint32 reserve_count) : dense_lookup_(reserve_count) {
  parents_.reserve(reserve_count);
  local_positions_.reserve(reserve_count);
  local_rotations_.reserve(reserve_count);
  local_scales_.reserve(reserve_count);
  worlds_.reserve(reserve_count);
  local_versions_.reserve(reserve_count);
  world_versions_.reserve(reserve_count);
  teleport_versions_.reserve(reserve_count);

  const uint32 chunk_count = getChunkCount(reserve_count);
  chunk_local_versions_.reserve(chunk_count);
  chunk_world_versions_.reserve(chunk_count);

  dense_handles_.reserve(reserve_count);
  first_children_.reserve(reserve_count);
  next_siblings_.reserve(reserve_count);
  depths_.reserve(reserve_count);
  mobilities_.reserve(reserve_count);
}

SceneGraph::~SceneGraph() = default;

TransformHandle SceneGraph::create(const TransformDesc& desc) {
  const uint32 parent = desc.parent ? toDense(desc.parent) : kInvalidIndex;
  ASSERT(desc.mobility != ETransformMobility::Static || parent == kInvalidIndex ||
             mobilities_[parent] == ETransformMobility::Static,
         "A static transform must not have a movable parent");

  const uint32 index = getDenseCount();
  ASSERT(index < kInvalidIndex, "Too many transforms");
  const TransformHandle handle = dense_lookup_.create(static_cast<uint32>(index));
  const uint32 version = getPendingVersion();

  uint32 depth = 0;
  if (parent != kInvalidIndex) {
    depth = depths_[parent] + 1u;
    ASSERT(depth <= 0xffffu, "Transform hierarchy is too deep");
  }

  parents_.push_back(parent);
  local_positions_.push_back(desc.position);
  local_rotations_.push_back(desc.rotation.Normalized());
  local_scales_.push_back(desc.scale);
  worlds_.push_back(Mat44::sIdentity());
  local_versions_.push_back(version);
  world_versions_.push_back(0);
  // A new node is a discontinuity, there is no previous transform to interpolate from.
  teleport_versions_.push_back(version);

  dense_handles_.push_back(handle);
  first_children_.push_back(kInvalidIndex);
  next_siblings_.push_back(kInvalidIndex);
  depths_.push_back(static_cast<uint16>(depth));
  mobilities_.push_back(desc.mobility);

  if (parent != kInvalidIndex) {
    linkChild(first_children_, next_siblings_, parent, index);
  }

  if (chunk_local_versions_.size() < getChunkCount(getDenseCount())) {
    chunk_local_versions_.push_back(0);
    chunk_world_versions_.push_back(0);
  }
  markLocalChanged(index);

  return handle;
}

void SceneGraph::destroy(TransformHandle handle, ETransformDestroyMode mode) {
  const uint32 root = toDense(handle);

  // Removes a node without children by moving the last node into its slot.
  auto remove_leaf = [this](uint32 index) {
    ASSERT(first_children_[index] == kInvalidIndex);

    if (parents_[index] != kInvalidIndex) {
      unlinkChild(first_children_, next_siblings_, parents_[index], index);
    }
    destroyed_handles_.push_back(dense_handles_[index]);
    destroyed_versions_.push_back(getPendingVersion());
    dense_lookup_.destroy(dense_handles_[index]);

    const uint32 last = getDenseCount() - 1;
    if (index != last) {
      const uint32 last_parent = parents_[last];
      if (last_parent != kInvalidIndex) {
        replaceChild(first_children_, next_siblings_, last_parent, last, index);
        hierarchy_order_dirty_ |= last_parent > index;
      }
      for (uint32 child = first_children_[last]; child != kInvalidIndex;
           child = next_siblings_[child]) {
        parents_[child] = index;
        hierarchy_order_dirty_ |= child < index;
      }

      parents_[index] = parents_[last];
      local_positions_[index] = local_positions_[last];
      local_rotations_[index] = local_rotations_[last];
      local_scales_[index] = local_scales_[last];
      worlds_[index] = worlds_[last];
      local_versions_[index] = local_versions_[last];
      world_versions_[index] = world_versions_[last];
      teleport_versions_[index] = teleport_versions_[last];
      dense_handles_[index] = dense_handles_[last];
      first_children_[index] = first_children_[last];
      depths_[index] = depths_[last];
      mobilities_[index] = mobilities_[last];
      // next_siblings_[index] was already set by replaceChild() for non-root nodes.
      if (last_parent == kInvalidIndex) {
        next_siblings_[index] = kInvalidIndex;
      }

      *dense_lookup_.get(dense_handles_[index]) = index;

      const uint32 chunk = index / kChunkSize;
      chunk_local_versions_[chunk] =
          std::max(chunk_local_versions_[chunk], local_versions_[index]);
      chunk_world_versions_[chunk] =
          std::max(chunk_world_versions_[chunk], world_versions_[index]);
    }

    parents_.pop_back();
    local_positions_.pop_back();
    local_rotations_.pop_back();
    local_scales_.pop_back();
    worlds_.pop_back();
    local_versions_.pop_back();
    world_versions_.pop_back();
    teleport_versions_.pop_back();
    dense_handles_.pop_back();
    first_children_.pop_back();
    next_siblings_.pop_back();
    depths_.pop_back();
    mobilities_.pop_back();

    const uint32 chunk_count = getChunkCount(getDenseCount());
    chunk_local_versions_.resize(chunk_count);
    chunk_world_versions_.resize(chunk_count);
  };

  if (mode == ETransformDestroyMode::ReparentChildren) {
    const uint32 parent = parents_[root];
    const Mat44 inv_parent_world =
        parent != kInvalidIndex ? computeWorldImmediate(dense_handles_[parent]).Inversed()
                                : Mat44::sIdentity();

    while (first_children_[root] != kInvalidIndex) {
      const uint32 child = first_children_[root];
      const Mat44 world = computeWorldImmediate(dense_handles_[child]);

      relink(parents_, first_children_, next_siblings_, depths_, child, parent);
      hierarchy_order_dirty_ |= parent != kInvalidIndex && parent > child;

      decomposeTRS(inv_parent_world * world, local_positions_[child],
                   local_rotations_[child], local_scales_[child]);
      markLocalChanged(child);
    }

    remove_leaf(root);
    return;
  }

  // Dense indices change while removing, so collect handles first.
  std::vector<TransformHandle> doomed;
  visitSubtreePostOrder(parents_, first_children_, next_siblings_, root,
                        [&](uint32 node) { doomed.push_back(dense_handles_[node]); });

  for (const TransformHandle node : doomed) {
    remove_leaf(toDense(node));
  }
}

bool SceneGraph::isAlive(TransformHandle handle) const {
  if (handle.empty() || handle.index() >= dense_lookup_.getSizeOfObjects()) {
    return false;
  }
  return dense_lookup_.getHandle(handle.index()) == handle;
}

void SceneGraph::setParent(TransformHandle child, TransformHandle parent,
                           EAttachRule rule) {
  const uint32 child_index = toDense(child);
  const uint32 parent_index = parent ? toDense(parent) : kInvalidIndex;
  ASSERT(mobilities_[child_index] == ETransformMobility::Movable,
         "Cannot change the parent of a static transform");

  if (parents_[child_index] == parent_index) {
    return;
  }

  for (uint32 ancestor = parent_index; ancestor != kInvalidIndex;
       ancestor = parents_[ancestor]) {
    ASSERT(ancestor != child_index, "Cannot attach a transform to its own descendant");
  }

  Mat44 world;
  if (rule == EAttachRule::KeepWorld) {
    world = computeWorldImmediate(child);
  }

  relink(parents_, first_children_, next_siblings_, depths_, child_index, parent_index);
  hierarchy_order_dirty_ |= parent_index != kInvalidIndex && parent_index > child_index;

  if (rule == EAttachRule::KeepWorld) {
    const Mat44 local = parent_index != kInvalidIndex
                            ? computeWorldImmediate(parent).Inversed() * world
                            : world;
    decomposeTRS(local, local_positions_[child_index], local_rotations_[child_index],
                 local_scales_[child_index]);
  }

  markLocalChanged(child_index);
}

TransformHandle SceneGraph::getParent(TransformHandle handle) const {
  const uint32 parent = parents_[toDense(handle)];
  return parent != kInvalidIndex ? dense_handles_[parent] : TransformHandle{};
}

TransformHandle SceneGraph::getFirstChild(TransformHandle handle) const {
  const uint32 child = first_children_[toDense(handle)];
  return child != kInvalidIndex ? dense_handles_[child] : TransformHandle{};
}

TransformHandle SceneGraph::getNextSibling(TransformHandle handle) const {
  const uint32 sibling = next_siblings_[toDense(handle)];
  return sibling != kInvalidIndex ? dense_handles_[sibling] : TransformHandle{};
}

uint32 SceneGraph::getDepth(TransformHandle handle) const {
  return depths_[toDense(handle)];
}

ETransformMobility SceneGraph::getMobility(TransformHandle handle) const {
  return mobilities_[toDense(handle)];
}

void SceneGraph::setLocalPosition(TransformHandle handle, Vec3Arg position) {
  const uint32 index = toDense(handle);
  ASSERT(mobilities_[index] == ETransformMobility::Movable,
         "Cannot move a static transform");
  local_positions_[index] = position;
  markLocalChanged(index);
}

void SceneGraph::setLocalRotation(TransformHandle handle, QuatArg rotation) {
  const uint32 index = toDense(handle);
  ASSERT(mobilities_[index] == ETransformMobility::Movable,
         "Cannot move a static transform");
  local_rotations_[index] = rotation.Normalized();
  markLocalChanged(index);
}

void SceneGraph::setLocalScale(TransformHandle handle, Vec3Arg scale) {
  const uint32 index = toDense(handle);
  ASSERT(mobilities_[index] == ETransformMobility::Movable,
         "Cannot move a static transform");
  local_scales_[index] = scale;
  markLocalChanged(index);
}

void SceneGraph::setLocalTRS(TransformHandle handle, Vec3Arg position, QuatArg rotation,
                             Vec3Arg scale) {
  const uint32 index = toDense(handle);
  ASSERT(mobilities_[index] == ETransformMobility::Movable,
         "Cannot move a static transform");
  local_positions_[index] = position;
  local_rotations_[index] = rotation.Normalized();
  local_scales_[index] = scale;
  markLocalChanged(index);
}

Vec3 SceneGraph::getLocalPosition(TransformHandle handle) const {
  return local_positions_[toDense(handle)];
}

Quat SceneGraph::getLocalRotation(TransformHandle handle) const {
  return local_rotations_[toDense(handle)];
}

Vec3 SceneGraph::getLocalScale(TransformHandle handle) const {
  return local_scales_[toDense(handle)];
}

void SceneGraph::setWorldTRS(TransformHandle handle, Vec3Arg position, QuatArg rotation,
                             Vec3Arg scale) {
  const uint32 index = toDense(handle);
  ASSERT(mobilities_[index] == ETransformMobility::Movable,
         "Cannot move a static transform");

  const uint32 parent = parents_[index];
  if (parent == kInvalidIndex) {
    local_positions_[index] = position;
    local_rotations_[index] = rotation.Normalized();
    local_scales_[index] = scale;
  } else {
    const Mat44 world = composeTRS(position, rotation.Normalized(), scale);
    const Mat44 local = computeWorldImmediate(dense_handles_[parent]).Inversed() * world;
    decomposeTRS(local, local_positions_[index], local_rotations_[index],
                 local_scales_[index]);
  }

  markLocalChanged(index);
}

void SceneGraph::teleport(TransformHandle handle) {
  const uint32 index = toDense(handle);
  ASSERT(mobilities_[index] == ETransformMobility::Movable,
         "Cannot teleport a static transform");
  teleport_versions_[index] = getPendingVersion();
  markLocalChanged(index);
}

const Mat44& SceneGraph::getWorld(TransformHandle handle) const {
#ifndef NDEBUG
  ASSERT(!isWorldStale(handle), "World transform has pending writes, call update()");
#endif
  return worlds_[toDense(handle)];
}

bool SceneGraph::isWorldStale(TransformHandle handle) const {
  for (uint32 node = toDense(handle); node != kInvalidIndex; node = parents_[node]) {
    if (local_versions_[node] > world_version_) {
      return true;
    }
  }
  return false;
}

Mat44 SceneGraph::computeWorldImmediate(TransformHandle handle) const {
  uint32 node = toDense(handle);
  Mat44 world =
      composeTRS(local_positions_[node], local_rotations_[node], local_scales_[node]);

  for (node = parents_[node]; node != kInvalidIndex; node = parents_[node]) {
    world =
        composeTRS(local_positions_[node], local_rotations_[node], local_scales_[node]) *
        world;
  }
  return world;
}

uint32 SceneGraph::update() {
  if (hierarchy_order_dirty_) {
    restoreHierarchyOrder();
  }

  const uint32 version = getPendingVersion();
  const uint32 count = getDenseCount();
  const uint32 chunk_count = static_cast<uint32>(chunk_local_versions_.size());

  // Parents come before children, so chunks before the first recomputed node cannot have
  // a stale ancestor and only need their own local version checked.
  bool propagating = false;
  for (uint32 chunk = 0; chunk < chunk_count; ++chunk) {
    if (!propagating && chunk_local_versions_[chunk] != version) {
      continue;
    }

    bool chunk_changed = false;
    const uint32 end = std::min(count, (chunk + 1) * kChunkSize);
    for (uint32 i = chunk * kChunkSize; i < end; ++i) {
      const uint32 parent = parents_[i];
      const bool parent_changed =
          parent != kInvalidIndex && world_versions_[parent] == version;
      if (!parent_changed && local_versions_[i] != version) {
        continue;
      }

      const Mat44 local =
          composeTRS(local_positions_[i], local_rotations_[i], local_scales_[i]);
      worlds_[i] = parent != kInvalidIndex ? worlds_[parent] * local : local;
      world_versions_[i] = version;
      if (parent_changed && teleport_versions_[parent] == version) {
        teleport_versions_[i] = version;
      }
      chunk_changed = true;
    }

    if (chunk_changed) {
      chunk_world_versions_[chunk] = version;
      propagating = true;
    }
  }

  world_version_ = version;
  trimDestroyLog();
  return world_version_;
}

uint32 SceneGraph::toDense(TransformHandle handle) const {
  ASSERT(isAlive(handle), "Invalid or destroyed transform handle");
  return *dense_lookup_.get(handle);
}

void SceneGraph::markLocalChanged(uint32 dense_index) {
  const uint32 version = getPendingVersion();
  local_versions_[dense_index] = version;
  chunk_local_versions_[dense_index / kChunkSize] = version;
}

void SceneGraph::restoreHierarchyOrder() {
  const uint32 count = getDenseCount();

  // Stable topological order: nodes keep their relative order, except that a node whose
  // parent comes later is deferred until right after its parent is emitted.
  std::vector<uint32> new_to_old;
  new_to_old.reserve(count);
  std::vector<bool> emitted(count, false);
  std::vector<uint32> stack;

  for (uint32 i = 0; i < count; ++i) {
    if (emitted[i]) {
      continue;
    }
    const uint32 parent = parents_[i];
    if (parent != kInvalidIndex && !emitted[parent]) {
      continue;
    }

    stack.push_back(i);
    while (!stack.empty()) {
      const uint32 node = stack.back();
      stack.pop_back();
      emitted[node] = true;
      new_to_old.push_back(node);

      for (uint32 child = first_children_[node]; child != kInvalidIndex;
           child = next_siblings_[child]) {
        if (child < i && !emitted[child]) {
          stack.push_back(child);
        }
      }
    }
  }
  ASSERT(new_to_old.size() == count, "Transform hierarchy contains a cycle");

  std::vector<uint32> old_to_new(count);
  for (uint32 i = 0; i < count; ++i) {
    old_to_new[new_to_old[i]] = i;
  }

  permuteIndices(parents_, new_to_old, old_to_new);
  permuteIndices(first_children_, new_to_old, old_to_new);
  permuteIndices(next_siblings_, new_to_old, old_to_new);
  permute(local_positions_, new_to_old);
  permute(local_rotations_, new_to_old);
  permute(local_scales_, new_to_old);
  permute(worlds_, new_to_old);
  permute(local_versions_, new_to_old);
  permute(world_versions_, new_to_old);
  permute(teleport_versions_, new_to_old);
  permute(dense_handles_, new_to_old);
  permute(depths_, new_to_old);
  permute(mobilities_, new_to_old);

  for (uint32 i = 0; i < count; ++i) {
    *dense_lookup_.get(dense_handles_[i]) = i;
  }

  const uint32 chunk_count = static_cast<uint32>(chunk_local_versions_.size());
  for (uint32 chunk = 0; chunk < chunk_count; ++chunk) {
    uint32 local_version = 0;
    uint32 world_version = 0;
    const uint32 end = std::min(count, (chunk + 1) * kChunkSize);
    for (uint32 i = chunk * kChunkSize; i < end; ++i) {
      local_version = std::max(local_version, local_versions_[i]);
      world_version = std::max(world_version, world_versions_[i]);
    }
    chunk_local_versions_[chunk] = local_version;
    chunk_world_versions_[chunk] = world_version;
  }

  hierarchy_order_dirty_ = false;
}

void SceneGraph::trimDestroyLog() {
  // Drops entries in batches, once the oldest one is twice the retention old, so the log
  // is shifted at most once every kDestroyLogRetention updates.
  if (destroyed_versions_.empty() ||
      world_version_ - destroyed_versions_.front() < 2 * kDestroyLogRetention) {
    return;
  }

  const auto end =
      std::upper_bound(destroyed_versions_.begin(), destroyed_versions_.end(),
                       world_version_ - kDestroyLogRetention);
  const auto count = end - destroyed_versions_.begin();
  destroy_log_trimmed_version_ = *(end - 1);
  destroyed_versions_.erase(destroyed_versions_.begin(), end);
  destroyed_handles_.erase(destroyed_handles_.begin(),
                           destroyed_handles_.begin() + count);
}

}  // namespace amaryl
