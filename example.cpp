// clang-format off
#include <vulkan\vulkan_core.h>
#include <GLFW\glfw3.h>
// clang-format on

#include <glm\gtx\transform.hpp>  // lookAt, perspective
#include <glm\gtx\quaternion.hpp> // quat

#include "window.h"
#include "playground.h"
#include "pipeline_state.h"
#include "pipeline.h"

struct RenderTexturePass : RenderPass
{
  RenderTexturePass(VkDevice device,
                    DeviceProps deviceProps,
                    RenderGraph* graph,
                    const char* imgName,
                    uint32_t specialization)
    : RenderPass(graph)
    , device(device)
    , deviceProps(deviceProps)
    , imgName(imgName)
    , specialization(specialization)
  {
    auto img = graph->GetVirtualImage(imgName);
    img->AddOperation(Operation::ColorOutputAttachment());
    clearValues.insert({ imgName, { 0.f, 0.f, 0.f } });
    images.push_back(imgName);
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

  void OnBuildDone(RenderGraph* graph) override
  {
    auto img = graph->GetVirtualImage(imgName);
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

    pipeline = new Pipeline(device, pipelineState, renderPass, 0);
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

  void OnRecordRenderPassCommands(VkCommandBuffer cmdBuffer) override
  {
    VkDeviceSize vbufferOffset = 0;
    pipeline->Bind(cmdBuffer);
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vbuffer.buf, &vbufferOffset);
    for (uint32_t i = 0; i < 512; ++i) {
      vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    }
  }
};

struct ComposePass : RenderPass
{
  Pipeline* pipeline = nullptr;
  DeviceProps deviceProps;

  struct Buffer
  {
    VkBuffer buf;
    VkDeviceMemory mem;
  };

  const int VERTEX_BUFFER_SIZE = 1024 * 1024 * 2;
  Buffer vbuffer = {};
  uint8_t* vbufferHostMemory;

  ComposePass(DeviceProps deviceProps, RenderGraph* graph)
    : RenderPass(graph)
    , deviceProps(deviceProps)
  {
    auto img1 = graph->GetVirtualImage("img1");
    img1->AddOperation(Operation::Sampled());
    clearValues.insert({ "img1", { 0.f, 0.f, 0.f } });
    images.push_back("img1");

    auto img2 = graph->GetVirtualImage("img2");
    img2->AddOperation(Operation::Sampled());
    clearValues.insert({ "img2", { 0.f, 0.f, 0.f } });
    images.push_back("img2");

    auto finalImg = graph->GetVirtualImage("finalImg");
    finalImg->AddOperation(Operation::ColorOutputAttachment());
    clearValues.insert({ "finalImg", { 0.f, 0.f, 0.f } });
    images.push_back("finalImg");
  }

