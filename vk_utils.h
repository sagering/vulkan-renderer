#ifndef VK_UTILS_H_
#define VK_UTILS_H_

#include <initializer_list>
#include <memory> // memcpy
#include <vulkan\vulkan.h>

#include "vk_init.h"

#define BREAK                                                                  \
  do {                                                                         \
    *((int*)(0)) = 0;                                                          \
  } while (0);
#define ASSERT_VK_SUCCESS(x)                                                   \
  if (x != VK_SUCCESS) {                                                       \
    BREAK                                                                      \
  }
#define ASSERT_VK_VALID_HANDLE(x)                                              \
  if (x == VK_NULL_HANDLE) {                                                   \
    BREAK                                                                      \
  }
#define ASSERT_TRUE(x)                                                         \
  if (x != true) {                                                             \
    BREAK                                                                      \
  }

inline VkPipeline
vkuCreateGraphicsPipeline(
  VkDevice device,
  std::initializer_list<VkPipelineShaderStageCreateInfo> stages,
  VkPipelineVertexInputStateCreateInfo vertexInputState,
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState,
  VkPipelineTessellationStateCreateInfo tessellationState,
  VkPipelineViewportStateCreateInfo viewportState,
  VkPipelineRasterizationStateCreateInfo rasterizationState,
  VkPipelineMultisampleStateCreateInfo multisampleState,
  VkPipelineDepthStencilStateCreateInfo depthStencilState,
  VkPipelineColorBlendStateCreateInfo colorBlendState,
  VkPipelineDynamicStateCreateInfo dynamicState,
  VkPipelineLayout layout,
  VkRenderPass renderPass,
  uint32_t subpass,
  VkPipeline basePipelineHandle,
  int32_t basePipelineIndex)
{
  auto graphicsPipelineCreateInfo =
    vkiGraphicsPipelineCreateInfo(static_cast<uint32_t>(stages.size()),
                                  stages.begin(),
                                  &vertexInputState,
                                  &inputAssemblyState,
                                  &tessellationState,
                                  &viewportState,
                                  &rasterizationState,
                                  &multisampleState,
                                  &depthStencilState,
                                  &colorBlendState,
                                  &dynamicState,
                                  layout,
                                  renderPass,
                                  subpass,
                                  basePipelineHandle,
                                  basePipelineIndex);

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult result = vkCreateGraphicsPipelines(
    device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline);
  return pipeline;
}

inline VkShaderModule
vkuCreateShaderModule(VkDevice device,
                      size_t codeSize,
                      const uint32_t* pCode,
                      const VkAllocationCallbacks* pAllocator = nullptr)
{
  auto info = vkiShaderModuleCreateInfo(codeSize, pCode);
  VkShaderModule handle = VK_NULL_HANDLE;
  vkCreateShaderModule(device, &info, pAllocator, &handle);
  return handle;
}

inline VkBuffer
vkuCreateBuffer(VkDevice device,
                VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkSharingMode sharingMode,
                std::initializer_list<uint32_t> queueFamilyIndices,
                const VkAllocationCallbacks* pAllocator = nullptr)
{
  auto info =
    vkiBufferCreateInfo(size,
                        usage,
                        sharingMode,
                        static_cast<uint32_t>(queueFamilyIndices.size()),
                        queueFamilyIndices.begin());
  VkBuffer handle = VK_NULL_HANDLE;
  vkCreateBuffer(device, &info, pAllocator, &handle);
  return handle;
}

inline VkFence
vkuCreateFence(VkDevice device)
{
  auto info = vkiFenceCreateInfo();
  VkFence handle = VK_NULL_HANDLE;
  vkCreateFence(device, &info, nullptr, &handle);
  return handle;
}

inline VkAccessFlags
vkuGetImageAccessFlags(VkImageLayout imageLayout);
inline VkPipelineStageFlags
vkuGetImageStageFlags(VkImageLayout imageLayout);

