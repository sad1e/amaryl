#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"
#include "amaryl/engine/render/range_allocator.h"
#include "amaryl/engine/render/render_scene.h"
#include "amaryl/engine/scene/scene_graph.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

constexpr uint32 kUnwritten = 0xcdcdcdcdu;

// Applies the uploads of a frame like the renderer: grows the buffers, runs the scatter
// copies, then the transform updates.
struct FakeGpu {
  std::vector<uint32> buffers[kGpuSceneBufferCount];
  std::vector<GpuTransform> worldCurr;
  std::vector<GpuTransform> worldPrev;

  void apply(const GpuSceneUploads& uploads) {
    for (uint32 b = 0; b < kGpuSceneBufferCount; ++b) {
      const auto id = static_cast<EGpuSceneBuffer>(b);
      std::vector<uint32>& buffer = buffers[b];
      const size_t size = size_t{uploads.capacity(id)} * getGpuSceneElementDwords(id);
      if (buffer.size() < size) {
        buffer.resize(size, kUnwritten);
      }

      const GpuScatterQueue& queue = uploads[id];
      std::vector<std::pair<uint32, uint32>> ranges;
      for (const GpuScatterCopy& copy : queue.copies) {
        const bool valid =
          copy.dwordCount != 0 &&
          size_t{copy.srcOffset} + copy.dwordCount <= queue.payload.size() &&
          size_t{copy.dstOffset} + copy.dwordCount <= size;
        expect(valid) << "invalid scatter copy";
        if (valid) {
          std::memcpy(buffer.data() + copy.dstOffset,
            queue.payload.data() + copy.srcOffset, copy.dwordCount * sizeof(uint32));
          ranges.emplace_back(copy.dstOffset, copy.dstOffset + copy.dwordCount);
        }
      }
      // The copies of a batch run in any order.
      std::sort(ranges.begin(), ranges.end());
      for (size_t i = 1; i < ranges.size(); ++i) {
        expect(ranges[i - 1].second <= ranges[i].first) << "overlapping scatter copies";
      }
    }

    const uint32 instance_capacity = uploads.capacity(EGpuSceneBuffer::Instances);
    if (worldCurr.size() < instance_capacity) {
      worldCurr.resize(instance_capacity);
      worldPrev.resize(instance_capacity);
    }
    std::vector<bool> updated(instance_capacity, false);
    for (const GpuTransformUpdate& update : uploads.transforms) {
      const uint32 id = update.instanceId;
      const bool settle = (update.flags & GPU_TRANSFORM_UPDATE_SETTLE) != 0;
      const bool teleport = (update.flags & GPU_TRANSFORM_UPDATE_TELEPORT) != 0;
      const bool valid = id < instance_capacity && !updated[id] && !(settle && teleport);
      expect(valid) << "invalid transform update";
      if (!valid) {
        continue;
      }
      updated[id] = true;
      if (settle) {
        worldPrev[id] = worldCurr[id];
      } else {
        worldPrev[id] = teleport ? update.world : worldCurr[id];
        worldCurr[id] = update.world;
      }
    }
  }

  // Reads element `index` of a buffer seen as an array of T.
  template <typename T>
  T read(EGpuSceneBuffer buffer, uint32 index) const {
    static_assert(sizeof(T) % sizeof(uint32) == 0);
    constexpr size_t kDwords = sizeof(T) / sizeof(uint32);
    const std::vector<uint32>& data = buffers[static_cast<uint32>(buffer)];
    T value{};
    const bool valid = (size_t{index} + 1) * kDwords <= data.size();
    expect(valid) << "out of bounds read";
    if (valid) {
      std::memcpy(&value, data.data() + size_t{index} * kDwords, sizeof(T));
    }
    return value;
  }

  uint8 readMeshletTriangleByte(uint32 byte_offset) const {
    const uint32 dword = read<uint32>(EGpuSceneBuffer::MeshletTriangles, byte_offset / 4);
    return static_cast<uint8>(dword >> (byte_offset % 4 * 8));
  }
};

