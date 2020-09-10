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

struct Barrier
{
  VkPipelineStageFlags srcStage = {};
  VkPipelineStageFlags dstStage = {};
  VkAccessFlags srcMask = {};
  VkAccessFlags dstMask = {};
  VkImageLayout oldLayout = {};
  VkImageLayout newLayout = {};
};

struct AttachmentOperation
{
  VkAttachmentLoadOp loadOp = {};
  VkAttachmentStoreOp storeOp = {};
  VkAttachmentLoadOp stencilLoadOp = {};
  VkAttachmentStoreOp stencilStoreOp = {};
};

struct ExecutionContext;
struct Operation
{
  VkImageUsageFlags usage = {};

  VkPipelineStageFlags stageFlags = {};
  VkAccessFlags accessFlags = {};
  VkImageLayout layout = {};

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

  static Operation Sampled()
  {
    Operation op;
    op.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    op.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    op.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    op.accessFlags = VK_ACCESS_SHADER_READ_BIT;

    return op;
  }

  static Operation PresentSrc()
  {
    Operation op;
    op.usage = 0;
    op.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    op.stageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    op.accessFlags = 0;

    return op;
  }

  bool HasAttachmentUsageFlags() const
  {
    const VkImageUsageFlags mask = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return (usage & mask) > 0;
  }

  bool HasWriteFlags() const
  {
    const VkAccessFlags mask =
      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT |
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
      VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |
      VK_ACCESS_COMMAND_PROCESS_WRITE_BIT_NVX |
      VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
    return (accessFlags & mask) > 0;
  }
};

struct OperationRange
{
  Operation op;

  uint32_t start; // inclusive
  uint32_t end;   // exclusive
};

struct VirtualImage
{
  VkFormat format;
  VkExtent3D extent;
  uint32_t layers;
  uint32_t levels;
  VkSampleCountFlagBits samples;
  VkImageSubresourceRange subresourceRange;

  VkImageUsageFlags usage = 0;

  uint32_t AddOperation(Operation operation)
  {
    usage |= operation.usage;
    ops.push_back(operation);
    return ops.size() - 1;
  }

  Operation GetCurrentOp() { return ops[counter]; }

  void ResetCounter() { counter = 0; }
  void IncCounter() { counter += 1; }

  bool HasStencilFormat() const
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

  bool HasStencilOnlyFormat() const { return format == VK_FORMAT_S8_UINT; }

  AttachmentOperation GetAttachmentOp() const
  {
    AttachmentOperation attachmentOp = {};

    if (ops[counter].HasWriteFlags()) {
      attachmentOp.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentOp.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    } else {
      attachmentOp.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      attachmentOp.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    if (!ops[counter].HasWriteFlags() || counter == ops.size() - 1 ||
        ops[counter + 1].HasWriteFlags()) {
      attachmentOp.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachmentOp.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    } else {
      attachmentOp.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentOp.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    }

    attachmentOp.loadOp = HasStencilOnlyFormat()
                            ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                            : attachmentOp.loadOp;
    attachmentOp.stencilLoadOp = !HasStencilFormat()
                                   ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                   : attachmentOp.stencilLoadOp;

    attachmentOp.storeOp = HasStencilOnlyFormat()
                             ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                             : attachmentOp.storeOp;
    attachmentOp.stencilStoreOp = !HasStencilFormat()
                                    ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                    : attachmentOp.stencilStoreOp;

    return attachmentOp;
  }

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
    physicalImage->stageFlags =
      VK_PIPELINE_STAGE_HOST_BIT; // VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    physicalImage->accessFlags = 0;
    physicalImage->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto eventCreateInfo = vkiEventCreateInfo();
    vkCreateEvent(device, &eventCreateInfo, nullptr, &physicalImage->event);
    vkSetEvent(device, physicalImage->event);

    return physicalImage;
  }