inline void
vkuTransitionLayout(VkCommandBuffer commandBuffer,
                    VkImage image,
                    VkImageSubresourceRange range,
                    VkImageLayout oldLayout,
                    VkImageLayout newLayout)
{

  if (newLayout == oldLayout)
    return;

  VkImageMemoryBarrier imageMemoryBarrier =
    vkiImageMemoryBarrier(vkuGetImageAccessFlags(oldLayout),
                          vkuGetImageAccessFlags(newLayout),
                          oldLayout,
                          newLayout,
                          VK_QUEUE_FAMILY_IGNORED,
                          VK_QUEUE_FAMILY_IGNORED,
                          image,
                          range);

  VkPipelineStageFlags srcStageFlags = vkuGetImageStageFlags(oldLayout);
  VkPipelineStageFlags dstStageFlags = vkuGetImageStageFlags(newLayout);

  vkCmdPipelineBarrier(commandBuffer,
                       srcStageFlags,
                       dstStageFlags,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &imageMemoryBarrier);
}

inline VkAccessFlags
vkuGetImageAccessFlags(VkImageLayout imageLayout)
{
  VkAccessFlags flags = 0;
  switch (imageLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      break;
    case VK_IMAGE_LAYOUT_GENERAL:
      flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
              VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
              VK_ACCESS_HOST_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      flags = VK_ACCESS_SHADER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      flags = VK_ACCESS_TRANSFER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      flags = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      flags = VK_ACCESS_HOST_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      break;
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      break;
    default:
      break;
  }
  return flags;
}

inline VkPipelineStageFlags
vkuGetImageStageFlags(VkImageLayout imageLayout)
{
  VkPipelineStageFlags flags = 0;
  switch (imageLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_GENERAL:
      flags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      flags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: // fallthrough
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      flags = VK_PIPELINE_STAGE_HOST_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      break;
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      break;
    default:
      break;
  }
  return flags;
}

inline VkImageAspectFlags
vkuGetImageAspectFlags(VkFormat format)
{
  VkImageAspectFlags flags = 0;
  switch (format) {
    case VK_FORMAT_D16_UNORM:           // fallthrough
    case VK_FORMAT_X8_D24_UNORM_PACK32: // fallthrough
    case VK_FORMAT_D32_SFLOAT:          // fallthrough
      flags = VK_IMAGE_ASPECT_DEPTH_BIT;
      break;
    case VK_FORMAT_S8_UINT:
      flags = VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D16_UNORM_S8_UINT: // fallthrough
    case VK_FORMAT_D24_UNORM_S8_UINT: // fallthrough
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default:
      flags = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
  }
  return flags;
}

static uint32_t
findMemoryTypeIdx(const VkMemoryRequirements& memoryRequirements,
                  const VkPhysicalDeviceMemoryProperties& memoryProperties,
                  const VkMemoryPropertyFlags& propertyFlags)
{
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
    if (memoryRequirements.memoryTypeBits & (1 << i) &&
        (memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) ==
          propertyFlags) {
      return i;
      break;
    }
  }

  return -1;
}

inline VkDeviceMemory
vkuAllocateMemory(VkDevice device,
                  VkDeviceSize allocationSize,
                  uint32_t memoryTypeIndex,
                  const VkAllocationCallbacks* pAllocator = nullptr)
{
  auto info = vkiMemoryAllocateInfo(allocationSize, memoryTypeIndex);
  VkDeviceMemory handle = VK_NULL_HANDLE;
  vkAllocateMemory(device, &info, pAllocator, &handle);
  return handle;
}

