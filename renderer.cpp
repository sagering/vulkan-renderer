#include "renderer.h"

#include "vk_init.h"
#include "vk_utils.h"

Renderer::Renderer(VulkanWindow* window)
  : VulkanBase(window)
{
  createResources();
}

std::string
LoadFile(const char* _filename)
{
  std::string buff;

  FILE* file = 0;
  fopen_s(&file, _filename, "rb");
  if (file) {
    fseek(file, 0, SEEK_END);
    size_t bytes = ftell(file);

    buff.resize(bytes);

    fseek(file, 0, SEEK_SET);
    fread(&(*buff.begin()), 1, bytes, file);
    fclose(file);
    return buff;
  }

  return buff;
}

VkShaderModule
LoadShaderModule(VkDevice device, const char* filename)
{
  std::string buff = LoadFile(filename);
  auto result =
    vkuCreateShaderModule(device, buff.size(), (uint32_t*)buff.data(), nullptr);
  ASSERT_VK_VALID_HANDLE(result);
  return result;
}

void
Renderer::createResources()
{
  // depth stencil image / view
  auto depthStencilFormat = VK_FORMAT_D32_SFLOAT;

  VkImageCreateInfo depthStencilImageInfo = vkiImageCreateInfo(
    VK_IMAGE_TYPE_2D,
    depthStencilFormat,
    { swapchain->imageExtent.width, swapchain->imageExtent.height, 1 },
    1,
    1,
    VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_SHARING_MODE_EXCLUSIVE,
    VK_QUEUE_FAMILY_IGNORED,
    nullptr,
    VK_IMAGE_LAYOUT_UNDEFINED);

  ASSERT_VK_SUCCESS(
    vkCreateImage(device, &depthStencilImageInfo, nullptr, &depthStencilImage));

  depthStencilImageMemory = vkuAllocateImageMemory(
    device, physicalDeviceProps.memProps, depthStencilImage, true);
  VkImageSubresourceRange dRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
  VkImageViewCreateInfo dImageViewInfo =
    vkiImageViewCreateInfo(depthStencilImage,
                           VK_IMAGE_VIEW_TYPE_2D,
                           depthStencilImageInfo.format,
                           { VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY },
                           dRange);

  ASSERT_VK_SUCCESS(vkCreateImageView(
    device, &dImageViewInfo, nullptr, &depthStencilImageView));

  // renderpass 2: post process
  // attachments: 0 swapchain image, 1 depth stencil image

  std::vector<VkAttachmentDescription> attachmentDescriptions;
  attachmentDescriptions.push_back(
    vkiAttachmentDescription(swapchain->surfaceFormat.format,
                             VK_SAMPLE_COUNT_1_BIT,
                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VK_ATTACHMENT_STORE_OP_STORE,
                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

  attachmentDescriptions.push_back(
    vkiAttachmentDescription(depthStencilFormat,
                             VK_SAMPLE_COUNT_1_BIT,
                             VK_ATTACHMENT_LOAD_OP_LOAD,
                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                             VK_ATTACHMENT_LOAD_OP_LOAD,
                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

  VkAttachmentReference colorAttachmentRef =
    vkiAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkAttachmentReference depthStencilAttachmentRef =
    vkiAttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  VkSubpassDescription subpassDesc =
    vkiSubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
                          0,
                          nullptr,
                          1,
                          &colorAttachmentRef,
                          nullptr,
                          &depthStencilAttachmentRef,
                          0,
                          nullptr);

  std::vector<VkSubpassDependency> dependencies;
  dependencies.push_back(vkiSubpassDependency(
    VK_SUBPASS_EXTERNAL,
    0,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    {}));

  dependencies.push_back(
    vkiSubpassDependency(VK_SUBPASS_EXTERNAL,
                         0,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                         {}));

  auto renderPassCreateInfo = vkiRenderPassCreateInfo(
    static_cast<uint32_t>(attachmentDescriptions.size()),
    attachmentDescriptions.data(),
    1,
    &subpassDesc,
    static_cast<uint32_t>(dependencies.size()),
    dependencies.data());

  ASSERT_VK_SUCCESS(
    vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderpass));

  // framebuffers for this pass
  framebuffers.resize(swapchain->imageCount);
  for (uint32_t i = 0; i < swapchain->imageCount; ++i) {
    VkImageView attachments[] = { swapchain->imageViews[i],
                                  depthStencilImageView };
    VkFramebufferCreateInfo createInfo =
      vkiFramebufferCreateInfo(renderpass,
                               2,
                               attachments,
                               swapchain->imageExtent.width,
                               swapchain->imageExtent.height,
                               1);

    ASSERT_VK_SUCCESS(
      vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffers[i]));
  }

  // shader modules
  fshader = LoadShaderModule(device, "main.frag.spv");
  vshader = LoadShaderModule(device, "main.vert.spv");

  // pipelines
  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  pipeline =
    GraphicsPipeline::GetBuilder()
      .SetDevice(device)
      .SetVertexShader(vshader)
      .SetFragmentShader(fshader)
      .SetVertexBindings({ SimpleVertex::GetBindingDescription() })
      .SetVertexAttributes(SimpleVertex::GetAttributeDescriptions())
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
      .SetViewports({ { 0.0f,
                        0.0f,
                        (float)swapchain->imageExtent.width,
                        (float)swapchain->imageExtent.height,
                        0.0f,
                        1.0f } })
      .SetScissors(
        { { { 0, 0 },
            { swapchain->imageExtent.width, swapchain->imageExtent.height } } })
      .SetColorBlendAttachments({ colorBlendAttachment })
      .SetDepthWriteEnable(VK_FALSE)
      .SetDepthTestEnable(VK_TRUE)
      .SetRenderPass(renderpass)
      .Build();

  // vertex buffer
  vbuffer = vkuCreateBuffer(device,
                            DYN_VERTEX_BUFFER_SIZE,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_SHARING_MODE_EXCLUSIVE,
                            {});

  vbufferMemory = vkuAllocateBufferMemory(device,
                                          physicalDeviceProps.memProps,
                                          vbuffer,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                          true);

  vbufferHostMemory = (uint8_t*)malloc(DYN_VERTEX_BUFFER_SIZE);

  vkMapMemory(device,
              vbufferMemory,
              0,
              DYN_VERTEX_BUFFER_SIZE,
              0,
              (void**)&vbufferHostMemory);
}

Renderer::~Renderer()
{
  vkQueueWaitIdle(queue);
  destroyResources();
}

void
Renderer::destroyResources()
{
  // buffers
  vkDestroyBuffer(device, vbuffer, nullptr);
  vkFreeMemory(device, vbufferMemory, nullptr);

  // shader modules
  vkDestroyShaderModule(device, fshader, nullptr);
  vkDestroyShaderModule(device, vshader, nullptr);

  // pipelines
  delete pipeline;
  pipeline = nullptr;

  // renderpasses and framebuffers
  for (auto fb : framebuffers) {
    vkDestroyFramebuffer(device, fb, nullptr);
  }
  vkDestroyRenderPass(device, renderpass, nullptr);
  vkDestroyImageView(device, depthStencilImageView, nullptr);
  vkDestroyImage(device, depthStencilImage, nullptr);
  vkFreeMemory(device, depthStencilImageMemory, nullptr);
}

void
Renderer::recordCommandBuffer(uint32_t idx)
{
  ASSERT_VK_SUCCESS(
    vkWaitForFences(device, 1, &fences[idx], true, (uint64_t)-1));
  ASSERT_VK_SUCCESS(vkResetFences(device, 1, &fences[idx]));
  ASSERT_VK_SUCCESS(vkResetCommandBuffer(commandBuffers[idx], 0));

  VkCommandBufferBeginInfo beginInfo = vkiCommandBufferBeginInfo(nullptr);
  ASSERT_VK_SUCCESS(vkBeginCommandBuffer(commandBuffers[idx], &beginInfo));

  VkClearValue clearValues[] = { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.f, 0 } };
  VkRenderPassBeginInfo renderPassInfo =
    vkiRenderPassBeginInfo(renderpass,
                           framebuffers[idx],
                           { { 0, 0 }, swapchain->imageExtent },
                           2,
                           clearValues);

  vkCmdBeginRenderPass(
    commandBuffers[idx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkDeviceSize vbufferOffset =
    curDynVertexBufferPartition * DYN_VERTEX_BUFFER_PARTITION_SIZE;
  vkCmdBindVertexBuffers(commandBuffers[idx], 0, 1, &vbuffer, &vbufferOffset);

  vkCmdBindPipeline(
    commandBuffers[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

  vkCmdDraw(commandBuffers[idx], totalNumVertices, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffers[idx]);

  ASSERT_VK_SUCCESS(vkEndCommandBuffer(commandBuffers[idx]));
}

void
Renderer::drawFrame()
{
  uint32_t nextImageIdx = -1;
  ASSERT_VK_SUCCESS(vkAcquireNextImageKHR(device,
                                          swapchain->handle,
                                          UINT64_MAX,
                                          imageAvailableSemaphore,
                                          VK_NULL_HANDLE,
                                          &nextImageIdx));

  recordCommandBuffer(nextImageIdx);

  VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkSubmitInfo submitInfo = vkiSubmitInfo(1,
                                          &imageAvailableSemaphore,
                                          waitStages,
                                          1,
                                          &commandBuffers[nextImageIdx],
                                          1,
                                          &renderFinishedSemaphore);
  ASSERT_VK_SUCCESS(vkQueueSubmit(queue, 1, &submitInfo, fences[nextImageIdx]));

  VkPresentInfoKHR presentInfo = vkiPresentInfoKHR(
    1, &renderFinishedSemaphore, 1, &swapchain->handle, &nextImageIdx, nullptr);
  ASSERT_VK_SUCCESS(vkQueuePresentKHR(queue, &presentInfo));

  // reset offsets / counts for the dynamic vertex buffer after each frame
  totalNumVertices = 0;
  curDynVertexBufferPartition = (curDynVertexBufferPartition + 1) % 2;
}
void
Renderer::pushVertices(const std::vector<float>& floats)
{
  size_t offset =
    curDynVertexBufferPartition * DYN_VERTEX_BUFFER_PARTITION_SIZE +
    totalNumVertices * sizeof(float) * 3;

  size_t size = floats.size() * sizeof(float);
  ASSERT_TRUE(offset + size < DYN_VERTEX_BUFFER_SIZE);

  memcpy(vbufferHostMemory + offset, floats.data(), size);
  totalNumVertices += static_cast<uint32_t>(floats.size()) / 3;
}

void
Renderer::OnSwapchainReinitialized()
{
  destroyResources();
  createResources();
}
