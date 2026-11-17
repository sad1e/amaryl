#pragma once

#include "amaryl/core/assert.h"
#include "amaryl/core/span.h"
#include "amaryl/nrhi/config.h"

namespace amaryl {

struct IContext;

constexpr uint32 MAX_DEBUG_STRING_LENGTH = 256;
constexpr uint32 DEBUG_STRING_SIZE = MAX_DEBUG_STRING_LENGTH - 1;
constexpr uint32 MAX_COLOR_ATTACHMENTS = 8;
constexpr uint32 MAX_MIP_LEVELS = 16;
constexpr uint32 NRHI_WHOLE_SIZE = ~0ULL;
constexpr uint32 NUM_GRAPHICS_SHADER_STAGES = 3;

DECLARE_AMARYL_HANDLE(ComputePipeline);
DECLARE_AMARYL_HANDLE(RenderPipeline);
DECLARE_AMARYL_HANDLE(RayTracingPipeline);
DECLARE_AMARYL_HANDLE(ShaderModule);
DECLARE_AMARYL_HANDLE(Sampler);
DECLARE_AMARYL_HANDLE(Buffer);
DECLARE_AMARYL_HANDLE(Texture);
DECLARE_AMARYL_HANDLE(QueryPool);
DECLARE_AMARYL_HANDLE(AccelerationStruct);

constexpr const char* kVertexEntryName = "vertexMain";
constexpr const char* kTessControlEntryName = "tessControlMain";
constexpr const char* kTessEvalEntryName = "tessEvaluationMain";
constexpr const char* kGeometryEntryName = "geometryMain";
constexpr const char* kFragmentEntryName = "fragmentMain";
constexpr const char* kTaskEntryName = "taskMain";
constexpr const char* kMeshEntryName = "meshMain";
constexpr const char* kComputeEntryName = "computeMain";
constexpr const char* kRayGenEntryName = "rayGenMain";
constexpr const char* kAnyHitEntryName = "anyHitMain";
constexpr const char* kClosestHitEntryName = "closestHitMain";
constexpr const char* kMissEntryName = "missMain";
constexpr const char* kIntersectionEntryName = "intersectionMain";
constexpr const char* kCallableEntryName = "callableMain";

enum class EStorageType : uint8 {
  // no-gfx-api: The standard memory type should be CPU mapped GPU memory (write combined
  // CPU access). It is fast for
  // the GPU to read, and the CPU can directly write to it just like it was a CPU memory
  // pointer.
  // - Standard
  //
  // The CPU and GPU share access to the resource, allocated in system memory. Host
  // visible not means the data need a
  // extra copy in memory, but the accessible in CPU.
  HostVisible,
  // no-gfx-api: GPU-only memory is used for textures and big GPU-only buffers. The CPU
  // can't directly write to these
  // GPU pointers. The use writes the dat to CPU mapped GPU  memory first and then issues
  // a copy command, which
  // transforms the data to optimal  compressed format.
  // - GPU
  //
  // The resource is only available to the GPU.
  Device,
  // no-gfx-api: CPU-cached readback memory is used for readback purposes. This memory
  // type is slower for the GPU to
  // write due to cache coherency with the CPU. Games only use readback seldomly. Common
  // use cases are screenshots and
  // virtual texturing readback.
  // GPGPU algorithms such as AI training and inference lean on efficient communication
  // between the CPU and GPU.
  //  - Readback.
  //
  // The CPU and GPU may maintain separate copies of the resource, and any changes need to
  // be explicitly synchronized.
  //< Managed,