bool isClose(const GpuTransform& transform, const Mat44& world) {
  for (uint32 row = 0; row < 3; ++row) {
    for (uint32 column = 0; column < 4; ++column) {
      if (std::abs(transform.rows[row][column] - world(row, column)) > 1e-5f) {
        return false;
      }
    }
  }
  return true;
}

// `cells` x `cells` quads in the XY plane, the rows are split evenly between `sections`.
std::shared_ptr<const MeshAsset> makeGrid(uint32 cells, uint32 sections) {
  std::vector<float3> positions;
  std::vector<float2> uvs;
  for (uint32 y = 0; y <= cells; ++y) {
    for (uint32 x = 0; x <= cells; ++x) {
      positions.push_back(float3{float(x), float(y), 0.0f});
      uvs.push_back(float2{float(x) / float(cells), float(y) / float(cells)});
    }
  }
  std::vector<std::vector<uint32>> indices(sections);
  for (uint32 y = 0; y < cells; ++y) {
    for (uint32 x = 0; x < cells; ++x) {
      const uint32 v0 = y * (cells + 1) + x;
      const uint32 v3 = v0 + cells + 1;
      std::vector<uint32>& out = indices[y * sections / cells];
      out.insert(out.end(), {v0, v0 + 1, v3 + 1, v0, v3 + 1, v3});
    }
  }

  std::vector<MeshSourceSection> source;
  for (const std::vector<uint32>& section : indices) {
    source.push_back({positions, {}, {}, uvs, section});
  }
  auto asset = std::make_shared<MeshAsset>();
  asset->name = "grid";
  expect(buildMeshAsset(source, {}, *asset));
  return asset;
}

// Reads the uploaded mesh back through its rebased offsets and compares it to `asset`.
void expectMesh(const FakeGpu& gpu, MeshHandle handle, const MeshAsset& asset) {
  const GpuMesh mesh =
    gpu.read<GpuMesh>(EGpuSceneBuffer::Meshes, MeshRegistry::getGpuIndex(handle));
  expect(mesh.lodCount == asset.lods.size() && mesh.sectionCount == asset.sectionCount);
  expect(mesh.boundingSphere == asset.boundingSphere);

  bool vertices_match = true;
  for (uint32 v = 0; v < asset.getVertexCount(); ++v) {
    vertices_match &= gpu.read<float3>(EGpuSceneBuffer::VertexPositions,
                        mesh.baseVertex + v) == asset.positions[v];
    for (uint32 k = 0; k < GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS; ++k) {
      const uint32 dword = (mesh.baseVertex + v) * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS + k;
      vertices_match &= gpu.read<uint32>(EGpuSceneBuffer::VertexAttributes, dword) ==
                        asset.attributes[v * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS + k];
    }
  }
  expect(vertices_match);

  for (uint32 lod = 0; lod < asset.lods.size() && lod < mesh.lodCount; ++lod) {
    const GpuMeshLod& gpu_lod = mesh.lods[lod];
    const GpuMeshLod& source_lod = asset.lods[lod];
    expect(gpu_lod.meshletCount == source_lod.meshletCount);

    // Indices stay relative to baseVertex.
    bool sections_match = true;
    for (uint32 s = 0; s < asset.sectionCount; ++s) {
      const GpuMeshSection section = gpu.read<GpuMeshSection>(
        EGpuSceneBuffer::MeshSections, gpu_lod.sectionOffset + s);
      const GpuMeshSection& source = asset.sections[source_lod.sectionOffset + s];
      sections_match &= section.indexCount == source.indexCount &&
                        section.materialSlot == source.materialSlot;
      for (uint32 i = 0; i < source.indexCount; ++i) {
        sections_match &=
          gpu.read<uint32>(EGpuSceneBuffer::Indices, section.firstIndex + i) ==
          asset.indices[source.firstIndex + i];
      }
    }
    expect(sections_match);

    bool meshlets_match = true;
    for (uint32 m = 0; m < source_lod.meshletCount; ++m) {
      const GpuMeshlet meshlet =
        gpu.read<GpuMeshlet>(EGpuSceneBuffer::Meshlets, gpu_lod.meshletOffset + m);
      const GpuMeshlet& source = asset.meshlets[source_lod.meshletOffset + m];
      meshlets_match &= meshlet.packedCounts == source.packedCounts &&
                        meshlet.packedCone == source.packedCone &&
                        meshlet.boundingSphere == source.boundingSphere;
      for (uint32 v = 0; v < gpuMeshletVertexCount(source.packedCounts); ++v) {
        meshlets_match &=
          gpu.read<uint32>(EGpuSceneBuffer::MeshletVertices, meshlet.vertexOffset + v) ==
          asset.meshletVertices[source.vertexOffset + v];
      }
      for (uint32 b = 0; b < gpuMeshletTriangleCount(source.packedCounts) * 3; ++b) {
        meshlets_match &= gpu.readMeshletTriangleByte(meshlet.triangleOffset + b) ==
                          asset.meshletTriangles[source.triangleOffset + b];
      }
    }
    expect(meshlets_match);
  }
}

