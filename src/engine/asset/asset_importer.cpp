#include "amaryl/engine/asset/asset_importer.h"

#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

#include "amaryl/core/logger.h"
#include "cgltf/cgltf.h"

namespace amaryl {
namespace {

static_assert(sizeof(float2) == 2 * sizeof(float) &&
                  sizeof(float3) == 3 * sizeof(float) &&
                  sizeof(float4) == 4 * sizeof(float),
              "Accessors are unpacked in place");

struct CgltfDataDeleter {
  void operator()(cgltf_data* data) const { cgltf_free(data); }
};
using CgltfDataPtr = std::unique_ptr<cgltf_data, CgltfDataDeleter>;

std::string getDirectory(const char* path) {
  const std::string_view view(path);
  const size_t slash = view.find_last_of("/\\");
  return slash == std::string_view::npos ? std::string()
                                         : std::string(view.substr(0, slash + 1));
}

bool isRequiredExtensionSupported(const char* name) {
  // Compressed geometry cannot be decoded yet.
  return std::strcmp(name, "KHR_draco_mesh_compression") != 0 &&
         std::strcmp(name, "EXT_meshopt_compression") != 0 &&
         std::strcmp(name, "KHR_meshopt_compression") != 0;
}

// "data:[<mime type>];base64,<data>"
bool decodeDataUri(const cgltf_data& data, const char* uri, std::vector<uint8>& out,
                   std::string& mime_type) {
  const char* comma = std::strchr(uri, ',');
  if (!comma || comma - uri < 12 || std::strncmp(comma - 7, ";base64", 7) != 0) {
    return false;
  }
  if (mime_type.empty()) {
    mime_type.assign(uri + 5, comma - 7);
  }

  const char* base64 = comma + 1;
  const size_t length = std::strlen(base64);
  size_t padding = 0;
  while (padding < 2 && padding < length && base64[length - 1 - padding] == '=') {
    ++padding;
  }
  const size_t size = length * 3 / 4 - padding;
  if (size == 0) {
    return false;
  }

  cgltf_options options{};
  options.memory = data.memory;
  void* bytes = nullptr;
  if (cgltf_load_buffer_base64(&options, size, base64, &bytes) != cgltf_result_success) {
    return false;
  }
  out.assign(static_cast<const uint8*>(bytes), static_cast<const uint8*>(bytes) + size);
  data.memory.free_func(data.memory.user_data, bytes);
  return true;
}

const cgltf_image* getImage(const cgltf_texture& texture) {
  if (texture.image) {
    return texture.image;
  }
  return texture.basisu_image ? texture.basisu_image : texture.webp_image;
}

// Loads every image and sampler once through the import callbacks.
struct TextureResolver {
  TextureResolver(const cgltf_data& data, const GltfImportOptions& options,
                  std::string directory)
      : data_(data),
        options_(options),
        directory_(std::move(directory)),
        images_(data.images_count * 2),
        samplers_(data.samplers_count + 1) {}

  TextureDescriptorIndex resolveTexture(const cgltf_texture_view& view, bool srgb) {
    if (!view.texture || !options_.loadImage) {
      return {};
    }
    const cgltf_image* image = getImage(*view.texture);
    if (!image) {
      ALOGW("glTF: texture %zu has no image", cgltf_texture_index(&data_, view.texture));
      return {};
    }
    if (view.texcoord != 0) {
      ALOGW("glTF: TEXCOORD_%d is not imported, TEXCOORD_0 is used", view.texcoord);
    }

    const size_t image_index = cgltf_image_index(&data_, image);
    std::optional<TextureDescriptorIndex>& texture =
        images_[image_index * 2 + (srgb ? 1 : 0)];
    if (!texture) {
      texture = loadImage(*image, static_cast<uint32>(image_index), srgb);
    }
    return *texture;
  }

  // glTF samplers are per texture, the whole material uses the sampler of `texture`.
  SamplerDescriptorIndex resolveSampler(const cgltf_texture& texture) {
    if (!options_.createSampler) {
      return {};
    }
    const cgltf_sampler* sampler = texture.sampler;
    std::optional<SamplerDescriptorIndex>& descriptor =
        samplers_[sampler ? cgltf_sampler_index(&data_, sampler) + 1 : 0];
    if (!descriptor) {
      GltfSamplerDesc desc;
      if (sampler) {
        desc.magFilter = static_cast<uint32>(sampler->mag_filter);
        desc.minFilter = static_cast<uint32>(sampler->min_filter);
        desc.wrapS = static_cast<uint32>(sampler->wrap_s);
        desc.wrapT = static_cast<uint32>(sampler->wrap_t);
      }
      descriptor = options_.createSampler(desc);
    }
    return *descriptor;
  }