  // The resource’s contents are only available to the GPU, and only exist temporarily
  // during a render pass. (METAL
  // only)
  Memoryless,
};

/** The queue type. */
enum class EQueueType {
  Graphics,
  Compute,
  Transfer,
};

enum class EIndexFormat : uint8 {
  UI8,
  UI16,
  UI32,
};

enum class EColorSpace : uint8 {
  SRGB_NONLINEAR,
  SRGB_EXTENDED_LINEAR,
  HDR10,
  BT709_LINEAR,
};

enum class EPresentMode : uint8 {
  FIFO = 0,  // default mode
  FIFO_Relaxed,
  Immediate,
  Mailbox,
  Shared_Demand_Refresh,      // VK_KHR_shared_presentable_image
  Shared_Continuous_Refresh,  // VK_KHR_shared_presentable_image
  FIFO_Latest_Ready,          // VK_KHR_present_mode_fifo_latest_ready
};

enum class ETextureType : uint8 { Texture1D, Texture2D, Texture3D, TextureCube };

enum class ESamplerFilter : uint8 { Nearest = 0, Linear, Cubic };

enum class ESamplerMip : uint8 { Disabled = 0, Nearest, Linear };

enum class ESamplerWrap : uint8 {
  Repeat = 0,
  Clamp,  // to edge
  ClampToBorder,
  MirrorRepeat,
  MirrorClampToEdge,
};

enum class EHWDeviceType {
  Integrated = 1,
  Discrete = 2,
  External = 3,
  Software = 4,
};

struct HWDeviceDesc {
  enum { MAX_PHYSICAL_DEVICE_NAME_SIZE = 256 };

  uintptr_t guid = 0;
  EHWDeviceType type = EHWDeviceType::Software;
  char name[MAX_PHYSICAL_DEVICE_NAME_SIZE] = {0};
};

enum class ECullMode { None, Back, Front };

enum class EWindingMode {
  CCW,  // CounterClockwise,
  CW    // Clockwise,
};

enum class EDepthFlags {
  DepthRead = BIT(0),
  DepthWrite = BIT(1),
};
MAKE_ENUM_FLAGS(EDepthFlags);

struct Viewport {
  float x{0.0f};
  float y{0.0f};
  float width{1.0f};
  float height{1.0f};
  float minDepth{0.0f};
  float maxDepth{1.0f};
};

enum class ECompareOp : uint8 {
  Never = 0,
  Less,
  Equal,
  LessEqual,
  Greater,
  NotEqual,
  GreaterEqual,
  AlwaysPass,
};

enum class EStencilOp : uint8 {
  Keep = 0,
  Zero,
  Replace,
  IncrementClamp,
  DecrementClamp,
  Invert,
  IncrementWrap,
  DecrementWrap,
};

enum class EBlendOp : uint8 {
  Add = 0,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

enum class EBlendFactor : uint8 {
  Zero = 0,
  One,
  SrcColor,
  OneMinusSrcColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstColor,
  OneMinusDstColor,
  DstAlpha,
  OneMinusDstAlpha,
  SrcAlphaSaturated,
  BlendColor,
  OneMinusBlendColor,
  BlendAlpha,
  OneMinusBlendAlpha,
  Src1Color,
  OneMinusSrc1Color,
  Src1Alpha,
  OneMinusSrc1Alpha,
};

struct SamplerStateDesc {
  ESamplerFilter minFilter{ESamplerFilter::Linear};
  ESamplerFilter magFilter{ESamplerFilter::Linear};
  ESamplerMip mipMap{ESamplerMip::Disabled};
  ESamplerWrap wrapU{ESamplerWrap::Repeat};
  ESamplerWrap wrapV{ESamplerWrap::Repeat};
  ESamplerWrap wrapW{ESamplerWrap::Repeat};
  ECompareOp depthCompareOp{ECompareOp::LessEqual};
  uint8 mipLodMin{0};
  uint8 mipLodMax{15};
  uint8 maxAnisotropic{1};
  bool depthCompareEnabled{false};
  const char* debugName{""};
};

struct StencilState {
  EStencilOp stencilFailureOp{EStencilOp::Keep};
  EStencilOp depthFailureOp{EStencilOp::Keep};
  EStencilOp depthStencilPassOp{EStencilOp::Keep};
  ECompareOp stencilCompareOp{ECompareOp::AlwaysPass};
  uint32 readMask{static_cast<uint32>(~0)};
  uint32 writeMask{static_cast<uint32>(~0)};
};

struct DepthState {
  ECompareOp compareOp{ECompareOp::AlwaysPass};
  bool bDepthWriteEnabled{false};
};

enum class EPolygonMode : uint8 {
  Fill = 0,
  Line = 1,
  Point = 2,
};

enum class EShadingRateCombinerOp : uint8 {
  Keep = 0,
  Replace,
  Min,
  Max,
  Mul,
};

enum class EVertexFormat : uint8 {
  Invalid = 0,

