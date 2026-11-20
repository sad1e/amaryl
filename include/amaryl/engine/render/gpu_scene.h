#pragma once

#include <cstring>
#include <type_traits>
#include <vector>

#include "amaryl/core/span.h"
#include "amaryl/engine/common.h"
#include "amaryl/engine/render/gpu_scene.shared.h"

namespace amaryl {

// GPU scene buffers written through scatter copies. worldCurr / worldPrev are written by
// the transform scatter pass from GpuSceneUploads::transforms instead.
enum class EGpuSceneBuffer : uint8 {
  Instances,
  MaterialSlots,
  Materials,
  Meshes,
  MeshSections,
  Meshlets,
  MeshletVertices,
  // Counted in dwords, 4 triangle bytes each.
  MeshletTriangles,
  Indices,
  // Counted in vertices, 3 floats each.
  VertexPositions,
  // Counted in vertices, GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS each.
  VertexAttributes,
  Count,
};

inline constexpr uint32 kGpuSceneBufferCount =
    static_cast<uint32>(EGpuSceneBuffer::Count);

// Dword scatter copies into one GPU buffer: `payload` goes to a staging buffer, every
// copy moves `dwordCount` dwords from `payload[srcOffset]` to `buffer[dstOffset]`.
// Contiguous writes are merged. Copies of a batch never overlap and may run in any order.
struct GpuScatterQueue {
  std::vector<uint32> payload;
  std::vector<GpuScatterCopy> copies;

  bool empty() const { return copies.empty(); }

  void clear() {
    payload.clear();
    copies.clear();
  }

  void write(uint32 dst_dword_offset, const void* data, uint32 dword_count) {
    if (dword_count == 0) {
      return;
    }

    const uint32 src_offset = static_cast<uint32>(payload.size());
    payload.resize(payload.size() + dword_count);
    std::memcpy(payload.data() + src_offset, data, dword_count * sizeof(uint32));
    if (!copies.empty()) {
      GpuScatterCopy& last = copies.back();
      if (last.dstOffset + last.dwordCount == dst_dword_offset &&
          last.srcOffset + last.dwordCount == src_offset) {
        last.dwordCount += dword_count;
        return;
      }
    }
    copies.push_back({dst_dword_offset, src_offset, dword_count, 0});
  }

  // Writes `values` to elements [first_element, first_element + values.size()).
  template <typename T>
  void writeElements(uint32 first_element, Span<const T> values) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) % sizeof(uint32) == 0);
    constexpr uint32 kDwords = sizeof(T) / sizeof(uint32);
    write(first_element * kDwords, values.data(),
          static_cast<uint32>(values.size()) * kDwords);
  }

  template <typename T>
  void writeElement(uint32 element, const T& value) {
    writeElements(element, Span<const T>(&value, 1));
  }
};

// Everything the renderer has to upload for one frame, filled by the `flushUploads()` of
// MeshRegistry, MaterialRegistry and RenderScene. The renderer grows every buffer to
// `capacities` (elements) first, then runs the copies and the transform updates.
struct GpuSceneUploads {
  GpuScatterQueue buffers[kGpuSceneBufferCount];
  std::vector<GpuTransformUpdate> transforms;
  uint32 capacities[kGpuSceneBufferCount]{};

  GpuScatterQueue& operator[](EGpuSceneBuffer buffer) {
    return buffers[static_cast<uint32>(buffer)];
  }
  const GpuScatterQueue& operator[](EGpuSceneBuffer buffer) const {
    return buffers[static_cast<uint32>(buffer)];
  }

  uint32& capacity(EGpuSceneBuffer buffer) {
    return capacities[static_cast<uint32>(buffer)];
  }
  uint32 capacity(EGpuSceneBuffer buffer) const {
    return capacities[static_cast<uint32>(buffer)];
  }

  // Capacities are kept, every `flushUploads()` overwrites its own.
  void clear() {
    for (GpuScatterQueue& queue : buffers) {
      queue.clear();
    }
    transforms.clear();
  }
};

}  // namespace amaryl
