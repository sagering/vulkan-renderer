
#include "vk_base.h"
#include "window.h"

#include "rendergraph.h"
#include "pipeline.h"

uint32_t Operation::nextId = 0;

struct TextureSubpass : Subpass
{
  TextureSubpass(VkDevice device,
                 DeviceProps deviceProps,
                 const char* imgName,
                 uint32_t specialization)
    : Subpass()
    , device(device)
    , deviceProps(deviceProps)
    , imgName(imgName)
    , specialization(specialization)
  {
    SetOperation(imgName, Operation::ColorOutputAttachment());
  }

  VkDevice device;
  DeviceProps deviceProps;

  Pipeline* pipeline;
  const char* imgName;

  uint32_t specialization;

  struct Buffer
  {
    VkBuffer buf;
    VkDeviceMemory mem;
  };

  const int DYN_VERTEX_BUFFER_SIZE = 1024 * 1024 * 2;
  Buffer vbuffer = {};
  uint8_t* vbufferHostMemory;

  void OnBakeDone() override
  {
    auto img = graph->vis[imgName];
    auto w = img->extent.width;
    auto h = img->extent.height;

    PipelineState pipelineState = {};
    pipelineState.shader.stages[0].shaderName = "main.vert.spv";
    pipelineState.shader.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineState.shader.stageCount += 1;

    pipelineState.shader.stages[1].shaderName = "main.frag.spv";
    pipelineState.shader.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    memcpy(pipelineState.shader.stages[1].specialization.data,
           &specialization,
           sizeof(specialization));
    pipelineState.shader.stages[1].specialization.dataSize = sizeof(uint32_t);
    pipelineState.shader.stages[1].specialization.mapEntries[0].constantID = 0;
    pipelineState.shader.stages[1].specialization.mapEntries[0].offset = 0;
    pipelineState.shader.stages[1].specialization.mapEntries[0].size =
      sizeof(uint32_t);
    pipelineState.shader.stages[1].specialization.mapEntryCount += 1;
    pipelineState.shader.stageCount += 1;

    // viewports / scissors depend on render pass (at least if you want to use
    // the whole attachment region)

    pipelineState.viewport.viewports[0] = { 0.0f,     0.0f, (float)w,
                                            (float)h, 0.0f, 1.0f };
    pipelineState.viewport.viewportCount += 1;
    pipelineState.viewport.scissors[0] = { { 0, 0 }, { w, h } };
    pipelineState.viewport.scissorCount += 1;

    // one color blend attachments for each color output attachment in the
    // renderpass
    pipelineState.blend.colorBlendAttachmentCount = 1;

    // depends on the mesh and on the reflection info (e.g. input attributes
    // that are actually present in the shader)

    SimplifiedVertexInputState vertexInputState = {};
    vertexInputState.attributeFlags[0] = POSITION;
    vertexInputState.attributeFlagsCount += 1;
    vertexInputState.Apply(&pipelineState);

    pipeline =
      new Pipeline(device, pipelineState, renderPass->renderPass, subpass);
    pipeline->Compile();

    // vertex buffer
    vbuffer.buf = vkuCreateBuffer(
      device, DYN_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    vbuffer.mem = vkuAllocateBufferMemory(device,
                                          deviceProps.memProps,
                                          vbuffer.buf,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                          true);

    vkMapMemory(device,
                vbuffer.mem,
                0,
                DYN_VERTEX_BUFFER_SIZE,
                0,
                (void**)&vbufferHostMemory);

    std::vector<float> verts = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };
    memcpy(vbufferHostMemory, verts.data(), verts.size() * sizeof(float));
  }

  void RecordCmds(VkCommandBuffer cmdBuffer) override
  {
    VkDeviceSize vbufferOffset = 0;
    pipeline->Bind(cmdBuffer);
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vbuffer.buf, &vbufferOffset);
    for (uint32_t i = 0; i < 512; ++i) {
      vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    }
  }
};

struct ComposePass : Subpass
{
  VkDevice device = VK_NULL_HANDLE;
  DeviceProps deviceProps = {};
  Pipeline* pipeline = nullptr;

  struct Buffer
  {
    VkBuffer buf;
    VkDeviceMemory mem;
  };

  const int VERTEX_BUFFER_SIZE = 1024 * 1024 * 2;
  Buffer vbuffer = {};
  uint8_t* vbufferHostMemory = nullptr;

  ComposePass(VkDevice device, DeviceProps deviceProps)
    : device(device)
    , deviceProps(deviceProps)
  {
    SetOperation("img1", Operation::Sampled());
    SetOperation("img2", Operation::Sampled());
    SetOperation("finalImg", Operation::ColorOutputAttachment());
  }