  Float1,
  Float2,
  Float3,
  Float4,

  Byte1,
  Byte2,
  Byte3,
  Byte4,

  UByte1,
  UByte2,
  UByte3,
  UByte4,

  Short1,
  Short2,
  Short3,
  Short4,

  UShort1,
  UShort2,
  UShort3,
  UShort4,

  Byte2Norm,
  Byte4Norm,

  UByte2Norm,
  UByte4Norm,

  Short2Norm,
  Short4Norm,

  UShort2Norm,
  UShort4Norm,

  Int1,
  Int2,
  Int3,
  Int4,

  UInt1,
  UInt2,
  UInt3,
  UInt4,

  HalfFloat1,
  HalfFloat2,
  HalfFloat3,
  HalfFloat4,

  Int_2_10_10_10_REV,
};

enum class EVertexInputRate : uint8 {
  Vertex,
  Instance,
};

enum class ETopology {
  Point,
  Line,
  LineStrip,
  Triangle,
  TriangleStrip,
  Patch,
};

enum class EFillMode {
  Solid,
  Wireframe,
};

enum class EFormat : uint8 {
  NONE = 0,

  R_UN8,
  R_UI16,
  R_UI32,
  R_UN16,
  R_F16,
  R_F32,

  A_UN8,

  RG_UN8,
  RG_UI16,
  RG_UI32,
  RG_UN16,
  RG_F16,
  RG_F32,

  RGBA_UN8,
  RGBA_UI32,
  RGBA_F16,
  RGBA_F32,
  RGBA_SRGB8,

  BGRA_UN8,
  BGRA_SRGB8,

  A2B10G10R10_UN,
  A2R10G10B10_UN,
  A1B5G5R5_UN,

  ETC2_RGB8,
  ETC2_SRGB8,
  BC7_RGBA,
  BC7_SRGBA,

  Z_UN16,
  Z_UN24,
  Z_F32,
  Z_UN24_S_UI8,
  Z_F32_S_UI8,

  // ignore multiple plane images.
  // YUV_NV12,
  // YUV_420p,

};

enum class EBufferUsage : uint64 {
  Vertex = BIT(0),
  Index = BIT(1),
  Constant = BIT(2),    // Vulkan Uniform Buffer / D3D CBV
  ShaderRead = BIT(3),  // SRV / read-only structured/raw/storage buffer
  Storage = BIT(4),     // UAV / read-write storage buffer
  TransferSrc = BIT(5),
  TransferDst = BIT(6),
  Indirect = BIT(7),  // Draw/dispatch indirect args

  ShaderDeviceAddress = BIT(8),
  AccelerationStructureBuildInput = BIT(9),
  AccelerationStructureStorage = BIT(10),
  ShaderBindingTable = BIT(11),
};
MAKE_ENUM_FLAGS(EBufferUsage);

enum class EShaderStage : uint64 {
  None = 0,

  Vertex = BIT(1),
  Geometry = BIT(2),
  Fragment = BIT(3),

  Compute = BIT(4),

  Task = BIT(5),  // aka amplification shader in d3d12.
  Mesh = BIT(6),

  RayGeneration = BIT(7),
  ClosestHit = BIT(8),
  Miss = BIT(9),
  Intersection = BIT(10),
  AnyHit = BIT(11),
  Callable = BIT(12),
  //>
  // WorkGraph = BIT(12),

  MAX_SHADER_STAGE,
};
MAKE_ENUM_FLAGS(EShaderStage)

enum class EBarrierStage : uint32 {
  None,
  Transfer,
  Compute,
  Vertex,
  Pixel,
  RasterColorOut,
  RasterDepthOut,