  void OnBuildDone(RenderGraph* graph) override
  {
    auto img = graph->GetVirtualImage("finalImg");
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

    pipeline = new Pipeline(graph->GetDevice(), pipelineState, renderPass, 0);
    pipeline->Compile();

    // vertex buffer
    vbuffer.buf = vkuCreateBuffer(graph->GetDevice(),
                                  VERTEX_BUFFER_SIZE,
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    vbuffer.mem = vkuAllocateBufferMemory(graph->GetDevice(),
                                          deviceProps.memProps,
                                          vbuffer.buf,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                          true);

    vkMapMemory(graph->GetDevice(),
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
    vkCreateDescriptorPool(graph->GetDevice(), &poolCreateInfo, nullptr, &pool);

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

      vkCreateSampler(
        graph->GetDevice(), &samplerInfo, nullptr, &samplers[set]);
      auto layout = pipeline->GetDescriptorSetLayout(0);
      auto allocateInfo = vkiDescriptorSetAllocateInfo(pool, 1, &layout);
      vkAllocateDescriptorSets(
        graph->GetDevice(), &allocateInfo, &descriptorSets[set]);
    }
  }

  void UpdateDescriptorSets()
  {
    static bool updated = false;

    if (updated) {
      return;
    }

    PhysicalImage* pImages[2] = { graph->GetPhysicalImage("img1"),
                                  graph->GetPhysicalImage("img2") };

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

      vkUpdateDescriptorSets(
        graph->GetDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    updated = true;
  }

  VkDescriptorPool pool = VK_NULL_HANDLE;

  VkDescriptorSet descriptorSets[2] = {};
  VkSampler samplers[2] = {};

  void OnRecordRenderPassCommands(VkCommandBuffer cmdBuffer)
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
struct PresentTransition : RenderGraphWorkUnit
{
  PresentTransition(RenderGraph* graph)
    : RenderGraphWorkUnit(graph)
  {
    images.push_back("finalImg");
    graph->GetVirtualImage("finalImg")->AddOperation(Operation::PresentSrc());
  }
};

struct ExampleRenderGraph : RenderGraph
{
  ExampleRenderGraph(VulkanBase* base, Swapchain* swapchain)
    : RenderGraph(base, swapchain)
  {
    swapchain = new Swapchain(base->device, base->deviceProps, base->surface);

    // Color

    VirtualImage* img1 = new VirtualImage;
    img1->extent = { swapchain->extent.width, swapchain->extent.height, 1 };
    img1->format = VK_FORMAT_R8G8B8A8_SRGB;
    img1->samples = VK_SAMPLE_COUNT_1_BIT;
    img1->layers = 1;
    img1->levels = 1;
    img1->subresourceRange = {
      vkuGetImageAspectFlags(img1->format), 0, img1->levels, 0, img1->layers
    };

    AddVirtualImage("img1", img1);

    VirtualImage* img2 = new VirtualImage;
    img2->extent = { swapchain->extent.width, swapchain->extent.height, 1 };
    img2->format = VK_FORMAT_R8G8B8A8_SRGB;
    img2->samples = VK_SAMPLE_COUNT_1_BIT;
    img2->layers = 1;
    img2->levels = 1;
    img2->subresourceRange = {
      vkuGetImageAspectFlags(img2->format), 0, img2->levels, 0, img2->layers
    };

    AddVirtualImage("img2", img2);

    VirtualImage* finalImg = new VirtualImage;
    finalImg->extent = { swapchain->extent.width, swapchain->extent.height, 1 };
    finalImg->format = swapchain->format.format;
    finalImg->samples = VK_SAMPLE_COUNT_1_BIT;
    finalImg->layers = 1;
    finalImg->levels = 1;
    finalImg->subresourceRange = { vkuGetImageAspectFlags(img2->format),
                                   0,
                                   finalImg->levels,
                                   0,
                                   finalImg->layers };

    AddVirtualImage("finalImg", finalImg);

    // Work

    AddWork(
      "pass1",
      new RenderTexturePass(base->device, base->deviceProps, this, "img1", 0));
    AddWork(
      "pass2",
      new RenderTexturePass(base->device, base->deviceProps, this, "img2", 1));

    AddWork("compose", new ComposePass(base->deviceProps, this));

    AddWork("presentTransition", new PresentTransition(this));
  }

  void OnSetupPhysicalImages() override
  {
    physicalImages["img1"] = GetVirtualImage("img1")->CreatePhysicalImage(
      base->device, base->deviceProps.memProps);
    physicalImages["img2"] = GetVirtualImage("img2")->CreatePhysicalImage(
      base->device, base->deviceProps.memProps);

    swapchain->CreatePhysicalSwapchain(GetVirtualImage("finalImg")->usage);
  }

  void OnFrameResolvePhysicalImages() override
  {
    physicalImages["finalImg"] =
      swapchain->AcquireImage(base->imageAvailableSemaphore);
  }

  void OnFrame() override
  {
    swapchain->Present(base->queue, base->renderFinishedSemaphore);
  }
};

int
main()
{
  {
    Window window(1280, 920, "Playground");
    VulkanBase base(&window);
    Swapchain swapchain(base.device, base.deviceProps, base.surface);
    ExampleRenderGraph rg(&base, &swapchain);

    rg.Setup();

    while (window.keyboardState.key[GLFW_KEY_ESCAPE] != 1) {
      window.Update();
      rg.RenderFrame();
    }
  }

  return 0;
}
