#pragma once

#include <algorithm>
#include <functional>
#include <glm\glm.hpp>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "vk_base.h"
#include "vk_init.h"
#include "vk_utils.h"

#include "graphics_pipeline.h"

struct Barrier
{
  VkPipelineStageFlags srcStage;
  VkPipelineStageFlags dstStage;
  VkAccessFlags srcMask;
  VkAccessFlags dstMask;
  VkImageLayout oldLayout;
  VkImageLayout newLayout;
};

struct Operation
{
  VkImageUsageFlags usage;

  VkPipelineStageFlags stageFlags;
  VkAccessFlags accessFlags;
  VkImageLayout layout;

  static Operation ColorOutputAttachment()
  {
    Operation op;
    op.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    op.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    op.stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    op.accessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    return op;
  }

  static Operation DepthStencilAttachment()
  {
    Operation op;
    op.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    op.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    op.stageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    op.accessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    return op;
  }

  bool HasAttachmentUsageFlags()
  {
    VkImageUsageFlags mask = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    return (usage & mask) > 0;
  }

  static Operation InputAttachment() {}

  bool HasWriteFlags() const { return false; }
};

struct AttachmentLoadStoreOp
{
  VkAttachmentLoadOp loadOp;
  VkAttachmentStoreOp storeOp;
  VkAttachmentLoadOp stencilLoadOp;
  VkAttachmentStoreOp stencilStoreOp;
};

struct OperationRange
{
  Operation op;

  uint32_t start; // inclusive
  uint32_t end;   // exclusive, currently not used
};

struct VirtualImage
{
  VkFormat format;
  VkExtent3D extent;
  uint32_t layers;
  uint32_t levels;
  VkSampleCountFlagBits samples;
  VkImageSubresourceRange subresourceRange;

  VkImageUsageFlags usage;

  // Is there a better place to store these?
  std::vector<Operation> ops;
  std::vector<Barrier> barriers;
  std::vector<int> barrierIndices;
  std::vector<AttachmentLoadStoreOp> loadStoreOps;
  std::vector<int> loadStoreOpsIndices;

  bool HasStencilFormat()
  {
    switch (format) {
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
      case VK_FORMAT_S8_UINT:
        return true;
      default:
        return false;
    }
  }

  bool HasStencilOnlyFormat() { return format == VK_FORMAT_S8_UINT; }

  PhysicalImage* CreatePhysicalImage(VkDevice device,
                                     VkPhysicalDeviceMemoryProperties memProps)
  {
    VkImage image;
    VkImageCreateInfo imageCreateInfo =
      vkiImageCreateInfo(VK_IMAGE_TYPE_2D,
                         format,
                         extent,
                         levels,
                         layers,
                         samples,
                         VK_IMAGE_TILING_OPTIMAL,
                         usage,
                         VK_SHARING_MODE_EXCLUSIVE,
                         VK_QUEUE_FAMILY_IGNORED,
                         nullptr,
                         VK_IMAGE_LAYOUT_UNDEFINED);

    ASSERT_VK_SUCCESS(vkCreateImage(device, &imageCreateInfo, nullptr, &image));

    VkDeviceMemory memory =
      vkuAllocateImageMemory(device, memProps, image, true);

    VkImageViewCreateInfo imageViewCreateInfo =
      vkiImageViewCreateInfo(image,
                             VK_IMAGE_VIEW_TYPE_2D,
                             format,
                             { VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY },
                             subresourceRange);

    VkImageView view;
    ASSERT_VK_SUCCESS(
      vkCreateImageView(device, &imageViewCreateInfo, nullptr, &view));

    PhysicalImage* physicalImage = new PhysicalImage;
    physicalImage->image = image;
    physicalImage->memory = memory;
    physicalImage->view = view;
    physicalImage->stageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    physicalImage->accessFlags = 0;
    physicalImage->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    return physicalImage;
  }
};

struct RenderPass;
struct RenderGraph
{
  RenderGraph(VkDevice device)
    : device(device)
  {}

  void AddRenderPass(const std::string& name, RenderPass* pass)
  {
    passes[name] = pass;
  }

  void AddVirtualImage(const std::string& name, VirtualImage* vImage)
  {
    images[name] = vImage;
  }

  VirtualImage* GetVirtualImage(const std::string& name)
  {
    auto result = images.find(name);
    return result == images.end() ? nullptr : (*result).second;
  }

  PhysicalImage* GetPhysicalImage(const std::string& name)
  {
    return physicalImages.find(name)->second;
  }

  void Setup();