  All,
};

enum class EHazardFlags : uint32 {
  DrawArguments = BIT(0),
  Descriptors = BIT(1),
  DepthStencil = BIT(2),
};
MAKE_ENUM_FLAGS(EHazardFlags);

enum class ESignal {
  AtomicSet,
  AtomicMax,
  AtomicMin,
  AtomicOr,
  // ...
};

enum class ELoadOp : uint8 {
  Load = 0,
  Clear,
  DontCare,
  None,
  Invalid = 0xFF,
};

enum class EStoreOp : uint8 {
  Store = 0,
  DontCare,
  None,
};

enum class EResolveMode : uint8 {
  None = 0,
  SampleZero,  // always supported
  Average,
  Min,
  Max,
};

union ClearColorValue {
  float float32[4];
  int32_t int32[4];
  uint32 uint32[4];
};

struct ScissorRect {
  uint32 x{0u};
  uint32 y{0u};
  uint32 width{0u};
  uint32 height{0u};
};

struct Dimensions {
  uint32 width{1u};
  uint32 height{1u};
  uint32 depth{1u};

  inline Dimensions divide1D(uint32 v) const {
    return {.width = width / v, .height = height, .depth = depth};
  }
  inline Dimensions divide2D(uint32 v) const {
    return {.width = width / v, .height = height / v, .depth = depth};
  }
  inline Dimensions divide3D(uint32 v) const {
    return {.width = width / v, .height = height / v, .depth = depth / v};
  }

  inline bool operator==(const Dimensions& other) const {
    return width == other.width && height == other.height && depth == other.depth;
  }
};

struct VertexInput final {
  enum { MAX_VERTEX_ATTRIBUTES = 16 };
  enum { MAX_VERTEX_BUFFER = 16 };

  // What data should be read
  struct VertexAttribute final {
    uint32 location{0u};
    uint32 binding{0u};
    EVertexFormat format = EVertexFormat::Invalid;
    uintptr_t offset{0u};
  } attributes[MAX_VERTEX_ATTRIBUTES];

  // Where to read, and how to move the next element
  struct VertexInputBinding final {
    uint32 stride{0u};
    EVertexInputRate inputRate{EVertexInputRate::Vertex};
  } inputBindings[MAX_VERTEX_BUFFER];

  FORCE_INLINE uint32 getNumAttributes() const {
    uint32 n = 0u;
    while (n < MAX_VERTEX_ATTRIBUTES && attributes[n].format != EVertexFormat::Invalid) {
      ++n;
    }
    return n;
  }

  FORCE_INLINE uint32 getNumInputBindings() const {
    uint32 n = 0u;
    while (n < MAX_VERTEX_BUFFER && inputBindings[n].stride) {
      ++n;
    }
    return n;
  }

  uint32 getVertexSize() const;
};

struct ColorAttachment {
  EFormat format{EFormat::NONE};
  bool blendEnabled{false};
  EBlendOp rgbBlendOp{EBlendOp::Add};
  EBlendOp alphaBlendOp{EBlendOp::Add};
  EBlendFactor srcRGBBlendFactor{EBlendFactor::One};
  EBlendFactor srcAlphaBlendFactor{EBlendFactor::One};
  EBlendFactor dstRGBBlendFactor{EBlendFactor::Zero};
  EBlendFactor dstAlphaBlendFactor{EBlendFactor::Zero};
};

struct ShaderModuleDesc {
  EShaderStage stage{EShaderStage::Fragment};
  const void* data{nullptr};
  size_t dataSize{0};
  const char* debugName{""};

  ShaderModuleDesc(const void* data, size_t dataLength, EShaderStage stage,
                   const char* debugName)
      : stage(stage), data(data), dataSize(dataLength), debugName(debugName) {
    ASSERT(dataSize);
  }
};

// The `VkSpecialization*` related API refer to the specialization in vulkan. for example,
// ```
// layout(constant_id = 0) const bool USE_SHADOW = true;
// layout(constant_id = 1) const bool USE_AO = true;
// layout(constant_id = 2) const bool USE_BLOOM = true;
// ```
// These three `constant_id` is specializations, they can reduce the shader permutations,
// the Specialization Constant solution in this situtation will generate only one
// shader/SPIR-V code, but it can contains 8 pipelines.

struct SpecializationConstantEntry {
  uint32 constantId{0u};
  uint32 offset{0u};
  uint32 size{0u};
};

struct SpecializationConstantDesc {
  enum { SPECIALIZATION_CONSTANTS_MAX = 16 };

