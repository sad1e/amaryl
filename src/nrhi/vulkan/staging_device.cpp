#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl::vk {
StagingDevice::StagingDevice(Context& context) : ctx_{context} {
  const VkDeviceSize max_memory_allocation_size =
      ctx_.physical_device_props11_.maxMemoryAllocationSize;

  ASSERT(ctx_.config_.maxStagingBufferSize <= max_memory_allocation_size,
         TEXT("maxStagingBufferSize should smaller than "
              "VkPhysicalDeviceVulkan11Properties.maxMemoryAllocationSize"));

  max_buffer_size_ =
      std::min(ctx_.config_.maxStagingBufferSize, max_memory_allocation_size);
  min_buffer_size_ = std::min(min_buffer_size_, max_buffer_size_);
}

void StagingDevice::bufferSubData(VulkanBuffer& buffer, size_t offset, size_t size,
                                  const void* data) {
  if (buffer.isMapped()) {
    buffer.bufferSubData(ctx_, offset, size, data);
    return;
  }

  const VkBuffer dst_vk_buffer = buffer.vk_handle_;

  const size_t original_dst_offst = offset;
  const size_t original_size = size;

  while (size) {
    MemoryRegionDesc mrd = getNextFreeOffset(size);

    const VkDeviceSize chunk_size = std::min<VkDeviceSize>(size, mrd.size);
    const bool is_last = chunk_size == size;

    // fetchthe staging buffer after `getNextFreeOffset` so the pointer is valid even if
    // it just grew
    VulkanBuffer* staging_buffer = ctx_.buffer_pool_.get(handle_);
    ASSERT(staging_buffer != nullptr, TEXT("Staging buffer is nullptr."));

    // copy data into staging buffer
    staging_buffer->bufferSubData(ctx_, mrd.offset, mrd.size, data);

    const VkBufferCopy copy_info{
        .srcOffset = mrd.offset,
        .dstOffset = offset,
        .size = chunk_size,
    };

    VulkanImmediateCommands::CommandBufferWrapper& cmd = ctx_.immediate_->acquire();

    vkCmdCopyBuffer(cmd.cmdBuf, staging_buffer->vk_handle_, dst_vk_buffer, 1, &copy_info);

    // barrier covering the whole range, notice the offset and size.
    if (is_last) {
      const VkBufferMemoryBarrier2 barrier{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = dst_vk_buffer,
          .offset = original_dst_offst,
          .size = original_size,
      };
      const VkDependencyInfo dep_info{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .bufferMemoryBarrierCount = 1,
          .pBufferMemoryBarriers = &barrier,
      };
      vkCmdPipelineBarrier2(cmd.cmdBuf, &dep_info);
    }

    mrd.handle = ctx_.immediate_->submit(cmd);
    insertRegion(mrd);

    size -= chunk_size;
    data = (uint8*)data + chunk_size;
    offset += chunk_size;
  }
}