  virtual void OnSetupVirtualImages() {}
  virtual void OnSetupRenderPasses() {}
  virtual void OnSetupPhysicalImages() {}
  virtual void Render() {}

protected:
  void BuildBarriers(VirtualImage* vImage, PhysicalImage* pImage)
  {
    std::vector<OperationRange> ranges;
    ranges.push_back(
      { vImage->ops[0], 0, static_cast<uint32_t>(vImage->ops.size()) });
    for (uint32_t i = 1; i < vImage->ops.size(); ++i) {
      auto& currRange = ranges.back();
      if (vImage->ops[i].HasWriteFlags() || currRange.op.HasWriteFlags() ||
          vImage->ops[i].layout != currRange.op.layout) {
        // end current range here
        currRange.end = i;

        // begin new range
        ranges.push_back(
          { vImage->ops[i], 0, static_cast<uint32_t>(vImage->ops.size()) });
      } else {
        // add to current range
        currRange.op.stageFlags |= vImage->ops[i].stageFlags;
        currRange.op.accessFlags |= vImage->ops[i].accessFlags;
      }
    }

    vImage->barriers.clear();
    vImage->barrierIndices.clear();

    vImage->barrierIndices.resize(vImage->ops.size(), -1);
    for (uint32_t i = 0; i < ranges.size(); ++i) {
      Barrier barrier;

      if (i == 0) {
        barrier.srcStage = pImage->stageFlags;
        barrier.srcMask = pImage->accessFlags;
        barrier.oldLayout = pImage->layout;
      } else {
        barrier.srcStage = ranges[i - 1].op.stageFlags;
        barrier.srcMask = ranges[i - 1].op.accessFlags;
        barrier.oldLayout = ranges[i - 1].op.layout;
      }

      barrier.dstStage = ranges[i].op.stageFlags;
      barrier.dstMask = ranges[i].op.accessFlags;
      barrier.newLayout = ranges[i].op.layout;

      vImage->barriers.push_back(barrier);
      vImage->barrierIndices[ranges[i].start] = vImage->barriers.size() - 1;
    }

    if (vImage->barriers.size() > 0) {
      auto& barrier = vImage->barriers.back();
      pImage->stageFlags = barrier.dstStage;
      pImage->accessFlags = barrier.dstMask;
      pImage->layout = barrier.newLayout;
    }
  }

private:
  void AnalyzeVirtualImage(VirtualImage* image)
  {
    if (image->ops.size() == 0)
      return;

    image->usage = 0;
    for (auto& op : image->ops) {
      image->usage |= op.usage;
    }

    image->loadStoreOpsIndices.resize(image->ops.size(), -1);
    for (uint32_t i = 0; i < image->ops.size(); ++i) {
      if (!image->ops[i].HasAttachmentUsageFlags()) {
        continue;
      }

      AttachmentLoadStoreOp loadStoreOp;

      if (image->ops[i].HasWriteFlags()) {
        loadStoreOp.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        loadStoreOp.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      } else {
        loadStoreOp.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        loadStoreOp.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      }

      if (!image->ops[i].HasWriteFlags() || i == image->ops.size() - 1 ||
          image->ops[i + 1].HasWriteFlags()) {
        loadStoreOp.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        loadStoreOp.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      } else {
        loadStoreOp.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        loadStoreOp.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      }

      loadStoreOp.loadOp = image->HasStencilOnlyFormat()
                             ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                             : loadStoreOp.loadOp;
      loadStoreOp.stencilLoadOp = !image->HasStencilFormat()
                                    ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                    : loadStoreOp.stencilLoadOp;

      loadStoreOp.storeOp = image->HasStencilOnlyFormat()
                              ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                              : loadStoreOp.storeOp;
      loadStoreOp.stencilStoreOp = !image->HasStencilFormat()
                                     ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                     : loadStoreOp.stencilStoreOp;

      image->loadStoreOps.push_back(loadStoreOp);
      image->loadStoreOpsIndices[i] = image->loadStoreOps.size() - 1;
    }
  }

protected:
  VkDevice device;
  std::map<std::string, RenderPass*> passes;
  std::map<std::string, VirtualImage*> images;
  std::map<std::string, PhysicalImage*> physicalImages;
};

struct RenderPass
{
  std::map<std::string, VkClearValue> clearValues;
  std::map<std::string, uint32_t> images;

  std::vector<std::string> attachmentNames;
  std::vector<VkClearValue> clearValuesFlat;
  VkRenderPass renderPass;

  using Attachments = std::vector<VkImageView>;
  std::map<Attachments, VkFramebuffer> framebuffers;