  SpecializationConstantEntry entries[SPECIALIZATION_CONSTANTS_MAX]{};

  const void* data{nullptr};

  size_t dataSize{0u};

  FORCE_INLINE uint32 getNumSpecializationConstants() const {
    uint32 n = 0u;
    while (n < SPECIALIZATION_CONSTANTS_MAX && entries[n].size) {
      ++n;
    }
    return n;
  }
};

struct RenderPipelineDesc final {
  ETopology topology{ETopology::Triangle};

  VertexInput vertexInput;

  ShaderModuleHandle smVert;
  // ShaderModuleHandle smTesc;
  // ShaderModuleHandle smTese;
  // ShaderModuleHandle smGeom;
  ShaderModuleHandle smTask;
  ShaderModuleHandle smMesh;
  ShaderModuleHandle smFrag;

  SpecializationConstantDesc specConstantDesc{};

  const char* entryPointVert{"main"};
  const char* entryPointTesc{"main"};
  const char* entryPointTese{"main"};
  const char* entryPointGeom{"main"};
  const char* entryPointTask{"main"};
  const char* entryPointMesh{"main"};
  const char* entryPointFrag{"main"};

  ColorAttachment color[MAX_COLOR_ATTACHMENTS]{};
  uint8 colorAttachmentsCount{0u};
  EFormat depthFormat{EFormat::NONE};
  EFormat stencilFormat{EFormat::NONE};

  ECullMode cullMode{ECullMode::None};
  EWindingMode frontFace{EWindingMode::CCW};
  EPolygonMode polygonMode{EPolygonMode::Fill};

  StencilState backFaceStencil{};
  StencilState frontFaceStencil{};

  uint32 samplesCount{1u};
  // uint32 patchControlPoints{0u};
  float minSampleShading{0.0f};
  bool alphaToCoverage{false};

  const char* debugName{""};

  FORCE_INLINE uint32 getNumColorAttachments() const {
    uint32 n = 0;
    while (n < MAX_COLOR_ATTACHMENTS && color[n].format != EFormat::NONE) {
      ++n;
    }
    return n;
  }
};

struct ComputePipelineDesc final {
  ShaderModuleHandle smCompute;
  SpecializationConstantDesc specConstantDesc;
  const char* entryPoint{"main"};
  const char* debugName{""};
};

// a single hit group - one "material" worth of shaders
struct RayTracingHitGroupDesc final {
  ShaderModuleHandle smCloseHit;
  ShaderModuleHandle smAnyHit;
  ShaderModuleHandle smIntersection;
};

struct RayTracingPipelineDesc final {
  // typically just one, but spec allows more
  Span<ShaderModuleHandle> smRayGen;
  // index 0 for primary rays, 1 for shadow rays, etc
  Span<ShaderModuleHandle> smMiss;
  Span<ShaderModuleHandle> smCallable;
  // hit groups - one per material
  Span<RayTracingHitGroupDesc> hitGroups;

  SpecializationConstantDesc specConstantDesc{};

  const char* debugName = "";
};

struct RenderPass final {
  struct AttachmentDesc final {
    ELoadOp loadOp{ELoadOp::Invalid};
    EStoreOp storeOp{EStoreOp::Store};
    EResolveMode resolveMode{EResolveMode::Average};
    uint8 layer{0u};
    uint8 level{0u};
    ClearColorValue clearColor{.float32 = {0.0f, 0.0f, 0.0f, 0.0f}};
    float clearDepth{1.0f};
    uint32_t clearStencil{0u};
  };

  AttachmentDesc color[MAX_COLOR_ATTACHMENTS] = {};
  AttachmentDesc depth = {.loadOp = ELoadOp::DontCare, .storeOp = EStoreOp::DontCare};
  AttachmentDesc stencil = {.loadOp = ELoadOp::DontCare, .storeOp = EStoreOp::DontCare};

  uint32_t layerCount = 1;
  uint32_t viewMask = 0;

  uint32_t getNumColorAttachments() const {
    uint32_t n = 0;
    while (n < MAX_COLOR_ATTACHMENTS && color[n].loadOp != ELoadOp::Invalid) {
      n++;
    }
    return n;
  }
};

struct Framebuffer final {
  struct AttachmentDesc {
    TextureHandle texture;
    TextureHandle resolveTexture;
  };

