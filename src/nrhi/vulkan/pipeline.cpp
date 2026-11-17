#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace vk {
uint32 VulkanPipelineBuilder::num_pipelines_created_ = 0u;

VulkanPipelineBuilder::VulkanPipelineBuilder()
    : vertex_input_state_({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          .vertexBindingDescriptionCount = 0,
          .pVertexBindingDescriptions = nullptr,
          .vertexAttributeDescriptionCount = 0,
          .pVertexAttributeDescriptions = nullptr,
      }),
      input_assembly_state_({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .flags = 0,
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .primitiveRestartEnable = VK_FALSE,
      }),
      rasterization_state_({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .flags = 0,
          .depthClampEnable = VK_FALSE,
          .rasterizerDiscardEnable = VK_FALSE,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .cullMode = VK_CULL_MODE_NONE,
          .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
          .depthBiasEnable = VK_FALSE,
          .depthBiasConstantFactor = 0.0f,
          .depthBiasClamp = 0.0f,
          .depthBiasSlopeFactor = 0.0f,
          .lineWidth = 1.0f,
      }),
      multisample_state_({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
          .sampleShadingEnable = VK_FALSE,
          .minSampleShading = 0.0f,
          .pSampleMask = nullptr,
          .alphaToCoverageEnable = VK_FALSE,
          .alphaToOneEnable = VK_FALSE,
      }),
      depth_stencil_state_({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .depthTestEnable = VK_FALSE,
          .depthWriteEnable = VK_FALSE,
          .depthCompareOp = VK_COMPARE_OP_LESS,
          .depthBoundsTestEnable = VK_FALSE,
          .stencilTestEnable = VK_FALSE,
          .front =
              {
                  .failOp = VK_STENCIL_OP_KEEP,
                  .passOp = VK_STENCIL_OP_KEEP,
                  .depthFailOp = VK_STENCIL_OP_KEEP,
                  .compareOp = VK_COMPARE_OP_NEVER,
                  .compareMask = 0,
                  .writeMask = 0,
                  .reference = 0,
              },
          .back =
              {
                  .failOp = VK_STENCIL_OP_KEEP,
                  .passOp = VK_STENCIL_OP_KEEP,
                  .depthFailOp = VK_STENCIL_OP_KEEP,
                  .compareOp = VK_COMPARE_OP_NEVER,
                  .compareMask = 0,
                  .writeMask = 0,
                  .reference = 0,
              },
          .minDepthBounds = 0.0f,
          .maxDepthBounds = 1.0f,
      }),
      tessellation_state_({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
          .flags = 0,
          .patchControlPoints = 0,
      }) {}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicState(VkDynamicState state,
                                                           bool enable) {
  if (!enable) {
    return *this;
  }

  ASSERT(num_dynamic_states_ < MAX_DYNAMIC_STATES);
  dynamic_states_[num_dynamic_states_++] = state;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::createFlags(VkPipelineCreateFlags flags,
                                                          bool enable) {
  if (enable) {
    flags_ |= flags;
  }
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::primitiveTopology(
    VkPrimitiveTopology topology) {
  input_assembly_state_.topology = topology;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::rasterizationSamples(
    VkSampleCountFlagBits samples, float min_sample_shading) {
  multisample_state_.rasterizationSamples = samples;
  multisample_state_.sampleShadingEnable = min_sample_shading > 0 ? VK_TRUE : VK_FALSE;
  multisample_state_.minSampleShading = min_sample_shading;

  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::alphaToCoverage(bool enable) {
  multisample_state_.alphaToCoverageEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStage(
    VkPipelineShaderStageCreateInfo stage) {
  if (stage.pNext) {
    ASSERT(num_shader_stages_ < ARRAY_COUNT(shader_stages_));
    shader_stages_[num_shader_stages_++] = stage;
  }

  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilStateOps(
    VkStencilFaceFlags face_mask, VkStencilOp fail_op, VkStencilOp pass_op,
    VkStencilOp depth_fail_op, VkCompareOp compare_op) {
  depth_stencil_state_.stencilTestEnable =
      depth_stencil_state_.stencilTestEnable == VK_TRUE ||  //
              fail_op != VK_STENCIL_OP_KEEP ||              //
              pass_op != VK_STENCIL_OP_KEEP ||              //
              depth_fail_op != VK_STENCIL_OP_KEEP ||        //
              compare_op != VK_COMPARE_OP_ALWAYS
          ? VK_TRUE
          : VK_FALSE;

  if (face_mask & VK_STENCIL_FACE_FRONT_BIT) {
    VkStencilOpState& s = depth_stencil_state_.front;
    s.failOp = fail_op;
    s.passOp = pass_op;
    s.depthFailOp = depth_fail_op;
    s.compareOp = compare_op;
  }

  if (face_mask & VK_STENCIL_FACE_BACK_BIT) {
    VkStencilOpState& s = depth_stencil_state_.back;
    s.failOp = fail_op;
    s.passOp = pass_op;
    s.depthFailOp = depth_fail_op;
    s.compareOp = compare_op;
  }

  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilMasks(VkStencilFaceFlags face_mask,
                                                           uint32 compare_mask,
                                                           uint32 write_mask,
                                                           uint32 ref) {
  if (face_mask & VK_STENCIL_FACE_FRONT_BIT) {
    VkStencilOpState& s = depth_stencil_state_.front;
    s.compareMask = compare_mask;
    s.writeMask = write_mask;
    s.reference = ref;
  }

  if (face_mask & VK_STENCIL_FACE_BACK_BIT) {
    VkStencilOpState& s = depth_stencil_state_.back;
    s.compareMask = compare_mask;
    s.writeMask = write_mask;
    s.reference = ref;
  }

  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::cullMode(VkCullModeFlags mode) {
  rasterization_state_.cullMode = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::frontFace(VkFrontFace mode) {
  rasterization_state_.frontFace = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::polygonMode(VkPolygonMode mode) {
  rasterization_state_.polygonMode = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::vertexInputState(
    const VkPipelineVertexInputStateCreateInfo& state) {
  vertex_input_state_ = state;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::viewMask(uint32 mask) {
  view_mask_ = mask;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::colorAttachments(
    const VkPipelineColorBlendAttachmentState* states, const VkFormat* formats,
    uint32 num_color_attachments) {
  ASSERT(states != nullptr);
  ASSERT(formats != nullptr);
  ASSERT(num_color_attachments <= ARRAY_COUNT(color_blend_attachment_states_));
  ASSERT(num_color_attachments <= ARRAY_COUNT(color_attachment_formats_));

  for (uint32 i = 0; i < num_color_attachments; i++) {
    color_blend_attachment_states_[i] = states[i];
    color_attachment_formats_[i] = formats[i];
  }

  num_color_attachments_ = num_color_attachments;

  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthAttachmentFormat(VkFormat format) {
  depth_attachment_format_ = format;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilAttachmentFormat(VkFormat format) {
  stencil_attachment_format_ = format;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::patchControlPoints(uint32 num_points) {
  ASSERT(false, TEXT("no tessellation"));

  return *this;
}

VkResult VulkanPipelineBuilder::build(VkDevice device, VkPipelineCache pipeline_cache,
                                      VkPipelineLayout pipeline_layout,
                                      VkPipeline* out_pipeline,
                                      const char* debug_name) noexcept {
  const VkPipelineDynamicStateCreateInfo dynamic_state_ci{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = num_dynamic_states_,
      .pDynamicStates = dynamic_states_,
  };

  // viewport and scissor can be NULL if the viewport state is dynamic
  // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPipelineViewportStateCreateInfo.html
  const VkPipelineViewportStateCreateInfo viewport_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };

  const VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = num_color_attachments_,
      .pAttachments = color_blend_attachment_states_,
  };
  const VkPipelineRenderingCreateInfo rendering_ci{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .viewMask = view_mask_,
      .colorAttachmentCount = num_color_attachments_,
      .pColorAttachmentFormats = color_attachment_formats_,
      .depthAttachmentFormat = depth_attachment_format_,
      .stencilAttachmentFormat = stencil_attachment_format_,
  };
  const VkGraphicsPipelineCreateInfo pipeline_ci{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &rendering_ci,
      .flags = flags_,
      .stageCount = num_shader_stages_,
      .pStages = shader_stages_,
      .pVertexInputState = &vertex_input_state_,
      .pInputAssemblyState = &input_assembly_state_,
      .pTessellationState = &tessellation_state_,
      .pViewportState = &viewport_state_ci,
      .pRasterizationState = &rasterization_state_,
      .pMultisampleState = &multisample_state_,
      .pDepthStencilState = &depth_stencil_state_,
      .pColorBlendState = &color_blend_state_ci,
      .pDynamicState = &dynamic_state_ci,
      .layout = pipeline_layout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  const VkResult result = vkCreateGraphicsPipelines(device, pipeline_cache, 1,
                                                    &pipeline_ci, nullptr, out_pipeline);
  VK_CHECK(result);

  if (result != VK_SUCCESS) {
    return result;
  }

  num_pipelines_created_++;
  return setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE, (uint64)*out_pipeline,
                            debug_name);
}

}  // namespace vk

}  // namespace amaryl