  void BuildBarriers(PhysicalImage* physicalImage)
  {
    if (ops.size() == 0) {
      return;
    }
    std::vector<OperationRange> ranges = {
      { ops[0], 0u, static_cast<uint32_t>(ops.size()) }
    };

    for (uint32_t i = 1; i < ops.size(); ++i) {
      auto& currRange = ranges.back();
      if (ops[i].HasWriteFlags() || currRange.op.HasWriteFlags() ||
          ops[i].layout != currRange.op.layout) {
        // end current range here
        currRange.end = i;

        // begin new range
        ranges.push_back({ ops[i], i, static_cast<uint32_t>(ops.size()) });
      } else {
        // add to current range
        currRange.op.stageFlags |= ops[i].stageFlags;
        currRange.op.accessFlags |= ops[i].accessFlags;
      }
    }

    barriers.clear();

    for (uint32_t i = 0; i < ranges.size(); ++i) {
      Barrier barrier;

      if (i == 0) {
        barrier.srcStage = physicalImage->stageFlags;
        barrier.srcMask = physicalImage->accessFlags;
        barrier.oldLayout = physicalImage->layout;
      } else {
        barrier.srcStage = ranges[i - 1].op.stageFlags;
        barrier.srcMask = ranges[i - 1].op.accessFlags;
        barrier.oldLayout = ranges[i - 1].op.layout;
      }

      barrier.dstStage = ranges[i].op.stageFlags;
      barrier.dstMask = ranges[i].op.accessFlags;
      barrier.newLayout = ranges[i].op.layout;

      barriers.insert({ ranges[i].start, barrier });

      if (i == ranges.size() - 1) {
        physicalImage->stageFlags = barrier.dstStage;
        physicalImage->accessFlags = barrier.dstMask;
        physicalImage->layout = barrier.newLayout;
      }
    }
  }

  bool GetCurrentBarrier(Barrier* barrier)
  {
    auto iter = barriers.find(counter);
    if (iter == barriers.end()) {
      return false;
    }

    *barrier = (*iter).second;
    return true;
  }

private:
  std::vector<Operation> ops = {};
  uint32_t counter = 0;

  // Is there a better place to store these?
  std::map<uint32_t, Barrier> barriers = {};
};

struct RenderGraphWorkUnit;
struct RenderGraph
{
  RenderGraph(VulkanBase* base, Swapchain* swapchain)
    : base(base)
    , swapchain(swapchain)
  {}

  VkDevice GetDevice() { return base->device; }
  void AddWork(const std::string& name, RenderGraphWorkUnit* workUnit)
  {
    work.push_back(workUnit);
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

  virtual void OnSetupPhysicalImages() {}

  virtual void OnFrameResolvePhysicalImages() {}
  virtual void OnFrame() {}

  void RenderFrame();

protected:
  VulkanBase* base;
  Swapchain* swapchain;

  std::vector<RenderGraphWorkUnit*> work;
  std::map<std::string, VirtualImage*> images;
  std::map<std::string, PhysicalImage*> physicalImages;
};

struct RenderGraphWorkUnit
{
  std::vector<std::string> images;

  RenderGraphWorkUnit(RenderGraph* graph)
    : graph(graph)
  {}
  RenderGraph* graph;

  void RecordCommands(VkCommandBuffer cmdBuffer)
  {
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;

    events.clear();
    waitStages.clear();

    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkEvent> events;

    for (auto& name : images) {
      VirtualImage* vImage = graph->GetVirtualImage(name);
      PhysicalImage* pImage = graph->GetPhysicalImage(name);

      Barrier barrier = {};

      if (vImage->GetCurrentBarrier(&barrier)) {

        VkImageMemoryBarrier imageMemoryBarrier =
          vkiImageMemoryBarrier(barrier.srcMask,
                                barrier.dstMask,
                                barrier.oldLayout,
                                barrier.newLayout,
                                VK_QUEUE_FAMILY_IGNORED,
                                -1,
                                pImage->image,
                                vImage->subresourceRange);

        events.push_back(pImage->event);
        waitStages.push_back(barrier.dstStage);

        // if (pImage->eventSet) {
        vkCmdWaitEvents(cmdBuffer,
                        1,
                        &pImage->event,
                        barrier.srcStage,
                        barrier.dstStage,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &imageMemoryBarrier);
        //} else {
        //  srcStage |= barrier.srcStage;
        //  dstStage |= barrier.dstStage;
        //  imageMemoryBarriers.push_back(imageMemoryBarrier);
        //}

        // pImage->eventSet = true;
      }
    }

    if (imageMemoryBarriers.size()) {
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
    }

    OnRecordCommands(cmdBuffer);

    for (uint32_t i = 0; i < events.size(); ++i) {
      vkCmdSetEvent(cmdBuffer, events[i], waitStages[i]);
    }

    for (auto& name : images) {
      VirtualImage* vImage = graph->GetVirtualImage(name);
      vImage->IncCounter();
    }
  }

