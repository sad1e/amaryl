#include "amaryl/engine/render/gpu_scene_buffers.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "amaryl/core/filesystem/vfs.h"
#include "amaryl/engine/render/shader_loader.h"

namespace amaryl {

namespace {

constexpr uint32 kMaxGroupsX = 65535;
// Must match the workgroup sizes of the shaders.
constexpr uint32 kScatterGroupSize = 64;
constexpr uint32 kTransformGroupSize = 64;
// Large copies (mesh geometry) are split so they spread over many workgroups.
constexpr uint32 kScatterChunkDwords = kScatterGroupSize * 16;
constexpr size_t kMinBufferSize = 64 * 1024;
constexpr size_t kStagingAlignment = 16;

// Mirrors shaders/gpu-scene-scatter.compute.slang.
struct ScatterPushConstants {
  uint64 payload;
  uint64 copies;
  uint64 dst;
  uint32 copyCount;
  uint32 pad0;
};
static_assert(sizeof(ScatterPushConstants) == 32);

// Mirrors shaders/gpu-scene-transforms.compute.slang.
struct TransformPushConstants {
  uint64 updates;
  uint64 worldCurr;
  uint64 worldPrev;
  uint32 updateCount;
  uint32 pad0;
};
static_assert(sizeof(TransformPushConstants) == 32);

constexpr const char* kBufferNames[kGpuSceneBufferCount] = {
  "GpuScene.instances",
  "GpuScene.materialSlots",
  "GpuScene.materials",
  "GpuScene.meshes",
  "GpuScene.meshSections",
  "GpuScene.meshlets",
  "GpuScene.meshletVertices",
  "GpuScene.meshletTriangles",
  "GpuScene.indices",
  "GpuScene.vertexPositions",
  "GpuScene.vertexAttributes",
};

Holder<ComputePipelineHandle> createComputePipeline(
  IContext* rhi, const char* name, Holder<ShaderModuleHandle>& out_module) {
  Result result;
  out_module = ShaderLoader::loadShaderModule(rhi,
    std::string(kEngineScheme) + "://shaders/" + name + ".compute.spv",
    EShaderStage::Compute, name, &result);
  ASSERT(result.isOk() && out_module.valid());

  Holder<ComputePipelineHandle> pipeline = rhi->createComputePipeline(
    {.smCompute = out_module, .entryPoint = kComputeEntryName, .debugName = name},
    &result);
  ASSERT(result.isOk() && pipeline.valid());
  return pipeline;
}

Dimensions getGroupCount(uint32 count) {
  return {std::min(count, kMaxGroupsX), (count + kMaxGroupsX - 1) / kMaxGroupsX, 1};
}

}  // namespace

GpuSceneBuffers::~GpuSceneBuffers() { shutdown(); }

void GpuSceneBuffers::init(IContext* rhi) {
  ASSERT(rhi != nullptr && rhi_ == nullptr);
  rhi_ = rhi;
  scatter_pipeline_ = createComputePipeline(rhi_, "gpu-scene-scatter", scatter_module_);
  transform_pipeline_ =
    createComputePipeline(rhi_, "gpu-scene-transforms", transform_module_);

  Result result;
  root_ = rhi_->createBuffer(
    {
      .usage = static_cast<uint8>(EBufferUsageBits::Storage),
      .storageType = EStorageType::Device,
      .size = sizeof(GpuSceneRoot),
      .data = &root_data_,
      .debugName = "GpuScene.root",
    },
    "GpuScene.root", &result);
  ASSERT(result.isOk() && root_.valid());
  refreshHandles();
}

void GpuSceneBuffers::shutdown() {
  if (!rhi_) {
    return;
  }
  handles_.clear();
  retired_.clear();
  staging_pool_.clear();
  for (DeviceBuffer& buffer : buffers_) {
    buffer = {};
  }
  world_curr_ = {};
  world_prev_ = {};
  root_.reset();
  root_data_ = {};
  scatter_pipeline_.reset();
  transform_pipeline_.reset();
  scatter_module_.reset();
  transform_module_.reset();
  rhi_ = nullptr;
}

uint64 GpuSceneBuffers::getRootAddress() const {
  ASSERT(rhi_ != nullptr);
  return rhi_->gpuAddress(root_, 0);
}

uint64 GpuSceneBuffers::getAddress(const DeviceBuffer& buffer) const {
  return buffer.size != 0 ? rhi_->gpuAddress(buffer.buffer, 0) : 0;
}

bool GpuSceneBuffers::isComplete(const InFlightBuffer& buffer) const {
  return buffer.submitted && rhi_->isSubmitComplete(buffer.submit);
}

void GpuSceneBuffers::refreshHandles() {
  handles_.clear();
  for (const DeviceBuffer& buffer : buffers_) {
    if (buffer.size != 0) {
      handles_.push_back(buffer.buffer);
    }
  }
  if (world_curr_.size != 0) {
    handles_.push_back(world_curr_.buffer);
    handles_.push_back(world_prev_.buffer);
  }
  handles_.push_back(root_);
}

bool GpuSceneBuffers::reserve(ICommandBuffer& cmd, DeviceBuffer& target, size_t bytes,
                              const char* name) {
  if (bytes <= target.size) {
    return false;
  }

  const size_t size = std::max({bytes, target.size + target.size / 2, kMinBufferSize});
  Result result;
  Holder<BufferHandle> buffer = rhi_->createBuffer(
    {
      .usage = static_cast<uint8>(EBufferUsageBits::Storage),
      .storageType = EStorageType::Device,
      .size = size,
      .debugName = name,
    },
    name, &result);
  ASSERT(result.isOk() && buffer.valid());

  if (target.size != 0) {
    // The records keep their offsets, the new tail is written before it is read.
    cmd.cmdCopyBuffer(target.buffer, buffer, 0, 0, target.size);
    retired_.push_back({std::move(target.buffer), target.size});
  }
  target.buffer = std::move(buffer);
  target.size = size;
  return true;
}

void GpuSceneBuffers::onSubmitted(SubmitHandle submit) {
  for (InFlightBuffer& buffer : retired_) {
    if (!buffer.submitted) {
      buffer.submit = submit;
      buffer.submitted = true;
    }
  }
  for (InFlightBuffer& buffer : staging_pool_) {
    if (!buffer.submitted) {
      buffer.submit = submit;
      buffer.submitted = true;
    }
  }
}

BufferHandle GpuSceneBuffers::acquireStaging(size_t bytes) {
  // Reuses a staging buffer the GPU is done with, the smallest one large enough.
  InFlightBuffer* best = nullptr;
  for (InFlightBuffer& buffer : staging_pool_) {
    if (buffer.size >= bytes && isComplete(buffer) &&
        (!best || buffer.size < best->size)) {
      best = &buffer;
    }
  }

  if (!best) {
    // Free buffers too small for this frame are dropped.
    std::erase_if(staging_pool_, [&](const InFlightBuffer& buffer) {
      return buffer.size < bytes && isComplete(buffer);
    });

    size_t size = kMinBufferSize;
    while (size < bytes) {
      size *= 2;
    }
    Result result;
    Holder<BufferHandle> buffer = rhi_->createBuffer(
      {
        .usage = static_cast<uint8>(EBufferUsageBits::Storage),
        .storageType = EStorageType::HostVisible,
        .size = size,
        .debugName = "GpuScene.staging",
      },
      "GpuScene.staging", &result);
    ASSERT(result.isOk() && buffer.valid());
    staging_pool_.push_back({std::move(buffer), size});
    best = &staging_pool_.back();
  }

  best->submitted = false;
  best->submit = {};
  return best->buffer;
}

void GpuSceneBuffers::upload(ICommandBuffer& cmd, const GpuSceneUploads& uploads) {
  ASSERT(rhi_ != nullptr);
  std::erase_if(retired_, [this](const InFlightBuffer& buffer) { return isComplete(buffer); });

  cmd.cmdPushDebugGroupLabel("GpuScene upload", 0xff80c0ffu);

  // 1. Grow the buffers to the capacities of this frame.
  bool grown = false;
  for (uint32 b = 0; b < kGpuSceneBufferCount; ++b) {
    const auto id = static_cast<EGpuSceneBuffer>(b);
    grown |= reserve(cmd, buffers_[b],
      size_t{uploads.capacity(id)} * getGpuSceneElementDwords(id) * sizeof(uint32),
      kBufferNames[b]);
  }
  const size_t transform_bytes =
    size_t{uploads.capacity(EGpuSceneBuffer::Instances)} * sizeof(GpuTransform);
  grown |= reserve(cmd, world_curr_, transform_bytes, "GpuScene.worldCurr");
  grown |= reserve(cmd, world_prev_, transform_bytes, "GpuScene.worldPrev");
  if (grown) {
    refreshHandles();
  }

  // 2. Pack the payloads, the copies (split in chunks) and the transforms.
  staging_bytes_.clear();
  auto append = [this](const void* data, size_t size) {
    const size_t offset =
      (staging_bytes_.size() + kStagingAlignment - 1) & ~(kStagingAlignment - 1);
    staging_bytes_.resize(offset + size);
    std::memcpy(staging_bytes_.data() + offset, data, size);
    return offset;
  };

  size_t payload_offsets[kGpuSceneBufferCount]{};
  size_t copy_offsets[kGpuSceneBufferCount]{};
  uint32 copy_counts[kGpuSceneBufferCount]{};
  for (uint32 b = 0; b < kGpuSceneBufferCount; ++b) {
    const GpuScatterQueue& queue = uploads.buffers[b];
    if (queue.empty()) {
      continue;
    }
    ASSERT(buffers_[b].size != 0);

    copies_.clear();
    for (const GpuScatterCopy& copy : queue.copies) {
      for (uint32 done = 0; done < copy.dwordCount; done += kScatterChunkDwords) {
        copies_.push_back({copy.dstOffset + done, copy.srcOffset + done,
                           std::min(kScatterChunkDwords, copy.dwordCount - done), 0});
      }
    }
    payload_offsets[b] =
      append(queue.payload.data(), queue.payload.size() * sizeof(uint32));
    copy_offsets[b] = append(copies_.data(), copies_.size() * sizeof(GpuScatterCopy));
    copy_counts[b] = static_cast<uint32>(copies_.size());
  }

  const uint32 transform_count = static_cast<uint32>(uploads.transforms.size());
  const size_t transforms_offset =
    transform_count != 0 ? append(uploads.transforms.data(),
                                  transform_count * sizeof(GpuTransformUpdate))
                         : 0;

  uint64 staging_address = 0;
  if (!staging_bytes_.empty()) {
    const BufferHandle staging = acquireStaging(staging_bytes_.size());
    rhi_->upload(staging, staging_bytes_.data(), staging_bytes_.size(), 0);
    staging_address = rhi_->gpuAddress(staging, 0);
  }

  // 3. Scatter, one workgroup per copy. The copies of a buffer never overlap.
  bool pipeline_bound = false;
  for (uint32 b = 0; b < kGpuSceneBufferCount; ++b) {
    if (copy_counts[b] == 0) {
      continue;
    }
    if (!pipeline_bound) {
      cmd.cmdBindComputePipeline(scatter_pipeline_);
      pipeline_bound = true;
    }
    const ScatterPushConstants push{
      .payload = staging_address + payload_offsets[b],
      .copies = staging_address + copy_offsets[b],
      .dst = getAddress(buffers_[b]),
      .copyCount = copy_counts[b],
      .pad0 = 0,
    };
    cmd.cmdPushConstants(push, 0);
    // Orders the writes after the reads of the previous frame and the growth copy.
    BufferHandle dst = buffers_[b].buffer;
    cmd.cmdDispatch(getGroupCount(copy_counts[b]), {.buffers = Span<BufferHandle>(&dst, 1)});
  }

  // 4. Transforms: curr / prev rotation, teleport and settle.
  if (transform_count != 0) {
    cmd.cmdBindComputePipeline(transform_pipeline_);
    const TransformPushConstants push{
      .updates = staging_address + transforms_offset,
      .worldCurr = getAddress(world_curr_),
      .worldPrev = getAddress(world_prev_),
      .updateCount = transform_count,
      .pad0 = 0,
    };
    cmd.cmdPushConstants(push, 0);
    BufferHandle deps[] = {world_curr_.buffer, world_prev_.buffer};
    cmd.cmdDispatch({(transform_count + kTransformGroupSize - 1) / kTransformGroupSize, 1, 1},
                    {.buffers = deps});
  }

  // 5. Root, the addresses change when a buffer grows.
  using B = EGpuSceneBuffer;
  const auto address = [this](B id) { return getAddress(buffers_[static_cast<uint32>(id)]); };
  root_data_.instances = address(B::Instances);
  root_data_.worldCurr = getAddress(world_curr_);
  root_data_.worldPrev = getAddress(world_prev_);
  root_data_.skinningMatrices = 0;
  root_data_.materialSlots = address(B::MaterialSlots);
  root_data_.materials = address(B::Materials);
  root_data_.meshes = address(B::Meshes);
  root_data_.meshSections = address(B::MeshSections);
  root_data_.meshlets = address(B::Meshlets);
  root_data_.meshletVertices = address(B::MeshletVertices);
  root_data_.meshletTriangles = address(B::MeshletTriangles);
  root_data_.indices = address(B::Indices);
  root_data_.vertexPositions = address(B::VertexPositions);
  root_data_.vertexAttributes = address(B::VertexAttributes);
  root_data_.instanceCapacity = uploads.capacity(B::Instances);
  root_data_.meshCount = uploads.capacity(B::Meshes);
  root_data_.materialCount = uploads.capacity(B::Materials);
  root_data_.frameIndex = frame_index_++;
  cmd.cmdUpdateBuffer(root_, root_data_);

  cmd.cmdPopDebugGroupLabel();
}

}  // namespace amaryl