  AttachmentDesc color[MAX_COLOR_ATTACHMENTS]{};
  AttachmentDesc depthStencil;
  // optional RG_UN8 map for VK_EXT_fragment_density_map; with
  // ETextureUsageBits::FragmentDensityMap
  TextureHandle fragmentDensityMap;
  // optional R_UI8 map for VK_KHR_fragment_shading_rate; with
  // ETextureUsageBits::ShadingRateAttachment
  TextureHandle shadingRateAttachment;
  // size in pixels of one texel of shadingRateAttachment
  Dimensions shadingRateAttachmentTexelSize;

  const char* debugName{""};

  uint32 getNumColorAttachments() const {
    uint32 n = 0;
    while (n < MAX_COLOR_ATTACHMENTS && color[n].texture) {
      n++;
    }
    return n;
  }
};

enum class EBufferUsageBits : uint8 {
  Index = BIT(0),
  Vertex = BIT(1),
  Uniform = BIT(2),
  Storage = BIT(3),
  Indirect = BIT(4),
  // ray tracing
  ShaderBindingTable = BIT(5),
  AccelStructBuildInputReadOnly = BIT(6),
  AccelStructStorage = BIT(7)
};
MAKE_ENUM_FLAGS(EBufferUsageBits)

struct BufferDesc final {
  uint8 usage = 0u;
  EStorageType storageType = EStorageType::HostVisible;
  size_t size = 0;
  const void* data = nullptr;
  const char* debugName = "";
};

struct Offset3D {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
};

struct TextureLayers {
  uint32 mipLevel = 0;
  uint32 layer = 0;
  uint32 numLayers = 1;
};

struct TextureRangeDesc {
  Offset3D offset = {};
  Dimensions dimensions = {1, 1, 1};
  uint32 layer = 0;
  uint32 numLayers = 1;
  uint32 mipLevel = 0;
  uint32 numMipLevels = 1;
};

enum class ETextureUsageBits : uint8 {
  Sampled = BIT(0),
  Storage = BIT(1),
  Attachment = BIT(2),
  InputAttachment = BIT(3),
  FragmentDensityMap = BIT(4),
  ShadingRateAttachment = BIT(5),
};
MAKE_ENUM_FLAGS(ETextureUsageBits);

enum Swizzle : uint8 {
  Swizzle_Default = 0,
  Swizzle_0,
  Swizzle_1,
  Swizzle_R,
  Swizzle_G,
  Swizzle_B,
  Swizzle_A,
};

struct ComponentMapping {
  Swizzle r = Swizzle_Default;
  Swizzle g = Swizzle_Default;
  Swizzle b = Swizzle_Default;
  Swizzle a = Swizzle_Default;

  bool identity() const {
    return r == Swizzle_Default && g == Swizzle_Default && b == Swizzle_Default &&
           a == Swizzle_Default;
  }
};

struct TextureDesc {
  ETextureType type{ETextureType::Texture2D};
  EFormat format{EFormat::NONE};
  Dimensions dimensions{1u, 1u, 1u};
  // array type flag for 2D/Cube
  uint32 numLayers{1u};
  uint32 numSamples{1u};
  ETextureUsageBits usage{};
  uint32 numMipLevels{1u};
  EStorageType storage{EStorageType::Device};
  ComponentMapping components{};
  const void* data{nullptr};
  //< how many mip-levels we want to upload
  uint32 dataNumMipLevels{1};
  //< generate mip-levels immediately, valid only with non-null data
  bool generateMipmaps{false};

  const char* debugName{""};
};

struct TextureViewDesc {
  ETextureType type{ETextureType::Texture2D};
  uint32 layer{0};
  uint32 numLayers{1};
  uint32 mipLevel{0};
  uint32 numMipLevels{1};
  ComponentMapping components{};
};

struct SubmitHandle {
  SubmitHandle() = default;

  explicit SubmitHandle(uint64_t handle) : handle_(handle) { ASSERT(submit_id_); }

  bool empty() const { return submit_id_ == 0; }

  uint64_t handle() const { return handle_; }