  uint32_t fbWidth, fbHeight, fbLayers;

  void Build(VkDevice device, RenderGraph* graph)
  {
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    std::vector<VkAttachmentReference> depthStencilAttachmentRefs;
    std::vector<VkAttachmentReference> inputAttachmentRefs;

    fbWidth = -1, fbHeight = -1, fbLayers = -1;

    for (auto& name : attachmentNames) {
      auto idx = images[name];

      VirtualImage* image = graph->GetVirtualImage(name);
      auto op = image->ops[idx];

      if (!op.HasAttachmentUsageFlags()) {
        continue;
      }

      fbWidth = std::min(fbWidth, image->extent.width);
      fbHeight = std::min(fbHeight, image->extent.height);
      fbLayers = std::min(fbLayers, image->layers);

      clearValuesFlat.push_back(clearValues.find(name)->second);

      AttachmentLoadStoreOp loadStoreOp =
        image->loadStoreOps[image->loadStoreOpsIndices[idx]];

      attachmentDescriptions.push_back(
        vkiAttachmentDescription(image->format,
                                 image->samples,
                                 loadStoreOp.loadOp,
                                 loadStoreOp.storeOp,
                                 loadStoreOp.stencilLoadOp,
                                 loadStoreOp.stencilStoreOp,
                                 image->ops[idx].layout,
                                 image->ops[idx].layout));

      VkAttachmentReference ref = {
        static_cast<uint32_t>(attachmentDescriptions.size() - 1), op.layout
      };

      switch (op.usage) {
        case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
          colorAttachmentRefs.push_back(ref);
          break;
        case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
          depthStencilAttachmentRefs.push_back(ref);
          break;
        case VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
          inputAttachmentRefs.push_back(ref);
          break;

        default:
          ASSERT_TRUE(false);
          break;
      }
    }

    ASSERT_TRUE(depthStencilAttachmentRefs.size() <= 1);

    VkSubpassDescription subpassDesc =
      vkiSubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
                            static_cast<uint32_t>(inputAttachmentRefs.size()),
                            inputAttachmentRefs.data(),
                            static_cast<uint32_t>(colorAttachmentRefs.size()),
                            colorAttachmentRefs.data(),
                            nullptr,
                            depthStencilAttachmentRefs.size() == 0
                              ? nullptr
                              : depthStencilAttachmentRefs.data(),
                            0,
                            nullptr);

    auto renderPassCreateInfo = vkiRenderPassCreateInfo(
      static_cast<uint32_t>(attachmentDescriptions.size()),
      attachmentDescriptions.data(),
      1,
      &subpassDesc,
      0,
      nullptr);

    ASSERT_VK_SUCCESS(
      vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass));
  }

  VkFramebuffer GetCurrentFrameBuffer(
    VkDevice device,
    const std::vector<VkImageView>& physicalAttachments)
  {
    auto iter = framebuffers.find(physicalAttachments);

    if (iter != framebuffers.end()) {
      return (*iter).second;
    } else {
      VkFramebuffer framebuffer = VK_NULL_HANDLE;
      VkFramebufferCreateInfo createInfo = vkiFramebufferCreateInfo(
        renderPass,
        static_cast<uint32_t>(physicalAttachments.size()),
        physicalAttachments.data(),
        fbWidth,
        fbHeight,
        fbLayers);

      ASSERT_VK_SUCCESS(
        vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffer));

      framebuffers.insert({ physicalAttachments, framebuffer });
      return framebuffer;
    }
  }

  void RecordCommands(RenderGraph* graph,
                      VkDevice device,
                      VkCommandBuffer cmdBuffer)
  {

    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;

    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkImageView> physicalAttachments;

    for (auto& name : attachmentNames) {
      auto& idx = images.find(name)->second;

      VirtualImage* vImage = graph->GetVirtualImage(name);
      PhysicalImage* pImage = graph->GetPhysicalImage(name);

      auto& op = vImage->ops[idx];
      auto barrierIdx = vImage->barrierIndices[idx];

      if (barrierIdx != -1) {
        auto barrier = vImage->barriers[barrierIdx];
        srcStage |= barrier.srcStage;
        dstStage |= barrier.dstStage;

        VkImageMemoryBarrier imageMemoryBarrier =
          vkiImageMemoryBarrier(barrier.srcMask,
                                barrier.dstMask,
                                barrier.oldLayout,
                                barrier.newLayout,
                                VK_QUEUE_FAMILY_IGNORED,
                                -1,
                                pImage->image,
                                vImage->subresourceRange);

        imageMemoryBarriers.push_back(imageMemoryBarrier);
      }

      physicalAttachments.push_back(pImage->view);
    }

    VkRenderPassBeginInfo renderPassInfo =
      vkiRenderPassBeginInfo(renderPass,
                             GetCurrentFrameBuffer(device, physicalAttachments),
                             { { 0, 0 }, { fbWidth, fbHeight } },
                             static_cast<uint32_t>(clearValuesFlat.size()),
                             clearValuesFlat.data());

    vkCmdPipelineBarrier(cmdBuffer,
                         srcStage,
                         dstStage,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         static_cast<uint32_t>(imageMemoryBarriers.size()),
                         imageMemoryBarriers.data());

    vkCmdBeginRenderPass(
      cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    OnRecordCommands(cmdBuffer);
    vkCmdEndRenderPass(cmdBuffer);
  }

  // Before VkRenderPass is created. Chance to add operations to VirtualImages.
  virtual void OnBeforeBuild(RenderGraph* graph) {}
  // After VkRenderPass is created. Chance to create resources that depend on
  // VkRenderPass.
  virtual void OnAfterBuild(RenderGraph* graph) {}
  // Called every frame. Change to issue actual work to the active command
  // buffer.
  virtual void OnRecordCommands(VkCommandBuffer cmdBuffer) {}
};