  virtual void OnRecordCommands(VkCommandBuffer) {}
  virtual void Build() {}

private:
  std::vector<VkEvent> events = {};
  std::vector<VkPipelineStageFlags> waitStages = {};
};

struct RenderPass : RenderGraphWorkUnit
{
  RenderPass(RenderGraph* graph)
    : RenderGraphWorkUnit(graph)
  {}
  std::map<std::string, VkClearValue> clearValues;

  std::vector<VkClearValue> clearValuesFlat;
  VkRenderPass renderPass;

  using Attachments = std::vector<VkImageView>;
  std::map<Attachments, VkFramebuffer> framebuffers;

  uint32_t fbWidth, fbHeight, fbLayers;

  void Build() override
  {
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    std::vector<VkAttachmentReference> depthStencilAttachmentRefs;
    std::vector<VkAttachmentReference> inputAttachmentRefs;

    fbWidth = -1, fbHeight = -1, fbLayers = -1;

    for (auto& name : images) {
      VirtualImage* image = graph->GetVirtualImage(name);
      auto op = image->GetCurrentOp();

      if (!op.HasAttachmentUsageFlags()) {
        continue;
      }

      fbWidth = std::min(fbWidth, image->extent.width);
      fbHeight = std::min(fbHeight, image->extent.height);
      fbLayers = std::min(fbLayers, image->layers);

      clearValuesFlat.push_back(clearValues.find(name)->second);

      AttachmentOperation attachmentOp = image->GetAttachmentOp();

      attachmentDescriptions.push_back(
        vkiAttachmentDescription(image->format,
                                 image->samples,
                                 attachmentOp.loadOp,
                                 attachmentOp.storeOp,
                                 attachmentOp.stencilLoadOp,
                                 attachmentOp.stencilStoreOp,
                                 op.layout,
                                 op.layout));

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

      image->IncCounter();
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

    ASSERT_VK_SUCCESS(vkCreateRenderPass(
      graph->GetDevice(), &renderPassCreateInfo, nullptr, &renderPass));

    OnBuildDone(graph);
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

  void OnRecordCommands(VkCommandBuffer cmdBuffer)
  {
    std::vector<VkImageView> physicalAttachments;

    for (auto& name : images) {
      VirtualImage* vImage = graph->GetVirtualImage(name);
      PhysicalImage* pImage = graph->GetPhysicalImage(name);

      auto op = vImage->GetCurrentOp();

      if (!op.HasAttachmentUsageFlags()) {
        continue;
      }

      physicalAttachments.push_back(pImage->view);
    }

    VkRenderPassBeginInfo renderPassInfo = vkiRenderPassBeginInfo(
      renderPass,
      GetCurrentFrameBuffer(graph->GetDevice(), physicalAttachments),
      { { 0, 0 }, { fbWidth, fbHeight } },
      static_cast<uint32_t>(clearValuesFlat.size()),
      clearValuesFlat.data());

    vkCmdBeginRenderPass(
      cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    OnRecordRenderPassCommands(cmdBuffer);
    vkCmdEndRenderPass(cmdBuffer);
  }

  // After VkRenderPass is created. Chance to create resources that depend on
  // VkRenderPass.
  virtual void OnBuildDone(RenderGraph* graph) {}
  virtual void OnRecordRenderPassCommands(VkCommandBuffer) {}
};