inline VkDeviceMemory
vkuAllocateImageMemory(VkDevice device,
                       VkPhysicalDeviceMemoryProperties memProps,
                       VkImage image,
                       bool bind)
{
  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(device, image, &memoryRequirements);

  VkDeviceMemory deviceMemory = vkuAllocateMemory(
    device,
    memoryRequirements.size,
    findMemoryTypeIdx(
      memoryRequirements, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

  if (bind) {
    vkBindImageMemory(device, image, deviceMemory, 0);
  }

  return deviceMemory;
}

inline VkDeviceMemory
vkuAllocateBufferMemory(VkDevice device,
                        VkPhysicalDeviceMemoryProperties memProps,
                        VkBuffer buffer,
                        VkMemoryPropertyFlags propertyFlags,
                        bool bind)
{
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

  VkDeviceMemory deviceMemory = vkuAllocateMemory(
    device,
    memoryRequirements.size,
    findMemoryTypeIdx(memoryRequirements, memProps, propertyFlags));

  if (bind)
    vkBindBufferMemory(device, buffer, deviceMemory, 0);

  return deviceMemory;
}

inline void
vkuTransferData(VkDevice device,
                VkDeviceMemory memory,
                VkDeviceSize offset,
                VkDeviceSize size,
                void* data)
{
  void* mappedMemory;
  vkMapMemory(device, memory, 0, size, 0, &mappedMemory);
  memcpy(mappedMemory, data, size);
  auto memoryRange = vkiMappedMemoryRange(memory, offset, VK_WHOLE_SIZE);
  vkFlushMappedMemoryRanges(device, 1, &memoryRange);
  vkUnmapMemory(device, memory);
}

inline VkCommandBuffer
vkuAllocateCmdBuffer(VkDevice device,
                     VkCommandPool cmdPool,
                     VkCommandBufferLevel level)
{
  auto info = vkiCommandBufferAllocateInfo(cmdPool, level, 1);
  VkCommandBuffer handle = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(device, &info, &handle);
  return handle;
}

inline void
vkuBeginCmdBuffer(VkCommandBuffer commandBuffer,
                  VkCommandBufferUsageFlags flags)
{
  auto info = vkiCommandBufferBeginInfo(nullptr);
  info.flags = flags;
  vkBeginCommandBuffer(commandBuffer, &info);
}

inline void
vkuTransferImageData(VkDevice device,
                     VkPhysicalDeviceMemoryProperties memProps,
                     VkCommandPool cmdPool,
                     VkQueue graphicsQueue,
                     VkImage image,
                     VkFormat format,
                     VkExtent3D extent,
                     VkImageLayout oldLayout,
                     VkImageLayout newLayout,
                     VkDeviceSize size,
                     void* data)
{
  VkBuffer stagingBuffer = vkuCreateBuffer(device,
                                           size,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           {});

  VkDeviceMemory stagingBufferMemory = vkuAllocateBufferMemory(
    device,
    memProps,
    stagingBuffer,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    true);

  vkuTransferData(device, stagingBufferMemory, 0, size, data);

  VkCommandBuffer cmdBuffer =
    vkuAllocateCmdBuffer(device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  vkuBeginCmdBuffer(cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VkImageSubresourceRange imageSubresourceRange =
    vkiImageSubresourceRange(vkuGetImageAspectFlags(format), 0, 1, 0, 1);

  vkuTransitionLayout(cmdBuffer,
                      image,
                      imageSubresourceRange,
                      oldLayout,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  auto bufferCopyRegion = vkiBufferImageCopy(
    0,
    0,
    0,
    vkiImageSubresourceLayers(vkuGetImageAspectFlags(format), 0, 0, 1),
    {},
    extent);

  vkCmdCopyBufferToImage(cmdBuffer,
                         stagingBuffer,
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &bufferCopyRegion);

  vkuTransitionLayout(cmdBuffer,
                      image,
                      imageSubresourceRange,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      newLayout);

  vkEndCommandBuffer(cmdBuffer);

  VkFence fence = vkuCreateFence(device);
  auto submitInfo =
    vkiSubmitInfo(0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr);

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
  vkWaitForFences(device, 1, &fence, true, (uint64_t)-1);
  vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
  vkDestroyFence(device, fence, nullptr);
  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);
}

inline void
vkuTransferBufferData(VkDevice device,
                      VkPhysicalDeviceMemoryProperties memProps,
                      VkCommandPool cmdPool,
                      VkQueue graphicsQueue,
                      VkBuffer buffer,
                      VkDeviceSize offset,
                      VkDeviceSize size,
                      void* data)
{
  VkBuffer stagingBuffer = vkuCreateBuffer(device,
                                           size,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           {});

  VkDeviceMemory stagingBufferMemory = vkuAllocateBufferMemory(
    device,
    memProps,
    stagingBuffer,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    true);

  vkuTransferData(device, stagingBufferMemory, 0, size, data);

  VkCommandBuffer cmdBuffer =
    vkuAllocateCmdBuffer(device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  vkuBeginCmdBuffer(cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VkBufferCopy copyRegion = { 0, offset, size };
  vkCmdCopyBuffer(cmdBuffer, stagingBuffer, buffer, 1, &copyRegion);
  vkEndCommandBuffer(cmdBuffer);

  VkFence fence = vkuCreateFence(device);
  auto submitInfo =
    vkiSubmitInfo(0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr);
  vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
  vkWaitForFences(device, 1, &fence, true, (uint64_t)-1);
  vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
  vkDestroyFence(device, fence, nullptr);
  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);
}
#endif