struct ExampleRenderPass : RenderPass
{
  ExampleRenderPass(VkDevice device, DeviceProps deviceProps)
    : RenderPass()
    , device(device)
    , deviceProps(deviceProps)
  {}

  VkDevice device;
  DeviceProps deviceProps;

  const int DYN_VERTEX_BUFFER_SIZE = 1024 * 1024 * 2;
  const int DYN_VERTEX_BUFFER_PARTITION_SIZE = DYN_VERTEX_BUFFER_SIZE / 2;

  GraphicsPipeline* pipeline;
  VkShaderModule vshader;
  VkShaderModule fshader;

  struct Buffer
  {
    VkBuffer buf;
    VkDeviceMemory mem;
  };

  Buffer vbuffer = {};
  uint8_t* vbufferHostMemory;

  void OnBeforeBuild(RenderGraph* graph) override
  {
    auto depthImage = graph->GetVirtualImage("depth");
    depthImage->ops.push_back(Operation::DepthStencilAttachment());
    images.insert({ "depth", depthImage->ops.size() - 1 });
    clearValues.insert({ "depth", { 0.f, 0 } });
    attachmentNames.push_back("depth");

    auto colorImage = graph->GetVirtualImage("color");
    colorImage->ops.push_back(Operation::ColorOutputAttachment());
    images.insert({ "color", colorImage->ops.size() - 1 });
    clearValues.insert({ "color", { 0.f, 0.f, 0.f } });
    attachmentNames.push_back("color");
  }

  struct VertexN
  {
    glm::vec3 pos;
    glm::vec3 normal;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
      VkVertexInputBindingDescription bindingDescription = {};
      bindingDescription.binding = 0;
      bindingDescription.stride = sizeof(VertexN);
      bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription>
    GetAttributeDescriptions()
    {
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

      attributeDescriptions[0].binding = 0;
      attributeDescriptions[0].location = 0;
      attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      attributeDescriptions[0].offset = offsetof(VertexN, pos);

      attributeDescriptions[1].binding = 0;
      attributeDescriptions[1].location = 1;
      attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      attributeDescriptions[1].offset = offsetof(VertexN, normal);
      return attributeDescriptions;
    }
  };

  std::string LoadFile(const char* _filename)
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

  VkShaderModule LoadShaderModule(VkDevice device, const char* filename)
  {
    std::string buff = LoadFile(filename);
    auto result = vkuCreateShaderModule(
      device, buff.size(), (uint32_t*)buff.data(), nullptr);
    ASSERT_VK_VALID_HANDLE(result);
    return result;
  }