  uint32 getBufferIndex() const { return buffer_index_; }

  uint32 getQueueFamilyIndex() const { return queue_family_index_; }

  uint32 getSubmitId() const { return submit_id_; }

  void setSubmitId(uint32 submitId) { submit_id_ = submitId; }

  void setBufferIndexAndQueueFamilyIndex(uint32 buffer_index, uint32 queue_family_index) {
    buffer_index_ = buffer_index;
    queue_family_index_ = queue_family_index;
  }

 private:
  union {
    struct {
      uint32_t buffer_index_ : 16;
      //< Vulkan queue family this submission ran on; makes a handle self-describing
      uint32_t queue_family_index_ : 16;
      uint32_t submit_id_ : 32;
    };

    uint64_t handle_ = 0;
  };
};

static_assert(sizeof(SubmitHandle) == sizeof(uint64_t));

enum class EAccelStructType : uint8 {
  Invalid = 0,
  TLAS = 1,
  BLAS = 2,
};

enum class EAccelStructGeomType : uint8 {
  Triangles = 0,
  AABBs = 1,
  Instances = 2,
};

enum class EAccelStructBuildFlagBits : uint8 {
  AllowUpdate = 1 << 0,
  AllowCompaction = 1 << 1,
  PreferFastTrace = 1 << 2,
  PreferFastBuild = 1 << 3,
  LowMemory = 1 << 4,
};

enum class EAccelStructGeometryFlagBits : uint8 {
  Opaque = 1 << 0,
  NoDuplicateAnyHit = 1 << 1,
};

enum class EAccelStructInstanceFlagBits : uint8 {
  TriangleFacingCullDisable = 1 << 0,
  TriangleFlipFacing = 1 << 1,
  ForceOpaque = 1 << 2,
  ForceNoOpaque = 1 << 3,
};

struct AccelStructSizes {
  uint64 accelerationStructureSize{0};
  uint64 updateScratchSize{0};
  uint64 buildScratchSize{0};
};

struct AccelStructBuildRange {
  uint32 primitiveCount{0};
  uint32 primitiveOffset{0};
  uint32 firstVertex{0};
  uint32 transformOffset{0};
};

struct mat3x4 {
  float matrix[3][4];
};

struct AccelStructInstance {
  mat3x4 transform;
  uint32 instanceCustomIndex : 24 = 0;
  uint32 mask : 8 = 0xff;
  uint32 instanceShaderBindingTableRecordOffset : 24 = 0;
  uint32 flags : 8 =
      static_cast<uint32>(EAccelStructInstanceFlagBits::TriangleFacingCullDisable);
  uint64 accelerationStructureReference = 0;
};

struct AccelStructDesc {
  EAccelStructType type{EAccelStructType::Invalid};
  EAccelStructGeomType geometryType{EAccelStructGeomType::Triangles};
  uint8_t geometryFlags{static_cast<uint8>(EAccelStructGeometryFlagBits::Opaque)};

  EVertexFormat vertexFormat{EVertexFormat::Invalid};
  BufferHandle vertexBuffer;
  uint32_t vertexStride{0};  // zero means the size of `vertexFormat`
  uint32_t numVertices{0};
  EIndexFormat indexFormat{EIndexFormat::UI32};
  BufferHandle indexBuffer;
  BufferHandle transformBuffer;
  BufferHandle instancesBuffer;
  AccelStructBuildRange buildRange{};
  uint8_t buildFlags{static_cast<uint8>(EAccelStructBuildFlagBits::PreferFastTrace)};
  const char* debugName{""};
};

uint32 getVertexFormatSize(EVertexFormat vertex_format);
NRHI_API const char* getEntryName(EShaderStage shader_type);

template <typename T>
struct TEnableBitmaskEnum : std::false_type {};
template <>
struct TEnableBitmaskEnum<ETextureUsageBits> : std::true_type {};
template <typename T>
concept TBitmaskEnum = std::is_enum_v<T> && TEnableBitmaskEnum<T>::value;
template <TBitmaskEnum T>
constexpr bool any(T flags) noexcept {
  using U = std::underlying_type_t<T>;
  return static_cast<U>(flags) != 0;
}

}  // namespace amaryl