 private:
  TextureDescriptorIndex loadImage(const cgltf_image& image, uint32 index, bool srgb) {
    GltfImageDesc desc;
    desc.index = index;
    desc.name = image.name ? image.name : "";
    desc.mimeType = image.mime_type ? image.mime_type : "";
    desc.srgb = srgb;

    std::vector<uint8> decoded;
    if (image.buffer_view) {
      const uint8* bytes = cgltf_buffer_view_data(image.buffer_view);
      if (!bytes) {
        ALOGW("glTF: image %u has no data", index);
        return {};
      }
      desc.data = Span<const uint8>(bytes, image.buffer_view->size);
    } else if (image.uri && std::strncmp(image.uri, "data:", 5) == 0) {
      if (!decodeDataUri(data_, image.uri, decoded, desc.mimeType)) {
        ALOGW("glTF: image %u has an invalid data uri", index);
        return {};
      }
      desc.data = decoded;
    } else if (image.uri && !std::strstr(image.uri, "://")) {
      std::string uri = image.uri;
      uri.resize(cgltf_decode_uri(uri.data()));
      desc.path = directory_ + uri;
    } else {
      ALOGW("glTF: image %u has no supported source", index);
      return {};
    }
    return options_.loadImage(desc);
  }

  const cgltf_data& data_;
  const GltfImportOptions& options_;
  std::string directory_;
  // Per image and color space, [image * 2 + srgb].
  std::vector<std::optional<TextureDescriptorIndex>> images_;
  // Per sampler, 0 is the default sampler.
  std::vector<std::optional<SamplerDescriptorIndex>> samplers_;
};

MaterialAsset convertMaterial(const cgltf_material& material, size_t index,
                              TextureResolver& textures) {
  MaterialAsset out;
  out.name = material.name ? material.name : "material_" + std::to_string(index);
  out.shadingModel = material.unlit ? EShadingModel::Unlit : EShadingModel::Lit;
  switch (material.alpha_mode) {
    case cgltf_alpha_mode_mask:
      out.alphaMode = EAlphaMode::Mask;
      break;
    case cgltf_alpha_mode_blend:
      out.alphaMode = EAlphaMode::Blend;
      break;
    default:
      out.alphaMode = EAlphaMode::Opaque;
      break;
  }
  out.doubleSided = material.double_sided;
  out.alphaCutoff = material.alpha_cutoff;

  // cgltf fills the glTF defaults when pbrMetallicRoughness is missing.
  const cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;
  const float* base_color = pbr.base_color_factor;
  out.baseColorFactor =
      float4{base_color[0], base_color[1], base_color[2], base_color[3]};
  out.metallicFactor = pbr.metallic_factor;
  out.roughnessFactor = pbr.roughness_factor;
  const float strength = material.has_emissive_strength
                             ? material.emissive_strength.emissive_strength
                             : 1.0f;
  const float* emissive = material.emissive_factor;
  out.emissiveFactor =
      float3{emissive[0] * strength, emissive[1] * strength, emissive[2] * strength};
  out.normalScale =
      material.normal_texture.texture ? material.normal_texture.scale : 1.0f;
  out.occlusionStrength =
      material.occlusion_texture.texture ? material.occlusion_texture.scale : 1.0f;

  if (material.has_pbr_metallic_roughness) {
    out.baseColorTexture = textures.resolveTexture(pbr.base_color_texture, true);
    out.metallicRoughnessTexture =
        textures.resolveTexture(pbr.metallic_roughness_texture, false);
  }
  out.normalTexture = textures.resolveTexture(material.normal_texture, false);
  out.occlusionTexture = textures.resolveTexture(material.occlusion_texture, false);
  out.emissiveTexture = textures.resolveTexture(material.emissive_texture, true);

  const cgltf_texture_view* views[] = {
      material.has_pbr_metallic_roughness ? &pbr.base_color_texture : nullptr,
      material.has_pbr_metallic_roughness ? &pbr.metallic_roughness_texture : nullptr,
      &material.normal_texture,
      &material.occlusion_texture,
      &material.emissive_texture,
  };
  for (const cgltf_texture_view* view : views) {
    if (view && view->texture) {
      out.sampler = textures.resolveSampler(*view->texture);
      break;
    }
  }
  return out;
}

struct PrimitiveStreams {
  std::vector<float3> positions;
  std::vector<float3> normals;
  std::vector<float4> tangents;
  std::vector<float2> uvs;
  std::vector<uint32> indices;
};

// Missing optional streams stay empty.
template <typename T>
bool unpackStream(const cgltf_accessor* accessor, cgltf_type type, size_t count,
                  std::vector<T>& out) {
  if (!accessor) {
    return true;
  }
  if (accessor->type != type || accessor->count != count) {
    return false;
  }
  out.resize(count);
  const size_t float_count = count * cgltf_num_components(type);
  return cgltf_accessor_unpack_floats(accessor, reinterpret_cast<float*>(out.data()),
                                      float_count) == float_count;
}

bool readPrimitive(const cgltf_primitive& primitive, PrimitiveStreams& out) {
  const cgltf_accessor* positions = nullptr;
  const cgltf_accessor* normals = nullptr;
  const cgltf_accessor* tangents = nullptr;
  const cgltf_accessor* uvs = nullptr;
  for (size_t i = 0; i < primitive.attributes_count; ++i) {
    const cgltf_attribute& attribute = primitive.attributes[i];
    if (attribute.index != 0) {
      continue;
    }
    switch (attribute.type) {
      case cgltf_attribute_type_position:
        positions = attribute.data;
        break;
      case cgltf_attribute_type_normal:
        normals = attribute.data;
        break;
      case cgltf_attribute_type_tangent:
        tangents = attribute.data;
        break;
      case cgltf_attribute_type_texcoord:
        uvs = attribute.data;
        break;
      default:
        break;
    }
  }
  if (!positions || positions->count == 0) {
    return false;
  }

  const size_t count = positions->count;
  if (!unpackStream(positions, cgltf_type_vec3, count, out.positions) ||
      !unpackStream(normals, cgltf_type_vec3, count, out.normals) ||
      !unpackStream(tangents, cgltf_type_vec4, count, out.tangents) ||
      !unpackStream(uvs, cgltf_type_vec2, count, out.uvs)) {
    return false;
  }
  if (!primitive.indices) {
    return count % 3 == 0;
  }

  const size_t index_count = primitive.indices->count;
  if (index_count == 0 || index_count % 3 != 0) {
    return false;
  }
  out.indices.resize(index_count);
  return cgltf_accessor_unpack_indices(primitive.indices, out.indices.data(),
                                       sizeof(uint32), index_count) == index_count;
}

GltfMesh convertMesh(const cgltf_data& data, const cgltf_mesh& mesh, size_t index,
                     const MeshBuildOptions& options) {
  auto asset = std::make_shared<MeshAsset>();
  asset->name = mesh.name ? mesh.name : "mesh_" + std::to_string(index);

  GltfMesh out;
  std::vector<PrimitiveStreams> primitives;
  primitives.reserve(mesh.primitives_count);
  for (size_t i = 0; i < mesh.primitives_count; ++i) {
    const cgltf_primitive& primitive = mesh.primitives[i];
    if (primitive.type != cgltf_primitive_type_triangles) {
      ALOGW("glTF: skipped non-triangle primitive %zu of mesh '%s'", i,
            asset->name.c_str());
      continue;
    }
    if (!readPrimitive(primitive, primitives.emplace_back())) {
      ALOGW("glTF: skipped invalid primitive %zu of mesh '%s'", i, asset->name.c_str());
      primitives.pop_back();
      continue;
    }
    out.sectionMaterials.push_back(
        primitive.material
            ? static_cast<int32>(cgltf_material_index(&data, primitive.material))
            : -1);
  }

  std::vector<MeshSourceSection> sections;
  sections.reserve(primitives.size());
  for (const PrimitiveStreams& streams : primitives) {
    sections.push_back({streams.positions, streams.normals, streams.tangents, streams.uvs,
                        streams.indices});
  }
  if (sections.empty() || !buildMeshAsset(sections, options, *asset)) {
    ALOGW("glTF: mesh '%s' has no valid primitive", asset->name.c_str());
    out.sectionMaterials.clear();
    return out;
  }
  out.asset = std::move(asset);
  return out;
}

void convertNodeTransform(const cgltf_node& node, GltfNode& out) {
  if (node.has_matrix) {
    const float* m = node.matrix;
    const Mat44 matrix(Vec4(m[0], m[1], m[2], m[3]), Vec4(m[4], m[5], m[6], m[7]),
                       Vec4(m[8], m[9], m[10], m[11]), Vec4(m[12], m[13], m[14], m[15]));
    Vec3 scale;
    const Mat44 rotation_translation = matrix.Decompose(scale);
    out.position = rotation_translation.GetTranslation();
    out.rotation = rotation_translation.GetRotation().GetQuaternion().Normalized();
    out.scale = scale;
    return;
  }

  if (node.has_translation) {
    out.position = Vec3(node.translation[0], node.translation[1], node.translation[2]);
  }
  if (node.has_rotation) {
    const Quat rotation(node.rotation[0], node.rotation[1], node.rotation[2],
                        node.rotation[3]);
    out.rotation = rotation.LengthSq() > 0.0f ? rotation.Normalized() : Quat::sIdentity();
  }
  if (node.has_scale) {
    out.scale = Vec3(node.scale[0], node.scale[1], node.scale[2]);
  }
}

// Depth first from the roots of the default scene, parents before children.
void convertNodes(const cgltf_data& data, const std::vector<GltfMesh>& meshes,
                  std::vector<GltfNode>& out) {
  std::vector<std::pair<const cgltf_node*, int32>> stack;
  const cgltf_scene* scene = data.scene ? data.scene : data.scenes;
  if (scene) {
    for (size_t i = scene->nodes_count; i-- > 0;) {
      const cgltf_node* node = scene->nodes[i];
      if (node->parent) {
        ALOGW("glTF: skipped scene root '%s' with a parent",
              node->name ? node->name : "");
        continue;
      }
      stack.emplace_back(node, -1);
    }
  } else {
    for (size_t i = data.nodes_count; i-- > 0;) {
      if (!data.nodes[i].parent) {
        stack.emplace_back(&data.nodes[i], -1);
      }
    }
  }

  std::vector<bool> visited(data.nodes_count, false);
  while (!stack.empty()) {
    const auto [node, parent] = stack.back();
    stack.pop_back();
    const size_t node_index = cgltf_node_index(&data, node);
    if (visited[node_index]) {
      continue;
    }
    visited[node_index] = true;

    const int32 index = static_cast<int32>(out.size());
    GltfNode& converted = out.emplace_back();
    converted.name = node->name ? node->name : "";
    converted.parent = parent;
    convertNodeTransform(*node, converted);
    if (node->mesh) {
      const size_t mesh_index = cgltf_mesh_index(&data, node->mesh);
      converted.mesh = meshes[mesh_index].asset ? static_cast<int32>(mesh_index) : -1;
    }
    for (size_t i = node->children_count; i-- > 0;) {
      stack.emplace_back(node->children[i], index);
    }
  }
}

bool importParsed(const cgltf_options& options, cgltf_data* data, const char* path,
                  const GltfImportOptions& import_options, GltfImportResult& out) {
  cgltf_result result = cgltf_load_buffers(&options, data, path);
  if (result != cgltf_result_success) {
    ALOGE("glTF: cannot load the buffers of '%s' (%d)", path, static_cast<int>(result));
    return false;
  }
  result = cgltf_validate(data);
  if (result != cgltf_result_success) {
    ALOGE("glTF: '%s' is invalid (%d)", path, static_cast<int>(result));
    return false;
  }
  for (size_t i = 0; i < data->extensions_required_count; ++i) {
    if (!isRequiredExtensionSupported(data->extensions_required[i])) {
      ALOGE("glTF: '%s' requires the unsupported extension %s", path,
            data->extensions_required[i]);
      return false;
    }
  }

  GltfImportResult imported;
  TextureResolver textures(*data, import_options, getDirectory(path));
  imported.materials.reserve(data->materials_count);
  for (size_t i = 0; i < data->materials_count; ++i) {
    imported.materials.push_back(convertMaterial(data->materials[i], i, textures));
  }
  imported.meshes.reserve(data->meshes_count);
  for (size_t i = 0; i < data->meshes_count; ++i) {
    imported.meshes.push_back(
        convertMesh(*data, data->meshes[i], i, import_options.meshBuild));
  }
  convertNodes(*data, imported.meshes, imported.nodes);
  out = std::move(imported);
  return true;
}

}  // namespace

bool importGltf(const char* path, const GltfImportOptions& options,
                GltfImportResult& out) {
  const cgltf_options cgltf_opts{};
  cgltf_data* data = nullptr;
  const cgltf_result result = cgltf_parse_file(&cgltf_opts, path, &data);
  if (result != cgltf_result_success) {
    ALOGE("glTF: cannot parse '%s' (%d)", path, static_cast<int>(result));
    return false;
  }
  const CgltfDataPtr owner(data);
  return importParsed(cgltf_opts, data, path, options, out);
}

bool importGltfFromMemory(Span<const uint8> bytes, const char* path,
                          const GltfImportOptions& options, GltfImportResult& out) {
  if (!path) {
    path = "";
  }
  const cgltf_options cgltf_opts{};
  cgltf_data* data = nullptr;
  const cgltf_result result = cgltf_parse(&cgltf_opts, bytes.data(), bytes.size(), &data);
  if (result != cgltf_result_success) {
    ALOGE("glTF: cannot parse '%s' (%d)", path, static_cast<int>(result));
    return false;
  }
  const CgltfDataPtr owner(data);
  return importParsed(cgltf_opts, data, path, options, out);
}

}  // namespace amaryl