  void OnAfterBuild(RenderGraph* graph) override
  {
    auto colorImage = graph->GetVirtualImage("color");

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
        .SetVertexBindings({ VertexN::GetBindingDescription() })
        .SetVertexAttributes(VertexN::GetAttributeDescriptions())
        .SetDescriptorSetLayouts(
          { { vkiDescriptorSetLayoutBinding(0,
                                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            1,
                                            VK_SHADER_STAGE_VERTEX_BIT,
                                            nullptr) } })
        .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetViewports({ { 0.0f,
                          0.0f,
                          (float)colorImage->extent.width,
                          (float)colorImage->extent.height,
                          0.0f,
                          1.0f } })
        .SetScissors(
          { { { 0, 0 },
              { colorImage->extent.width, colorImage->extent.height } } })
        .SetColorBlendAttachments({ colorBlendAttachment })
        .SetDepthWriteEnable(VK_TRUE)
        .SetDepthTestEnable(VK_TRUE)
        .SetRenderPass(renderPass)
        .Build();

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
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };
    memcpy(vbufferHostMemory, verts.data(), verts.size() * sizeof(float));
  }

  void OnRecordCommands(VkCommandBuffer cmdBuffer) override
  {
    VkDeviceSize vbufferOffset = 0;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vbuffer.buf, &vbufferOffset);
    vkCmdBindPipeline(
      cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
  }
};

struct ExampleRenderGraph : RenderGraph
{
  ExampleRenderGraph(VulkanBase* base)
    : RenderGraph(base->device)
    , base(base)
  {}

  void OnSetupVirtualImages() override
  {
    swapchain = new Swapchain(base->device, base->deviceProps, base->surface);

    VirtualImage* depth = new VirtualImage;
    depth->extent = { swapchain->extent.width, swapchain->extent.height, 1 };
    depth->format = VK_FORMAT_D32_SFLOAT;
    depth->samples = VK_SAMPLE_COUNT_1_BIT;
    depth->layers = 1;
    depth->levels = 1;
    depth->subresourceRange = {
      vkuGetImageAspectFlags(depth->format), 0, depth->levels, 0, depth->layers
    };

    AddVirtualImage("depth", depth);

    VirtualImage* vSwapchain = new VirtualImage;
    vSwapchain->extent = { swapchain->extent.width,
                           swapchain->extent.height,
                           1 };
    vSwapchain->format = swapchain->format.format;
    vSwapchain->samples = VK_SAMPLE_COUNT_1_BIT;
    vSwapchain->layers = 1;
    vSwapchain->levels = 1;
    vSwapchain->subresourceRange = { vkuGetImageAspectFlags(vSwapchain->format),
                                     0,
                                     depth->levels,
                                     0,
                                     depth->layers };

    AddVirtualImage("color", vSwapchain);
  }

  void OnSetupRenderPasses() override
  {
    AddRenderPass("examplePass",
                  new ExampleRenderPass(base->device, base->deviceProps));
  }

  void OnSetupPhysicalImages() override
  {
    physicalImages["depth"] = GetVirtualImage("depth")->CreatePhysicalImage(
      base->device, base->deviceProps.memProps);

    swapchain->CreatePhysicalSwapchain(GetVirtualImage("color")->usage);
  }

  void Render()
  {
    // This is called every frame.
    // 1. We need to resolve virtual images to physical images (e.g. swapchain
    // images).
    // 2. We need to rebuild barriers because physical images have a state that
    // is not the same at the beginning of every frame, e.g. layout.

    // Some of the work we are doing here should be handled differently, e.g.
    // through OnResolveVirtualImages callback.

    physicalImages["color"] =
      swapchain->AcquireImage(base->imageAvailableSemaphore);
    for (auto& kv : images) {
      BuildBarriers(kv.second, physicalImages[kv.first]);
    }

    auto cmdBuffer = base->NextCmdBuffer();

    VkCommandBufferBeginInfo beginInfo = vkiCommandBufferBeginInfo(nullptr);
    ASSERT_VK_SUCCESS(vkBeginCommandBuffer(cmdBuffer.cmdBuffer, &beginInfo));

    for (auto& pass : passes) {
      pass.second->RecordCommands(this, base->device, cmdBuffer.cmdBuffer);
    }

    auto barrier =
      vkiImageMemoryBarrier(0,
                            0,
                            GetVirtualImage("color")->ops.back().layout,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_QUEUE_FAMILY_IGNORED,
                            -1,
                            physicalImages["color"]->image,
                            GetVirtualImage("color")->subresourceRange);

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
                                            &base->imageAvailableSemaphore,
                                            &waitStage,
                                            1,
                                            &cmdBuffer.cmdBuffer,
                                            1,
                                            &base->renderFinishedSemaphore);

    physicalImages["color"]->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    physicalImages["color"]->stageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    physicalImages["color"]->accessFlags = 0;

    ASSERT_VK_SUCCESS(
      vkQueueSubmit(base->queue, 1, &submitInfo, cmdBuffer.fence));
    swapchain->Present(base->queue, base->renderFinishedSemaphore);
  }

  VulkanBase* base = nullptr;
  Swapchain* swapchain = nullptr;
};