  void OnBakeDone() override
  {
    auto img = graph->vis["finalImg"];
    auto w = img->extent.width;
    auto h = img->extent.height;

    PipelineState pipelineState = {};
    pipelineState.shader.stages[0].shaderName = "compose.vert.spv";
    pipelineState.shader.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineState.shader.stageCount += 1;

    pipelineState.shader.stages[1].shaderName = "compose.frag.spv";
    pipelineState.shader.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineState.shader.stageCount += 1;

    // viewports / scissors depend on render pass (at least if you want to use
    // the whole attachment region)

    pipelineState.viewport.viewports[0] = { 0.0f,     0.0f, (float)w,
                                            (float)h, 0.0f, 1.0f };
    pipelineState.viewport.viewportCount += 1;
    pipelineState.viewport.scissors[0] = { { 0, 0 }, { w, h } };
    pipelineState.viewport.scissorCount += 1;

    // one color blend attachments for each color output attachment in the
    // renderpass
    pipelineState.blend.colorBlendAttachmentCount = 1;

    // depends on the mesh and on the reflection info (e.g. input attributes
    // that are actually present in the shader)

    SimplifiedVertexInputState vertexInputState = {};
    vertexInputState.attributeFlags[0] = POSITION | TEXTURE_COORD;
    vertexInputState.attributeFlagsCount += 1;
    vertexInputState.Apply(&pipelineState);

    pipeline =
      new Pipeline(device, pipelineState, renderPass->renderPass, subpass);
    pipeline->Compile();

    // vertex buffer
    vbuffer.buf = vkuCreateBuffer(
      device, VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    vbuffer.mem = vkuAllocateBufferMemory(device,
                                          deviceProps.memProps,
                                          vbuffer.buf,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                          true);

    vkMapMemory(device,
                vbuffer.mem,
                0,
                VERTEX_BUFFER_SIZE,
                0,
                (void**)&vbufferHostMemory);

    std::vector<float> verts = {
      -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
      -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
      -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 1.0f, 1.0f,

      0.0f,  -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
      0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
      0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
    };

    memcpy(vbufferHostMemory, verts.data(), verts.size() * sizeof(float));

    auto poolSize =
      vkiDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
    auto poolCreateInfo = vkiDescriptorPoolCreateInfo(2, 1, &poolSize);
    vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &pool);

    for (uint32_t set = 0; set < 2; ++set) {
      auto samplerInfo =
        vkiSamplerCreateInfo(VK_FILTER_NEAREST,
                             VK_FILTER_NEAREST,
                             VK_SAMPLER_MIPMAP_MODE_LINEAR,
                             VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             0.f,
                             VK_FALSE,
                             0.f,
                             VK_FALSE,
                             VK_COMPARE_OP_NEVER,
                             0.f,
                             0.f,
                             VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                             VK_FALSE);

      vkCreateSampler(device, &samplerInfo, nullptr, &samplers[set]);
      auto layout = pipeline->GetDescriptorSetLayout(0);
      auto allocateInfo = vkiDescriptorSetAllocateInfo(pool, 1, &layout);
      vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSets[set]);
    }
  }

  void UpdateDescriptorSets()
  {
    static bool updated = false;

    if (updated) {
      return;
    }

    PhysicalImage* pImages[2] = { graph->pis["img1"], graph->pis["img2"] };

    for (uint32_t set = 0; set < 2; ++set) {
      auto imageInfo =
        vkiDescriptorImageInfo(samplers[set],
                               pImages[set]->view,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      auto descriptorWrite =
        vkiWriteDescriptorSet(descriptorSets[set],
                              0,
                              0,
                              1,
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                              &imageInfo,
                              nullptr,
                              nullptr);

      vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    updated = true;
  }

  VkDescriptorPool pool = VK_NULL_HANDLE;

  VkDescriptorSet descriptorSets[2] = {};
  VkSampler samplers[2] = {};

  void RecordCmds(VkCommandBuffer cmdBuffer) override
  {
    UpdateDescriptorSets();

    VkDeviceSize vbufferOffset = 0;
    pipeline->Bind(cmdBuffer);

    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vbuffer.buf, &vbufferOffset);
    for (uint32_t i = 0; i < 2; ++i) {
      pipeline->BindDescriptorSets(
        cmdBuffer, 0, 1, &descriptorSets[i], 0, nullptr);

      for (uint32_t j = 0; j < 512; ++j) {
        vkCmdDraw(cmdBuffer, 6, 1, i * 6, 0);
      }
    }
  }
};