void StagingDevice::imageData2D(VulkanImage& image, const VkRect2D& region,
                                uint32 base_mip_level, uint32 num_mip_levels,
                                uint32 base_layer, uint32 num_layers, VkFormat format,
                                const void* data, uint32 buffer_row_length) {
  // the layout after upload.
  // Only sampled images or read as input attachments are allowed to use
  // `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
  const VkImageLayout layout_after_upload =
      (image.usage_flags_ &
       (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
          ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
          : VK_IMAGE_LAYOUT_GENERAL;

  ASSERT(num_mip_levels < MAX_MIP_LEVELS,
         TEXT("Mip levels should be less than MAX_MIP_LEVELS"));

  const EFormat eformat = toEFormat(format);
  const uint32 width = image.extent_.width >> base_mip_level;
  const uint32 height = image.extent_.height >> base_mip_level;
  // Which means the image start from (0, 0) and has the same width/height with
  // region.extent.
  const bool covers_full_image = !region.offset.x && !region.offset.y &&  //
                                 region.extent.width == width &&
                                 region.extent.height == height;

  ASSERT(covers_full_image || image.layout_ != VK_IMAGE_LAYOUT_UNDEFINED,
         TEXT("imageData2D: the value should be full image or image original layout "
              "should not "
              "be VK_IMAGE_LAYOUT_UNDEFINED"));

  if (num_mip_levels > 1 || num_layers > 1) {
    ASSERT(!buffer_row_length);
    ASSERT(covers_full_image,
           TEXT("Uploading mip-levels with an image region that is smaller "
                "than then base mip-level is not supported"));
  }

  // * The fast uploading path is copying from host memory ( no staging buffer or submit
  // ), using `VK_EXT_host_image_copy`. It must be the **first uploading**, because if
  // re-uploading the resource, the GPU may still sampling, which would lead to a
  // resource race situation.
  // * fast upload conditions:
  // 1. covers full image and layout is undefined.
  bool enable_first_upload =
      covers_full_image && image.layout_ == VK_IMAGE_LAYOUT_UNDEFINED;
  // 2. has only one aspect (depth&stencil not support)
  const VkImageAspectFlags host_copy_aspect = image.getImageAspectFlags();
  enable_first_upload =
      enable_first_upload && ((host_copy_aspect & (host_copy_aspect - 1)) == 0);
  // 3. single plane, exclude multi-plane YUV
  enable_first_upload = enable_first_upload && getNumImagePlanes(image.format_) == 1;
  // 4. `VK_IMAGE_USAGE_HOST_TRANSFER_BIT` should be set
  enable_first_upload =
      enable_first_upload && image.usage_flags_ & VK_IMAGE_USAGE_HOST_TRANSFER_BIT;

  if (enable_first_upload) {
    std::vector<VkMemoryToImageCopy> regions;
    regions.reserve(num_mip_levels * num_layers);

    uint64 src_offset = 0u;
    for (uint32 mip_level = 0; mip_level != num_mip_levels; ++mip_level) {
      for (uint32 layer = 0; layer != num_layers; ++layer) {
        const uint32 current_mip_level = base_mip_level + mip_level;
        const uint32 current_layer = base_layer + layer;

        ASSERT(current_mip_level < image.num_levels_);
        ASSERT(current_layer < image.num_layers_);

        regions.push_back(VkMemoryToImageCopy{
            .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY,
            .pHostPointer = (const uint8*)data + src_offset,
            .memoryRowLength = buffer_row_length,
            .memoryImageHeight = 0,
            .imageSubresource =
                VkImageSubresourceLayers{host_copy_aspect, current_mip_level,
                                         current_layer, 1},
            .imageOffset =
                {
                    .x = region.offset.x >> mip_level,
                    .y = region.offset.y >> mip_level,
                    .z = 0,
                },
            .imageExtent =
                {
                    .width = std::max(1u, region.extent.width >> mip_level),
                    .height = std::max(1u, region.extent.height >> mip_level),
                    .depth = 1u,
                },
        });

        src_offset += getTextureBytesPerLayer(
            buffer_row_length ? buffer_row_length : region.extent.width,
            region.extent.height, eformat, mip_level);
      }
    }

    const VkHostImageLayoutTransitionInfo host_image_layout_transition_info{
        .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO,
        .image = image.vk_handle_,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = layout_after_upload,
        .subresourceRange = {host_copy_aspect, base_mip_level, num_mip_levels, base_layer,
                             num_layers},
    };

    VK_CHECK(
        vkTransitionImageLayout(ctx_.device_, 1, &host_image_layout_transition_info));

    const VkCopyMemoryToImageInfo copy_memory_to_image{
        .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO,
        .dstImage = image.vk_handle_,
        .dstImageLayout = layout_after_upload,
        .regionCount = (uint32)regions.size(),
        .pRegions = regions.data(),
    };
    VK_CHECK(vkCopyMemoryToImage(ctx_.device_, &copy_memory_to_image));

    image.layout_ = layout_after_upload;
    return;
  }

  // find the storage size for all mip-levels being uploaded
  uint32 layer_storage_size = 0u;
  for (uint32 i = 0u; i < num_mip_levels; ++i) {
    const uint32 mip_size = getTextureBytesPerLayer(
        buffer_row_length ? buffer_row_length : region.extent.width, region.extent.height,
        eformat, i);

    layer_storage_size += mip_size;
  }

  const uint64 storage_size = (uint64)layer_storage_size * num_layers;
  ensureStagingBufferSize(storage_size);

  ASSERT(storage_size < staging_buffer_size_);

  MemoryRegionDesc desc = getNextFreeOffset(storage_size);
  // not support for copying image in multiple smaller chunck sizes, if we get smaller
  // buffer size than storage_size, we will wait for GPU idle and get bigger chunk.
  if (desc.size < storage_size) {
    waitAndReset();
    desc = getNextFreeOffset(storage_size);
  }

  ASSERT(desc.size >= storage_size);

  VulkanImmediateCommands::CommandBufferWrapper& wrapper = ctx_.immediate_->acquire();
  VulkanBuffer* staging_buffer = ctx_.buffer_pool_.get(handle_);

  staging_buffer->bufferSubData(ctx_, desc.offset, storage_size, data);

  uint32 offset = 0u;

  const uint32 num_planes = getNumImagePlanes(image.format_);
  if (num_planes > 1) {
    // multiple plane is more strictly.
    ASSERT(base_layer == 0 && base_mip_level == 0);
    ASSERT(num_layers == 1 && num_mip_levels == 1);
    ASSERT(region.offset.x == 0 && region.offset.y == 0);
    ASSERT(image.type_ == VK_IMAGE_TYPE_2D);
    ASSERT(image.extent_.width == region.extent.width &&
           image.extent_.height == region.extent.height);
  }

  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
  if (num_planes == 2) {
    aspect_flags = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }
  if (num_planes == 3) {
    aspect_flags = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT |
                   VK_IMAGE_ASPECT_PLANE_2_BIT;
  }

  for (uint32 mip_level = 0; mip_level < num_mip_levels; ++mip_level) {
    for (uint32 layer = 0; layer < num_layers; ++layer) {
      const uint32 current_mip_level = base_mip_level + mip_level;
      const uint32 current_layer = base_layer + layer;

      ASSERT(current_mip_level < image.num_levels_);
      ASSERT(mip_level < image.num_levels_);
      ASSERT(current_layer < image.num_layers_);

      // 1. Transition initial image layout into `TRANSFER_DST_OPTIMAL`
      imageMemoryBarrier2(
          wrapper.cmdBuf,    //
          image.vk_handle_,  //
          StageAccess{.stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                      .access = VK_ACCESS_2_NONE},
          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
          covers_full_image ? VK_IMAGE_LAYOUT_UNDEFINED : image.layout_,  //
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                           //
          VkImageSubresourceRange{aspect_flags, current_mip_level, 1, current_layer, 1});
      // 2. Copy the pixel data from the staging buffer into the image
      uint32 plane_offset = 0u;
      for (uint32 plane = 0u; plane < num_planes; ++plane) {
        const VkExtent2D extent = getImagePlaneExtent(
            {
                .width = std::max(1u, region.extent.width >> mip_level),
                .height = std::max(1u, region.extent.height >> mip_level),
            },
            toEFormat(format), plane);
        const VkRect2D plane_region{
            .offset = {.x = region.offset.x >> mip_level,
                       .y = region.offset.y >> mip_level},
            .extent = extent,
        };
        const VkBufferImageCopy2 buffer_image_copy2{
            .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .bufferOffset = desc.offset + offset + plane_offset,
            .bufferRowLength = buffer_row_length,
            .bufferImageHeight = 0,
            .imageSubresource =
                VkImageSubresourceLayers{
                    num_planes > 1 ? VK_IMAGE_ASPECT_PLANE_0_BIT << plane : aspect_flags,
                    current_mip_level, current_layer, 1},
            .imageOffset = {.x = region.offset.x, .y = region.offset.y, .z = 0},
            .imageExtent = {.width = region.extent.width,
                            .height = region.extent.height,
                            .depth = 1u},
        };
        const VkCopyBufferToImageInfo2 copy_buffer_to_image_info2{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .srcBuffer = staging_buffer->vk_handle_,
            .dstImage = image.vk_handle_,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &buffer_image_copy2,
        };

        vkCmdCopyBufferToImage2(wrapper.cmdBuf, &copy_buffer_to_image_info2);

        plane_offset += getTextureBytesPerLayer(region.extent.width, region.extent.height,
                                                toEFormat(format), plane);
      }

      // 3. Transition `TRANSFER_DST_OPTIMAL` into the layout the image can actually use
      imageMemoryBarrier2(
          wrapper.cmdBuf,    //
          image.vk_handle_,  //
          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
          StageAccess{
              .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
              .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  //
          layout_after_upload,                   //
          VkImageSubresourceRange{aspect_flags, current_mip_level, 1, current_layer, 1});

      offset += getTextureBytesPerLayer(region.extent.width, region.extent.height,
                                        eformat, current_mip_level);
    }
  }

  image.layout_ = layout_after_upload;
  desc.handle = ctx_.immediate_->submit(wrapper);

  insertRegion(desc);
}

void StagingDevice::imageData3D(VulkanImage& image, const VkOffset3D& offset,
                                const VkExtent3D& extent, VkFormat format,
                                const void* data) {
  ASSERT(image.num_levels_ == 1,
         TEXT("Can handle only 3D images with exactly 1 mip-level"));

  const VkImageLayout layout_after_upload =
      (image.usage_flags_ & VK_IMAGE_USAGE_SAMPLED_BIT)
          ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
          : VK_IMAGE_LAYOUT_GENERAL;

  // image3D has multiple slices
  const uint64 slice_bytes64 =
      (uint64)extent.width * extent.height * getBytesPerPixel(format);
  ASSERT(slice_bytes64 <= UINT32_MAX, TEXT("Single depth slice exceeds 4GB"));
  const uint32 slice_bytes = (uint32)slice_bytes64;
  // The batch size is `maxStagingBufferSize`
  const uint32 max_slices_per_batch =
      std::max(1u, (uint32)(ctx_.config_.maxStagingBufferSize / slice_bytes));

  ensureStagingBufferSize((VkDeviceSize)slice_bytes * max_slices_per_batch);

  const uint8* ptr_src = static_cast<const uint8*>(data);
  uint32 remaining_slices = extent.depth;
  uint32 current_z = offset.z;

  VulkanBuffer* staging_buffer = ctx_.buffer_pool_.get(handle_);

  while (remaining_slices) {
    const uint32 batch_slices = std::min(remaining_slices, max_slices_per_batch);
    const VkDeviceSize batch_bytes = (VkDeviceSize)batch_slices * slice_bytes;

    MemoryRegionDesc desc = getNextFreeOffset(batch_bytes);
    if (desc.size < batch_bytes) {
      waitAndReset();
      desc = getNextFreeOffset(batch_bytes);
    }
    ASSERT(desc.size > batch_bytes);

    staging_buffer->bufferSubData(ctx_, desc.offset, batch_bytes, ptr_src);

    auto& wrapper = ctx_.immediate_->acquire();

    // 1. Transition the whole image UNDEFINED -> TRANSFER_DST_OPTIMAL
    if (remaining_slices == extent.depth) {
      imageMemoryBarrier2(wrapper.cmdBuf, image.vk_handle_,  //
                          StageAccess{.stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                      .access = VK_ACCESS_2_NONE},
                          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    }

    const VkBufferImageCopy2 buffer_image_copy2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .bufferOffset = desc.offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageOffset = {offset.x, offset.y, (int32)current_z},
        .imageExtent = {extent.width, extent.height, batch_slices},
    };
    const VkCopyBufferToImageInfo2 copy_buffer_to_image_info2{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
        .srcBuffer = staging_buffer->vk_handle_,
        .dstImage = image.vk_handle_,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &buffer_image_copy2,
    };

    vkCmdCopyBufferToImage2(wrapper.cmdBuf, &copy_buffer_to_image_info2);

    // 2. Transition the whole image out of TRANSFER_DST_OPTIMAL
    if (remaining_slices == batch_slices) {
      imageMemoryBarrier2(
          wrapper.cmdBuf, image.vk_handle_,  //
          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
          StageAccess{
              .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
              .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
          },
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout_after_upload,
          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    }

    desc.handle = ctx_.immediate_->submit(wrapper);
    insertRegion(desc);

    ptr_src += batch_bytes;
    current_z += batch_slices;
    remaining_slices -= batch_slices;
  }  // while

  image.layout_ = layout_after_upload;
}

void StagingDevice::getImageData(VulkanImage& image, const VkOffset3D& offset,
                                 const VkExtent3D& extent, VkImageSubresourceRange range,
                                 VkFormat format, void* out_data) {
  ASSERT(image.layout_ != VK_IMAGE_LAYOUT_UNDEFINED);
  ASSERT(range.layerCount == 1);

  const uint64 storage_size =
      (uint64)extent.width * extent.height * extent.depth * getBytesPerPixel(format);

  ASSERT(storage_size <= staging_buffer_size_,
         TEXT("The image storage size should be less than staging buffer size"));

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(storage_size);

  // No support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storage_size, we will wait for GPU idle and get
  // a bigger chunk.
  if (desc.size < storage_size) {
    waitAndReset();
    desc = getNextFreeOffset(storage_size);
  }

  ASSERT(desc.size >= storage_size);

  VulkanBuffer* staging_buffer = ctx_.buffer_pool_.get(handle_);

  VulkanImmediateCommands::CommandBufferWrapper& wrapper1 = ctx_.immediate_->acquire();

  // 1. Transition to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  imageMemoryBarrier2(
      wrapper1.cmdBuf, image.vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_READ_BIT},  //
      image.layout_,                                         //
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                  //
      range);

  // 2.  Copy the pixel data from the image into the staging buffer
  const VkBufferImageCopy2 copy = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
      .bufferOffset = desc.offset,
      .bufferRowLength = 0,
      .bufferImageHeight = extent.height,
      .imageSubresource =
          VkImageSubresourceLayers{
              .aspectMask = range.aspectMask,
              .mipLevel = range.baseMipLevel,
              .baseArrayLayer = range.baseArrayLayer,
              .layerCount = range.layerCount,
          },
      .imageOffset = offset,
      .imageExtent = extent,
  };
  const VkCopyImageToBufferInfo2 copyInfo = {
      .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
      .srcImage = image.vk_handle_,
      .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .dstBuffer = staging_buffer->vk_handle_,
      .regionCount = 1,
      .pRegions = &copy,
  };
  vkCmdCopyImageToBuffer2(wrapper1.cmdBuf, &copyInfo);

  desc.handle = ctx_.immediate_->submit(wrapper1);
  insertRegion(desc);

  waitAndReset();

  if (!staging_buffer->is_coherent_memory_) {
    staging_buffer->invalidateMappedMemory(ctx_, desc.offset, desc.size);
  }

  // 3. Copy data from staging buffer into data
  memcpy(out_data, staging_buffer->getMappedPtr() + desc.offset, storage_size);

  // 4. Transition back to the initial image layout
  VulkanImmediateCommands::CommandBufferWrapper& wrapper2 = ctx_.immediate_->acquire();

  imageMemoryBarrier2(
      wrapper2.cmdBuf, image.vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_READ_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.layout_, range);

  ctx_.immediate_->wait(ctx_.immediate_->submit(wrapper2));
}

StagingDevice::MemoryRegionDesc StagingDevice::getNextFreeOffset(VkDeviceSize size) {
  const VkDeviceSize requested_aligned_size =
      Align<VkDeviceSize>(size, kStagingBufferAlignment);
  ensureStagingBufferSize(requested_aligned_size);

  ASSERT(!regions_.empty(), TEXT("The default regions should not be empty."));

  if (regions_.size() > 1) {
    size_t write = 0;
    bool cur_ready = ctx_.immediate_->isReady(regions_[0].handle);
    for (size_t read = 1; read < regions_.size(); ++read) {
      MemoryRegionDesc& cur_write_region = regions_[write];

      const MemoryRegionDesc& next_region = regions_[read];
      const bool next_ready = ctx_.immediate_->isReady(next_region.handle);

      if (cur_ready && next_ready &&
          cur_write_region.offset + cur_write_region.size == next_region.offset) {
        cur_write_region.size += next_region.size;
        cur_write_region.handle = SubmitHandle();
      } else {
        regions_[++write] = next_region;
        cur_ready = next_ready;
      }
    }
    regions_.resize(write + 1);
  }

  // if we can't find an available region that is big enough to store
  // requested_aligned_size, return whatever we could find, which will be stored in
  // best_next_it
  auto best_next_it = regions_.end();

  for (auto it = regions_.begin(); it != regions_.end(); ++it) {
    // Check the region is free.
    if (ctx_.immediate_->isReady(it->handle)) {
      // Check the region size is larger enough.
      if (it->size >= requested_aligned_size) {
        const VkDeviceSize unused_size = it->size - requested_aligned_size;
        const VkDeviceSize unused_offset = it->offset - requested_aligned_size;

        // Return this region and add the remaining unused size to the `regions_`.
        SCOPE_EXIT {
          regions_.erase(it);
          if (unused_size > 0) {
            insertRegion({unused_offset, unused_size, SubmitHandle()});
          }
        };

        return {it->offset, requested_aligned_size, SubmitHandle()};
      }

      // Cache the largest available region that isn't as big as the one we'are looking
      // for
      if (best_next_it == regions_.end() || it->size > best_next_it->size) {
        best_next_it = it;
      }
    }
  }

  if (best_next_it != regions_.end() && ctx_.immediate_->isReady(best_next_it->handle)) {
    SCOPE_EXIT { regions_.erase(best_next_it); };

    return {best_next_it->offset, best_next_it->size, SubmitHandle()};
  }

  // Still not found a free region to use, wait the entire staging buffer to become
  // free.
  waitAndReset();

  // `waitAndReset()` will add a region that spans the entire buffer.
  regions_.clear();

  const uint64 unused_size = staging_buffer_size_ - requested_aligned_size
                                 ? staging_buffer_size_ - requested_aligned_size
                                 : 0u;
  if (unused_size) {
    const uint64 unused_offset = staging_buffer_size_ - unused_size;
    regions_.insert(regions_.begin(), {unused_offset, unused_size, SubmitHandle()});
  }

  return {
      .offset = 0u,
      .size = staging_buffer_size_ - unused_size,
      .handle = SubmitHandle(),
  };
}

void StagingDevice::ensureStagingBufferSize(VkDeviceSize size_needed) {
  const uint64 max_staging_buffer_size = ctx_.config_.maxStagingBufferSize;
  const VkDeviceSize aligned_size = std::max(
      Align<VkDeviceSize>(size_needed, kStagingBufferAlignment), min_buffer_size_);

  size_needed =
      aligned_size < max_staging_buffer_size ? aligned_size : max_staging_buffer_size;

  if (!handle_.empty()) {
    const bool has_enough_size = size_needed < staging_buffer_size_;
    const bool is_max_size = staging_buffer_size_ == max_staging_buffer_size;

    if (has_enough_size || is_max_size) {
      return;
    }
  }

  // NOTE: The GPU will no longer use
  waitAndReset();

  handle_ = nullptr;

  // if the combined size of the new staging buffer and the existing one is larger than
  // the limit imposed by some architectures on buffers that are device and host
  // visible, we need to wait for the current buffer to be destroyed before we can
  // allocate a new one.
  //
  // NOTE: Wait the driver destroy handles.
  if ((size_needed + staging_buffer_size_) > max_staging_buffer_size) {
    ctx_.waitDeferredTasks();
  }

  staging_buffer_size_ = size_needed;

  char debug_name[MAX_DEBUG_STRING_LENGTH]{0};
  (void)snprintf(debug_name, DEBUG_STRING_SIZE, "Buffer: staging buffer: %u",
                 staging_buffer_count_++);

  handle_ = {&ctx_,
             ctx_.createBuffer(
                 staging_buffer_size_,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, debug_name)};

  ASSERT(!handle_.empty(), TEXT("Staging buffer create failed."));

  regions_.clear();
  regions_.push_back({0, staging_buffer_size_, SubmitHandle()});
}

void StagingDevice::insertRegion(const MemoryRegionDesc& region) {
  auto it = regions_.begin();
  while (it != regions_.end() && it->offset < region.offset) {
    ++it;
  }
  regions_.insert(it, region);
}

void StagingDevice::waitAndReset() {
  for (const MemoryRegionDesc& mrd : regions_) {
    ctx_.immediate_->wait(mrd.handle);
  }

  regions_.clear();
  regions_.push_back({
      .offset = 0u,
      .size = staging_buffer_size_,
      .handle = SubmitHandle(),
  });
}

}  // namespace amaryl::vk
