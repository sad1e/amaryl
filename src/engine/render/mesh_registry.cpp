#include "amaryl/engine/render/mesh_registry.h"

#include "amaryl/core/logger.h"
#include "amaryl/engine/render/material_registry.h"

namespace amaryl {
namespace {

constexpr uint32 kPositionDwords = 3;

bool validateAsset(const MeshAsset& asset) {
  const size_t vertex_count = asset.positions.size();
  const size_t lod_count = asset.lods.size();
  bool valid =
      vertex_count != 0 &&
      asset.attributes.size() == vertex_count * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS &&
      !asset.indices.empty() && asset.sectionCount != 0 &&
      asset.sectionCount <= 0xffffu && lod_count != 0 &&
      lod_count <= GPU_SCENE_MAX_LODS &&
      asset.sections.size() == lod_count * asset.sectionCount &&
      !asset.meshlets.empty() && !asset.meshletVertices.empty() &&
      !asset.meshletTriangles.empty() && asset.meshletTriangles.size() % 4 == 0;
  for (const GpuMeshLod& lod : asset.lods) {
    valid = valid &&
            size_t{lod.meshletOffset} + lod.meshletCount <= asset.meshlets.size() &&
            size_t{lod.sectionOffset} + asset.sectionCount <= asset.sections.size();
  }
  for (const GpuMeshSection& section : asset.sections) {
    valid = valid &&
            size_t{section.firstIndex} + section.indexCount <= asset.indices.size() &&
            section.materialSlot < asset.sectionCount;
  }
  for (const GpuMeshlet& meshlet : asset.meshlets) {
    const uint32 counts = meshlet.packedCounts;
    valid = valid &&
            size_t{meshlet.vertexOffset} + gpuMeshletVertexCount(counts) <=
                asset.meshletVertices.size() &&
            meshlet.triangleOffset % 4 == 0 &&
            size_t{meshlet.triangleOffset} + gpuMeshletTriangleCount(counts) * 3 <=
                asset.meshletTriangles.size();
  }
  for (const uint32 index : asset.indices) {
    valid = valid && index < vertex_count;
  }
  for (const uint32 index : asset.meshletVertices) {
    valid = valid && index < vertex_count;
  }
  return valid;
}

}  // namespace

MeshRegistry::MeshRegistry(MaterialRegistry& materials) : materials_(materials) {}

MeshRegistry::~MeshRegistry() {
  pool_.for_each([this](const Record& record) {
    for (const MaterialHandle material : record.defaultMaterials) {
      materials_.release(material);
    }
  });
}

MeshHandle MeshRegistry::create(std::shared_ptr<const MeshAsset> asset,
                                Span<const MaterialHandle> default_materials) {
  if (!asset || !validateAsset(*asset)) {
    ALOGE("MeshRegistry: malformed mesh asset '%s'", asset ? asset->name.c_str() : "");
    return {};
  }

  Record record;
  record.defaultMaterials.resize(asset->sectionCount, materials_.getDefault());
  for (size_t i = 0; i < record.defaultMaterials.size() && i < default_materials.size();
       ++i) {
    if (default_materials[i]) {
      record.defaultMaterials[i] = default_materials[i];
    }
  }
  for (const MaterialHandle material : record.defaultMaterials) {
    materials_.addRef(material);
  }

  record.baseVertex = vertices_.allocate(static_cast<uint32>(asset->positions.size()));
  record.indexOffset = indices_.allocate(static_cast<uint32>(asset->indices.size()));
  record.sectionOffset = sections_.allocate(static_cast<uint32>(asset->sections.size()));
  record.meshletOffset = meshlets_.allocate(static_cast<uint32>(asset->meshlets.size()));
  record.meshletVertexOffset =
      meshlet_vertices_.allocate(static_cast<uint32>(asset->meshletVertices.size()));
  record.meshletTriangleOffset = meshlet_triangles_.allocate(
      static_cast<uint32>(asset->meshletTriangles.size() / 4));
  record.pendingUpload = true;
  record.asset = std::move(asset);

  const MeshHandle handle = pool_.create(std::move(record));
  pending_slots_.push_back(handle.index());
  return handle;
}

void MeshRegistry::destroy(MeshHandle handle) {
  Record& record = getRecord(handle);
  ASSERT(record.refCount == 0, "Destroying a mesh in use");

  const MeshAsset& asset = *record.asset;
  vertices_.free(record.baseVertex, static_cast<uint32>(asset.positions.size()));
  indices_.free(record.indexOffset, static_cast<uint32>(asset.indices.size()));
  sections_.free(record.sectionOffset, static_cast<uint32>(asset.sections.size()));
  meshlets_.free(record.meshletOffset, static_cast<uint32>(asset.meshlets.size()));
  meshlet_vertices_.free(record.meshletVertexOffset,
                         static_cast<uint32>(asset.meshletVertices.size()));
  meshlet_triangles_.free(record.meshletTriangleOffset,
                          static_cast<uint32>(asset.meshletTriangles.size() / 4));
  for (const MaterialHandle material : record.defaultMaterials) {
    materials_.release(material);
  }
  pool_.destroy(handle);
}

bool MeshRegistry::isAlive(MeshHandle handle) const {
  return handle.index() < pool_.getSizeOfObjects() &&
         pool_.getHandle(handle.index()) == handle;
}

const MeshAsset& MeshRegistry::getAsset(MeshHandle handle) const {
  return *getRecord(handle).asset;
}

uint32 MeshRegistry::getSectionCount(MeshHandle handle) const {
  return getRecord(handle).asset->sectionCount;
}

MaterialHandle MeshRegistry::getDefaultMaterial(MeshHandle handle, uint32 section) const {
  const Record& record = getRecord(handle);
  ASSERT(section < record.defaultMaterials.size(), "Invalid mesh section");
  return record.defaultMaterials[section];
}

void MeshRegistry::addRef(MeshHandle handle) { ++getRecord(handle).refCount; }

void MeshRegistry::release(MeshHandle handle) {
  Record& record = getRecord(handle);
  ASSERT(record.refCount > 0, "Mesh reference count underflow");
  --record.refCount;
}

uint32 MeshRegistry::getRefCount(MeshHandle handle) const {
  return getRecord(handle).refCount;
}

void MeshRegistry::flushUploads(GpuSceneUploads& uploads) {
  for (const uint32 slot : pending_slots_) {
    Record& record = *pool_.get(pool_.getHandle(slot));
    if (record.pendingUpload) {
      record.pendingUpload = false;
      upload(slot, record, uploads);
    }
  }
  pending_slots_.clear();

  uploads.capacity(EGpuSceneBuffer::Meshes) = pool_.getSizeOfObjects();
  uploads.capacity(EGpuSceneBuffer::MeshSections) = sections_.getEnd();
  uploads.capacity(EGpuSceneBuffer::Meshlets) = meshlets_.getEnd();
  uploads.capacity(EGpuSceneBuffer::MeshletVertices) = meshlet_vertices_.getEnd();
  uploads.capacity(EGpuSceneBuffer::MeshletTriangles) = meshlet_triangles_.getEnd();
  uploads.capacity(EGpuSceneBuffer::Indices) = indices_.getEnd();
  uploads.capacity(EGpuSceneBuffer::VertexPositions) = vertices_.getEnd();
  uploads.capacity(EGpuSceneBuffer::VertexAttributes) = vertices_.getEnd();
}

MeshRegistry::Record& MeshRegistry::getRecord(MeshHandle handle) {
  ASSERT(isAlive(handle), "Invalid mesh handle");
  return *pool_.get(handle);
}

const MeshRegistry::Record& MeshRegistry::getRecord(MeshHandle handle) const {
  ASSERT(isAlive(handle), "Invalid mesh handle");
  return *pool_.get(handle);
}

void MeshRegistry::upload(uint32 slot, const Record& record,
                          GpuSceneUploads& uploads) const {
  const MeshAsset& asset = *record.asset;
  const uint32 vertex_count = asset.getVertexCount();
  uploads[EGpuSceneBuffer::VertexPositions].write(record.baseVertex * kPositionDwords,
                                                  asset.positions.data(),
                                                  vertex_count * kPositionDwords);
  uploads[EGpuSceneBuffer::VertexAttributes].write(
      record.baseVertex * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS, asset.attributes.data(),
      vertex_count * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS);
  uploads[EGpuSceneBuffer::Indices].write(record.indexOffset, asset.indices.data(),
                                          static_cast<uint32>(asset.indices.size()));
  uploads[EGpuSceneBuffer::MeshletVertices].write(
      record.meshletVertexOffset, asset.meshletVertices.data(),
      static_cast<uint32>(asset.meshletVertices.size()));
  uploads[EGpuSceneBuffer::MeshletTriangles].write(
      record.meshletTriangleOffset, asset.meshletTriangles.data(),
      static_cast<uint32>(asset.meshletTriangles.size() / 4));

  std::vector<GpuMeshSection> sections = asset.sections;
  for (GpuMeshSection& section : sections) {
    section.firstIndex += record.indexOffset;
  }
  uploads[EGpuSceneBuffer::MeshSections].writeElements<GpuMeshSection>(
      record.sectionOffset, sections);

  std::vector<GpuMeshlet> meshlets = asset.meshlets;
  for (GpuMeshlet& meshlet : meshlets) {
    meshlet.vertexOffset += record.meshletVertexOffset;
    meshlet.triangleOffset += record.meshletTriangleOffset * 4;
  }
  uploads[EGpuSceneBuffer::Meshlets].writeElements<GpuMeshlet>(record.meshletOffset,
                                                               meshlets);

  const Vec3 aabb_min(asset.aabbMin);
  const Vec3 aabb_max(asset.aabbMax);
  const Vec3 center = (aabb_min + aabb_max) * 0.5f;
  const Vec3 extents = (aabb_max - aabb_min) * 0.5f;
  GpuMesh mesh{};
  mesh.boundingSphere = asset.boundingSphere;
  mesh.aabbCenter = float4{center.GetX(), center.GetY(), center.GetZ(), 0.0f};
  mesh.aabbExtents = float4{extents.GetX(), extents.GetY(), extents.GetZ(), 0.0f};
  mesh.baseVertex = record.baseVertex;
  mesh.lodCount = static_cast<uint32>(asset.lods.size());
  mesh.sectionCount = asset.sectionCount;
  for (uint32 i = 0; i < mesh.lodCount; ++i) {
    mesh.lods[i] = asset.lods[i];
    mesh.lods[i].meshletOffset += record.meshletOffset;
    mesh.lods[i].sectionOffset += record.sectionOffset;
  }
  uploads[EGpuSceneBuffer::Meshes].writeElement(slot, mesh);
}

}  // namespace amaryl