void
main()
{
  Window window(1280, 920, "Playground");
  VulkanBase base(&window);
  Swapchain swapchain(base.device, base.deviceProps, base.surface);

  RenderGraph* graph = new RenderGraph;

  VirtualImage* img1 = new VirtualImage;
  img1->extent = { swapchain.extent.width, swapchain.extent.height, 1 };
  img1->format = VK_FORMAT_R8G8B8A8_SRGB;
  img1->samples = VK_SAMPLE_COUNT_1_BIT;
  img1->layers = 1;
  img1->levels = 1;
  img1->subresourceRange = {
    vkuGetImageAspectFlags(img1->format), 0, img1->levels, 0, img1->layers
  };

  graph->AddVirtualImage("img1", img1);

  VirtualImage* img2 = new VirtualImage;
  img2->extent = { swapchain.extent.width, swapchain.extent.height, 1 };
  img2->format = VK_FORMAT_R8G8B8A8_SRGB;
  img2->samples = VK_SAMPLE_COUNT_1_BIT;
  img2->layers = 1;
  img2->levels = 1;
  img2->subresourceRange = {
    vkuGetImageAspectFlags(img2->format), 0, img2->levels, 0, img2->layers
  };

  graph->AddVirtualImage("img2", img2);

  VirtualImage* finalImg = new VirtualImage;
  finalImg->extent = { swapchain.extent.width, swapchain.extent.height, 1 };
  finalImg->format = swapchain.format.format;
  finalImg->samples = VK_SAMPLE_COUNT_1_BIT;
  finalImg->layers = 1;
  finalImg->levels = 1;
  finalImg->subresourceRange = { vkuGetImageAspectFlags(img2->format),
                                 0,
                                 finalImg->levels,
                                 0,
                                 finalImg->layers };

  graph->AddVirtualImage("finalImg", finalImg);

  RenderPass* renderPass0 = new RenderPass;

  renderPass0->AddSubpass(
    new TextureSubpass(base.device, base.deviceProps, "img1", 0));
  renderPass0->AddSubpass(
    new TextureSubpass(base.device, base.deviceProps, "img2", 1));

  RenderPass* renderPass1 = new RenderPass;
  renderPass1->AddSubpass(new ComposePass(base.device, base.deviceProps));

  graph->AddRenderPass(renderPass0);
  graph->AddRenderPass(renderPass1);

  graph->Bake(base.device);

  graph->pis["img1"] = graph->vis["img1"]->CreatePhysicalImage(
    base.device, base.deviceProps.memProps);
  graph->pis["img2"] = graph->vis["img2"]->CreatePhysicalImage(
    base.device, base.deviceProps.memProps);

  swapchain.CreatePhysicalSwapchain(graph->vis["finalImg"]->usage);

  while (true) {
    VkDevice device = base.device;
    auto cmdBuffer = base.NextCmdBuffer();
    VkQueue queue = base.queue;
    VkSemaphore imageAvailableSemaphore = base.imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore = base.renderFinishedSemaphore;

    graph->pis["finalImg"] =
      swapchain.AcquireImage(base.imageAvailableSemaphore);

    VkCommandBufferBeginInfo beginInfo = vkiCommandBufferBeginInfo(nullptr);
    ASSERT_VK_SUCCESS(vkBeginCommandBuffer(cmdBuffer.cmdBuffer, &beginInfo));

    graph->RecordCmds(device, cmdBuffer.cmdBuffer);

    auto barrier =
      vkiImageMemoryBarrier(0,
                            0,
                            graph->imageRanges["finalImg"].back().op.layout,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_QUEUE_FAMILY_IGNORED,
                            -1,
                            graph->pis["finalImg"]->image,
                            graph->vis["finalImg"]->subresourceRange);

    vkCmdPipelineBarrier(cmdBuffer.cmdBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    ASSERT_VK_SUCCESS(vkEndCommandBuffer(cmdBuffer.cmdBuffer));

    VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = vkiSubmitInfo(1,
                                            &imageAvailableSemaphore,
                                            &waitStage,
                                            1,
                                            &cmdBuffer.cmdBuffer,
                                            1,
                                            &renderFinishedSemaphore);

    graph->pis["finalImg"]->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    graph->pis["finalImg"]->stageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    graph->pis["finalImg"]->accessFlags = 0;

    ASSERT_VK_SUCCESS(vkQueueSubmit(queue, 1, &submitInfo, cmdBuffer.fence));
    swapchain.Present(base.queue, base.renderFinishedSemaphore);
  }
}