struct SceneFixture {
  SceneGraph graph;
  MaterialRegistry materials;
  MeshRegistry meshes{materials};
  RenderScene scene{graph, meshes, materials};
  FakeGpu gpu;
  GpuSceneUploads uploads;

  void frame() {
    graph.update();
    scene.sync();
    uploads.clear();
    meshes.flushUploads(uploads);
    materials.flushUploads(uploads);
    scene.flushUploads(uploads);
    gpu.apply(uploads);
  }

  GpuInstance getInstance(RenderProxyHandle proxy) const {
    return gpu.read<GpuInstance>(
      EGpuSceneBuffer::Instances, RenderScene::getInstanceId(proxy));
  }

  uint32 getMaterialSlot(RenderProxyHandle proxy, uint32 section) const {
    return gpu.read<uint32>(
      EGpuSceneBuffer::MaterialSlots, getInstance(proxy).materialSlotOffset + section);
  }

  // The transform update of `proxy` in the last frame, null without update.
  const GpuTransformUpdate* findUpdate(RenderProxyHandle proxy) const {
    for (const GpuTransformUpdate& update : uploads.transforms) {
      if (update.instanceId == RenderScene::getInstanceId(proxy)) {
        return &update;
      }
    }
    return nullptr;
  }
};

}  // namespace

int main() {
  "range allocator uses best fit and coalesces"_test = [] {
    RangeAllocator ranges;
    expect(ranges.allocate(10) == 0u);
    expect(ranges.allocate(20) == 10u);
    expect(ranges.allocate(5) == 30u);
    expect(ranges.allocate(30) == 35u);
    expect(ranges.allocate(8) == 65u);
    expect(ranges.getEnd() == 73u && ranges.getAllocatedSize() == 73u);

    // Holes of 30 and 20 elements, the smallest one that fits is used.
    ranges.free(35, 30);
    ranges.free(10, 20);
    expect(ranges.allocate(18) == 10u);
    expect(ranges.allocate(25) == 35u);
    expect(ranges.allocate(6) == 73u);
    expect(ranges.allocate(5) == 60u);
    expect(ranges.getEnd() == 79u && ranges.getAllocatedSize() == 77u);

    // Neighbor holes are merged: [0, 35).
    ranges.free(30, 5);
    ranges.free(0, 10);
    ranges.free(10, 18);
    expect(ranges.allocate(35) == 0u);
    ranges.free(0, 35);

    // Freeing the last range shrinks the space, with the hole before it.
    ranges.free(73, 6);
    expect(ranges.getEnd() == 73u);
    ranges.free(65, 8);
    ranges.free(60, 5);
    expect(ranges.getEnd() == 60u);
    ranges.free(35, 25);
    expect(ranges.getEnd() == 0u && ranges.getAllocatedSize() == 0u);
    expect(ranges.allocate(4) == 0u);

    ranges.reset();
    expect(ranges.getEnd() == 0u && ranges.getAllocatedSize() == 0u);
    expect(ranges.allocate(7) == 0u);
  };

  "material registry uploads created and updated materials"_test = [] {
    MaterialRegistry materials;
    FakeGpu gpu;
    GpuSceneUploads uploads;
    // Returns the number of uploaded materials.
    const auto flush = [&] {
      uploads.clear();
      materials.flushUploads(uploads);
      gpu.apply(uploads);
      return uploads[EGpuSceneBuffer::Materials].payload.size() /
             (sizeof(GpuMaterial) / sizeof(uint32));
    };
    const auto expectUploaded = [&](MaterialHandle handle) {
      const GpuMaterial expected = toGpuMaterial(materials.get(handle));
      const GpuMaterial actual = gpu.read<GpuMaterial>(
        EGpuSceneBuffer::Materials, MaterialRegistry::getGpuIndex(handle));
      expect(std::memcmp(&expected, &actual, sizeof(GpuMaterial)) == 0);
    };

    const MaterialHandle fallback = materials.getDefault();
    expect(materials.isAlive(fallback) && materials.getCount() == 1u);
    expect(materials.get(fallback).name == "default");

    MaterialAsset red;
    red.name = "red";
    red.baseColorFactor = float4{1.0f, 0.0f, 0.0f, 1.0f};
    red.alphaMode = EAlphaMode::Mask;
    red.baseColorTexture = TextureDescriptorIndex{7};
    const MaterialHandle a = materials.create(red);
    expect(flush() == 2u && uploads.capacity(EGpuSceneBuffer::Materials) == 2u);
    expectUploaded(fallback);
    expectUploaded(a);
    expect(flush() == 0u);

    // Several updates between two flushes upload the material once.
    red.roughnessFactor = 0.5f;
    materials.update(a, red);
    red.doubleSided = true;
    materials.update(a, red);
    expect(flush() == 1u);
    expectUploaded(a);
    expect(gpu.read<GpuMaterial>(EGpuSceneBuffer::Materials, a.index()).pipelineBin ==
           getMaterialPipelineBin(EShadingModel::Lit, EAlphaMode::Mask, true));

    // Clones are independent copies.
    const MaterialHandle b = materials.clone(a);
    expect(b != a && materials.get(b).name == "red");
    MaterialAsset blue = materials.get(b);
    blue.baseColorFactor = float4{0.0f, 0.0f, 1.0f, 1.0f};
    materials.update(b, blue);
    expect(flush() == 1u);
    expectUploaded(a);
    expectUploaded(b);

    // Materials destroyed before the flush are skipped, reused slots are uploaded.
    const MaterialHandle c = materials.create(red);
    materials.destroy(c);
    expect(!materials.isAlive(c) && flush() == 0u);
    materials.destroy(b);
    const MaterialHandle d = materials.create(blue);
    expect(d.index() == b.index() && !materials.isAlive(b));
    expect(flush() == 1u);
    expectUploaded(d);

    materials.addRef(a);
    materials.addRef(a);
    expect(materials.getRefCount(a) == 2u);
    materials.release(a);
    materials.release(a);
    materials.destroy(a);
    materials.destroy(d);
    expect(materials.getCount() == 1u);
  };

  "mesh registry uploads and rebases the geometry"_test = [] {
    MaterialRegistry materials;
    MeshRegistry meshes(materials);
    FakeGpu gpu;
    GpuSceneUploads uploads;
    const auto flush = [&] {
      uploads.clear();
      meshes.flushUploads(uploads);
      materials.flushUploads(uploads);
      gpu.apply(uploads);
    };

    const auto big = makeGrid(12, 2);
    const auto small = makeGrid(3, 1);
    expect(big->meshlets.size() > 1u);

    const MaterialHandle red = materials.create({});
    const MaterialHandle fallback = materials.getDefault();
    const MaterialHandle defaults[] = {red};
    const MeshHandle a = meshes.create(big, defaults);
    const MeshHandle b = meshes.create(small);
    expect(meshes.isAlive(a) && meshes.isAlive(b) && meshes.getCount() == 2u);
    expect(meshes.getSectionCount(a) == 2u && &meshes.getAsset(a) == big.get());
    expect(meshes.getDefaultMaterial(a, 0) == red &&
           meshes.getDefaultMaterial(a, 1) == fallback);
    expect(materials.getRefCount(red) == 1u && materials.getRefCount(fallback) == 2u);

    flush();
    expectMesh(gpu, a, *big);
    expectMesh(gpu, b, *small);
    expect(uploads.capacity(EGpuSceneBuffer::Meshes) == 2u);
    expect(uploads.capacity(EGpuSceneBuffer::VertexPositions) ==
           big->getVertexCount() + small->getVertexCount());

    // Geometry is uploaded once.
    flush();
    expect(uploads[EGpuSceneBuffer::Meshes].empty() &&
           uploads[EGpuSceneBuffer::VertexPositions].empty());

    // The ranges of destroyed meshes are reused, the buffers do not grow.
    const uint32 vertex_capacity = uploads.capacity(EGpuSceneBuffer::VertexPositions);
    meshes.destroy(a);
    expect(!meshes.isAlive(a) && materials.getRefCount(red) == 0u);
    const MeshHandle c = meshes.create(small);
    expect(c.index() == a.index());
    flush();
    expect(gpu.read<GpuMesh>(EGpuSceneBuffer::Meshes, c.index()).baseVertex == 0u);
    expect(uploads.capacity(EGpuSceneBuffer::VertexPositions) == vertex_capacity);
    expectMesh(gpu, b, *small);
    expectMesh(gpu, c, *small);

    // Malformed assets are rejected.
    expect(!meshes.create(nullptr));
    expect(!meshes.create(std::make_shared<MeshAsset>()));
    auto broken = std::make_shared<MeshAsset>(*small);
    broken->indices[0] = broken->getVertexCount();
    expect(!meshes.create(broken));
    expect(meshes.getCount() == 2u);

    meshes.addRef(b);
    expect(meshes.getRefCount(b) == 1u);
    meshes.release(b);
    meshes.destroy(b);
    meshes.destroy(c);
    expect(meshes.getCount() == 0u && materials.getRefCount(fallback) == 0u);
    materials.destroy(red);
  };

  "render proxies upload instances, material slots and transforms"_test = [] {
    SceneFixture f;
    const auto grid = makeGrid(4, 2);
    const MaterialHandle red = f.materials.create({});
    const MaterialHandle blue = f.materials.create({});
    const MaterialHandle fallback = f.materials.getDefault();
    const MaterialHandle defaults[] = {red};
    const MeshHandle mesh = f.meshes.create(grid, defaults);

    const TransformHandle moving = f.graph.create({.position = Vec3(1, 2, 3)});
    const TransformHandle fixed =
      f.graph.create({.position = Vec3(5, 0, 0), .mobility = ETransformMobility::Static});
    const MaterialHandle overrides[] = {MaterialHandle{}, blue};
    const RenderProxyHandle a =
      f.scene.create({.transform = moving, .mesh = mesh, .materials = overrides});
    const RenderProxyHandle s =
      f.scene.create({.transform = fixed, .mesh = mesh, .castShadow = false});
    expect(f.scene.isAlive(a) && f.scene.isAlive(s) && f.scene.getCount() == 2u);
    expect(f.scene.getTransform(a) == moving && f.scene.getMesh(a) == mesh);
    expect(f.scene.getMaterial(a, 0) == red && f.scene.getMaterial(a, 1) == blue);
    expect(f.scene.getMaterial(s, 0) == red && f.scene.getMaterial(s, 1) == fallback);
    expect(f.meshes.getRefCount(mesh) == 2u);
    expect(f.materials.getRefCount(red) == 3u && f.materials.getRefCount(blue) == 1u);

    f.frame();
    GpuInstance instance = f.getInstance(a);
    expect(instance.meshIndex == MeshRegistry::getGpuIndex(mesh));
    expect(instance.flags == (GPU_INSTANCE_FLAG_VALID | GPU_INSTANCE_FLAG_CAST_SHADOW));
    expect(instance.skinningOffset == GPU_SCENE_INVALID_INDEX);
    expect(instance.localBoundingSphere == grid->boundingSphere);
    expect(f.getMaterialSlot(a, 0) == MaterialRegistry::getGpuIndex(red));
    expect(f.getMaterialSlot(a, 1) == MaterialRegistry::getGpuIndex(blue));
    expect(
      f.getInstance(s).flags == (GPU_INSTANCE_FLAG_VALID | GPU_INSTANCE_FLAG_STATIC));
    expect(f.getMaterialSlot(s, 0) == MaterialRegistry::getGpuIndex(red));
    expect(f.getMaterialSlot(s, 1) == MaterialRegistry::getGpuIndex(fallback));

    // New instances teleport to their transform.
    expect(f.uploads.transforms.size() == 2u);
    for (const RenderProxyHandle proxy : {a, s}) {
      const GpuTransformUpdate* update = f.findUpdate(proxy);
      expect(update && update->flags == GPU_TRANSFORM_UPDATE_TELEPORT);
      const uint32 id = RenderScene::getInstanceId(proxy);
      const Mat44 world = f.graph.getWorld(f.scene.getTransform(proxy));
      expect(isClose(f.gpu.worldCurr[id], world) && isClose(f.gpu.worldPrev[id], world));
    }

    // Moved instances keep their previous transform for one frame, then settle.
    const uint32 id = RenderScene::getInstanceId(a);
    const Mat44 start = f.graph.getWorld(moving);
    f.graph.setLocalPosition(moving, Vec3(4, 5, 6));
    f.frame();
    const GpuTransformUpdate* update = f.findUpdate(a);
    expect(f.uploads.transforms.size() == 1u && update && update->flags == 0u);
    expect(isClose(f.gpu.worldCurr[id], f.graph.getWorld(moving)));
    expect(isClose(f.gpu.worldPrev[id], start));
    expect(f.uploads[EGpuSceneBuffer::Instances].empty());

    f.frame();
    update = f.findUpdate(a);
    expect(f.uploads.transforms.size() == 1u && update &&
           update->flags == GPU_TRANSFORM_UPDATE_SETTLE);
    expect(isClose(f.gpu.worldPrev[id], f.graph.getWorld(moving)));
    f.frame();
    expect(f.uploads.transforms.empty());

    // Teleported instances do not settle.
    f.graph.setLocalPosition(moving, Vec3(-1, 0, 0));
    f.graph.teleport(moving);
    f.frame();
    update = f.findUpdate(a);
    expect(update && update->flags == GPU_TRANSFORM_UPDATE_TELEPORT);
    expect(isClose(f.gpu.worldPrev[id], f.graph.getWorld(moving)));
    f.frame();
    expect(f.uploads.transforms.empty());

    // Changes rewrite the instance, an empty material restores the mesh's default.
    f.scene.setMaterial(a, 1, {});
    f.scene.setCastShadow(a, false);
    expect(f.scene.getMaterial(a, 1) == fallback && f.materials.getRefCount(blue) == 0u);
    f.frame();
    expect(f.getInstance(a).flags == GPU_INSTANCE_FLAG_VALID);
    expect(f.getMaterialSlot(a, 1) == MaterialRegistry::getGpuIndex(fallback));

    // Proxies are destroyed with their transform at the next sync.
    f.graph.destroy(moving);
    expect(f.scene.isAlive(a));
    f.frame();
    expect(!f.scene.isAlive(a) && f.scene.getCount() == 1u);
    instance = f.getInstance(a);
    expect(instance.flags == 0u && instance.meshIndex == GPU_SCENE_INVALID_INDEX);
    expect(f.meshes.getRefCount(mesh) == 1u && f.materials.getRefCount(red) == 2u);
    f.scene.destroy(a);

    // Slots are reused.
    const TransformHandle other = f.graph.create({.position = Vec3(0, 0, 7)});
    const RenderProxyHandle b = f.scene.create({.transform = other, .mesh = mesh});
    expect(b.index() == a.index() && b != a);
    f.frame();
    expect(f.getInstance(b).flags ==
           (GPU_INSTANCE_FLAG_VALID | GPU_INSTANCE_FLAG_CAST_SHADOW));
    expect(isClose(f.gpu.worldCurr[b.index()], f.graph.getWorld(other)));
  };

  "proxies only follow their own transform"_test = [] {
    SceneFixture f;
    const MeshHandle mesh = f.meshes.create(makeGrid(2, 1));
    const TransformHandle shared = f.graph.create();
    const RenderProxyHandle p0 = f.scene.create({.transform = shared, .mesh = mesh});
    const RenderProxyHandle p1 = f.scene.create({.transform = shared, .mesh = mesh});
    const RenderProxyHandle p2 = f.scene.create({.transform = shared, .mesh = mesh});
    f.frame();

    f.scene.destroy(p1);
    f.graph.setLocalPosition(shared, Vec3(1, 0, 0));
    f.frame();
    expect(f.uploads.transforms.size() == 2u && f.findUpdate(p0) && f.findUpdate(p2));
    expect(f.getInstance(p1).flags == 0u);

    // A transform destroyed and reused before the sync only takes its own proxies.
    f.graph.destroy(shared);
    const TransformHandle reused = f.graph.create({.position = Vec3(0, 3, 0)});
    expect(reused.index() == shared.index());
    const RenderProxyHandle p3 = f.scene.create({.transform = reused, .mesh = mesh});
    f.frame();
    expect(!f.scene.isAlive(p0) && !f.scene.isAlive(p2) && f.scene.isAlive(p3));
    expect(f.scene.getCount() == 1u && f.meshes.getRefCount(mesh) == 1u);
    const GpuTransformUpdate* update = f.findUpdate(p3);
    expect(f.uploads.transforms.size() == 1u && update &&
           update->flags == GPU_TRANSFORM_UPDATE_TELEPORT);
    expect(isClose(f.gpu.worldCurr[p3.index()], f.graph.getWorld(reused)));
  };

  "proxies are released without the destroy log"_test = [] {
    SceneFixture f;
    const MeshHandle mesh = f.meshes.create(makeGrid(2, 1));
    const TransformHandle transform = f.graph.create();
    const RenderProxyHandle proxy =
      f.scene.create({.transform = transform, .mesh = mesh});
    f.frame();

    f.graph.destroy(transform);
    for (uint32 i = 0; i < 3 * SceneGraph::kDestroyLogRetention; ++i) {
      f.graph.update();
    }
    f.frame();
    expect(!f.scene.isAlive(proxy) && f.meshes.getRefCount(mesh) == 0u);
    expect(f.getInstance(proxy).flags == 0u);
  };

  "invalid proxies are rejected and the scene releases its references"_test = [] {
    SceneGraph graph;
    MaterialRegistry materials;
    MeshRegistry meshes(materials);
    const MeshHandle mesh = meshes.create(makeGrid(2, 1));
    const MaterialHandle red = materials.create({});
    const MaterialHandle dead_material = materials.create({});
    materials.destroy(dead_material);
    const TransformHandle dead_transform = graph.create();
    graph.destroy(dead_transform);
    const TransformHandle transform = graph.create();
    {
      RenderScene scene(graph, meshes, materials);
      const MaterialHandle dead_materials[] = {dead_material};
      expect(!scene.create({.transform = dead_transform, .mesh = mesh}));
      expect(!scene.create({.transform = transform, .mesh = MeshHandle{}}));
      expect(!scene.create(
        {.transform = transform, .mesh = mesh, .materials = dead_materials}));
      expect(scene.getCount() == 0u && meshes.getRefCount(mesh) == 0u);

      const MaterialHandle overrides[] = {red};
      expect(scene.create({.transform = transform, .mesh = mesh, .materials = overrides})
          .valid());
      expect(scene.create({.transform = transform, .mesh = mesh}).valid());
      expect(meshes.getRefCount(mesh) == 2u && materials.getRefCount(red) == 1u);
    }
    expect(meshes.getRefCount(mesh) == 0u && materials.getRefCount(red) == 0u);
    expect(materials.getRefCount(materials.getDefault()) == 1u);
    meshes.destroy(mesh);
    materials.destroy(red);
  };

  "proxies are found by instance id"_test = [] {
    SceneFixture f;
    const MeshHandle mesh = f.meshes.create(makeGrid(2, 1));
    const TransformHandle transform = f.graph.create();
    const RenderProxyHandle a = f.scene.create({.transform = transform, .mesh = mesh});
    const RenderProxyHandle b = f.scene.create({.transform = transform, .mesh = mesh});
    expect(f.scene.getProxyByInstanceId(RenderScene::getInstanceId(a)) == a);
    expect(f.scene.getProxyByInstanceId(RenderScene::getInstanceId(b)) == b);
    expect(!f.scene.getProxyByInstanceId(1000u));

    f.scene.destroy(a);
    expect(!f.scene.getProxyByInstanceId(RenderScene::getInstanceId(a)));
    f.scene.destroy(b);
  };

  "materials hold references on their textures"_test = [] {
    struct FakeTextures final : ITextureReferences {
      std::vector<int32> counts = std::vector<int32>(8, 0);
      void addRef(TextureDescriptorIndex texture) override { ++counts[texture.value]; }
      void release(TextureDescriptorIndex texture) override { --counts[texture.value]; }
    };

    FakeTextures textures;
    MaterialRegistry materials;
    MaterialAsset asset;
    asset.baseColorTexture = {1};
    asset.normalTexture = {2};
    asset.emissiveTexture = {1};
    const MaterialHandle before = materials.create(asset);

    // Existing materials are moved to the new references.
    materials.setTextureReferences(&textures);
    expect(textures.counts[0] == 0 && textures.counts[1] == 2 && textures.counts[2] == 1);

    const MaterialHandle copy = materials.clone(before);
    expect(textures.counts[1] == 4 && textures.counts[2] == 2);

    MaterialAsset updated = asset;
    updated.normalTexture = {3};
    updated.emissiveTexture = {};
    materials.update(copy, updated);
    expect(textures.counts[1] == 3 && textures.counts[2] == 1 && textures.counts[3] == 1);

    materials.destroy(copy);
    expect(textures.counts[1] == 2 && textures.counts[2] == 1 && textures.counts[3] == 0);

    materials.setTextureReferences(nullptr);
    expect(std::all_of(textures.counts.begin(), textures.counts.end(),
      [](int32 count) { return count == 0; }));
    // Without references, the materials no longer count.
    materials.destroy(before);
    expect(textures.counts[1] == 0);
  };

  "gpu scene element sizes"_test = [] {
    expect(getGpuSceneElementDwords(EGpuSceneBuffer::VertexPositions) == 3u);
    expect(getGpuSceneElementDwords(EGpuSceneBuffer::Indices) == 1u);
    expect(getGpuSceneElementDwords(EGpuSceneBuffer::Materials) ==
           sizeof(GpuMaterial) / sizeof(uint32));
  };

  return 0;
}
