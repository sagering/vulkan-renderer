#pragma once

#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
#include <vulkan\vulkan.h>

#include "vk_init.h"
#include "vk_utils.h"

struct Operation
{
  VkImageUsageFlags usage = 0;

  VkPipelineStageFlags stageFlags = 0;
  VkAccessFlags accessFlags = 0;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

  Operation() { id = nextId++; }

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

  bool HasAttachmentUsageFlags() const
  {
    const VkImageUsageFlags mask = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return (usage & mask) > 0;
  }

  enum
  {
    PASS_UNINITIALIZED = 0xffffffff,
  };

  static Operation ColorOutputAttachment()
  {
    Operation op;
    op.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    op.stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    op.accessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    op.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return op;
  }

  static Operation DepthStencilAttachment()
  {
    Operation op;
    op.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    op.stageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    op.accessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    op.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return op;
  }

  static Operation Sampled()
  {
    Operation op;
    op.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    op.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    op.accessFlags = VK_ACCESS_SHADER_READ_BIT;
    op.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return op;
  }

  static Operation PresentSrc()
  {
    Operation op;
    op.usage = 0;
    op.stageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    op.accessFlags = 0;
    op.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return op;
  }

  uint32_t id = 0;
  uint32_t renderPass = PASS_UNINITIALIZED;
  uint32_t subpass = PASS_UNINITIALIZED;

  static uint32_t nextId;
};

struct OperationRange
{
  Operation op;

  uint32_t start; // inclusive
  uint32_t end;   // exclusive
};

struct SplitBarrier
{
  VkPipelineStageFlags stageFlags = 0;
  VkAccessFlags accessFlags = 0;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct ImageBarrier
{
  SplitBarrier first;
  SplitBarrier second;
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
    physicalImage->view = view;
    physicalImage->stageFlags =
      VK_PIPELINE_STAGE_HOST_BIT; // VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    physicalImage->accessFlags = 0;
    physicalImage->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto eventCreateInfo = vkiEventCreateInfo();
    vkCreateEvent(device, &eventCreateInfo, nullptr, &physicalImage->event);

    return physicalImage;
  }
};

struct RenderGraph;
struct RenderPass;
struct Subpass
{
  std::map<std::string, Operation> imageOps = {};

  void SetOperation(const std::string& name, Operation op)
  {
    imageOps[name] = op;
  }

  RenderGraph* graph = nullptr;
  RenderPass* renderPass = nullptr;
  uint32_t subpass = 0;

  virtual void RecordCmds(VkCommandBuffer cmdBuffer) {}
  virtual void OnBakeDone() {}
};

struct RenderPass
{
  std::map<std::string, std::vector<Operation>> imageOps = {};

  std::vector<Subpass*> subpasses = {};
  std::map<std::pair<uint32_t, uint32_t>, VkSubpassDependency>
    subpassDependencies = {};

  VkRenderPass renderPass = VK_NULL_HANDLE;

  std::vector<VkClearValue> clearValues = {};
  VkRect2D renderArea = {};

  void AddSubpass(Subpass* subpass) { subpasses.push_back(subpass); }
};

struct RenderGraph
{
  // TODO: handling of externally acquired/used images, e.g. swapchain images
  //       problems: 1. synchronization
  //                 2. layout for external usage before and after might be
  //                 different

  std::map<std::string, std::vector<Operation>> imageOps = {};
  std::map<std::string, std::vector<OperationRange>> imageRanges = {};
  std::map<uint32_t, SplitBarrier> setEvents = {};
  std::map<uint32_t, ImageBarrier> waitEvents = {};

  std::vector<RenderPass*> renderPasses = {};

  std::map<std::string, VirtualImage*> vis = {};
  std::map<std::string, PhysicalImage*> pis = {};
  std::map<std::vector<PhysicalImage*>, VkFramebuffer> framebuffers = {};

  std::map<std::string, VkImageLayout> outputs = {};

  void AddVirtualImage(const std::string& name, VirtualImage* vi)
  {
    vis[name] = vi;
  }

  void AddRenderPass(RenderPass* renderPass)
  {
    renderPasses.push_back(renderPass);
  }

  void RecordCmds(
    VkDevice device,
    VkCommandBuffer cmdBuffer) // device needed until we have a better solution
                               // for handling framebuffers
  {
    {
      // TODO: move these barrier closer to the actual first usage of the image
      VkPipelineStageFlags srcStage = 0;
      VkPipelineStageFlags dstStage = 0;

      std::vector<VkImageMemoryBarrier> imageMemoryBarriers = {};
      for (auto const& kv : imageRanges) {
        auto const& name = kv.first;
        auto const& ranges = kv.second;

        auto pi = pis[name];
        auto vi = vis[name];

        auto barrier = vkiImageMemoryBarrier(pi->accessFlags,
                                             ranges.front().op.accessFlags,
                                             pi->layout,
                                             ranges.front().op.layout,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             pi->image,
                                             vi->subresourceRange);
        imageMemoryBarriers.push_back(barrier);

        srcStage |= pi->stageFlags;
        dstStage |= ranges.front().op.stageFlags;
      }

      if (imageMemoryBarriers.size() > 0) {
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
    }

    for (uint32_t i = 0; i < renderPasses.size(); ++i) {
      std::vector<VkEvent> events = {};
      std::vector<VkPipelineStageFlags> stages = {};

      // TODO: find better solution for handling framebuffers

      std::vector<PhysicalImage*> physicalAttachments = {};
      for (auto const& kv : renderPasses[i]->imageOps) {
        if (std::any_of(
              kv.second.begin(), kv.second.end(), [](const Operation& op) {
                return op.HasAttachmentUsageFlags();
              })) {
          physicalAttachments.push_back(pis[kv.first]);
        }
      }

      auto iter = framebuffers.find(physicalAttachments);
      if (iter == framebuffers.end()) {
        std::vector<VkImageView> views = {};
        for (auto const& pi : physicalAttachments) {
          views.push_back(pi->view);
        }

        VkFramebufferCreateInfo framebufferCreateInfo =
          vkiFramebufferCreateInfo(renderPasses[i]->renderPass,
                                   static_cast<uint32_t>(views.size()),
                                   views.data(),
                                   renderPasses[i]->renderArea.extent.width,
                                   renderPasses[i]->renderArea.extent.height,
                                   1);

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        ASSERT_VK_SUCCESS(vkCreateFramebuffer(
          device, &framebufferCreateInfo, nullptr, &framebuffer));
        framebuffers.insert({ physicalAttachments, framebuffer });
      }

      VkRenderPassBeginInfo renderPassBeginInfo = vkiRenderPassBeginInfo(
        renderPasses[i]->renderPass,
        framebuffers[physicalAttachments],
        renderPasses[i]->renderArea,
        static_cast<uint32_t>(renderPasses[i]->clearValues.size()),
        renderPasses[i]->clearValues.data());
      vkCmdBeginRenderPass(
        cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      for (uint32_t j = 0; j < renderPasses[i]->subpasses.size(); ++j) {
        for (auto const& kv : renderPasses[i]->subpasses[j]->imageOps) {
          auto const& name = kv.first;
          auto const& op = kv.second;

          auto vi = vis[name];
          auto pi = pis[name];

          {
            auto iter = waitEvents.find(op.id);
            if (iter != waitEvents.end()) {
              auto const& barrierPair = (*iter).second;
              VkImageMemoryBarrier imageMemoryBarrier =
                vkiImageMemoryBarrier(barrierPair.first.accessFlags,
                                      barrierPair.second.accessFlags,
                                      barrierPair.first.layout,
                                      barrierPair.second.layout,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      -1,
                                      pi->image,
                                      vi->subresourceRange);

              vkCmdWaitEvents(cmdBuffer,
                              1,
                              &pi->event,
                              barrierPair.first.stageFlags,
                              barrierPair.second.stageFlags,
                              0,
                              nullptr,
                              0,
                              nullptr,
                              1,
                              &imageMemoryBarrier);
            }
          }

          {
            auto iter = setEvents.find(op.id);
            if (iter != setEvents.end()) {
              // vkCmdSetEvent cannot be called inside a render pass,
              // but we can move it to the end of the render pass, because
              // it will not be waited on in the same render pass anyways.
              // Reason: Synchronization between subpasses is done via subpass
              // dependencies.
              events.push_back(pi->event);
              stages.push_back((*iter).second.stageFlags);
            }
          }
        }

        renderPasses[i]->subpasses[j]->RecordCmds(cmdBuffer);

        if (j < renderPasses[i]->subpasses.size() - 1) {
          vkCmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }
      }

      vkCmdEndRenderPass(cmdBuffer);

      // set events
      for (uint32_t j = 0; j < events.size(); ++j) {
        vkCmdSetEvent(cmdBuffer, events[j], stages[j]);
      }

      for (auto const& kv : imageRanges) {
        auto const& name = kv.first;
        auto const& ranges = kv.second;

        auto pi = pis[name];

        pi->stageFlags = ranges.back().op.stageFlags;
        pi->accessFlags = ranges.back().op.accessFlags;
        pi->layout = ranges.back().op.layout;
      }
    }
  }

  void Bake(VkDevice device)
  {
    for (uint32_t i = 0; i < renderPasses.size(); ++i) {
      for (uint32_t j = 0; j < renderPasses[i]->subpasses.size(); ++j) {
        renderPasses[i]->subpasses[j]->graph = this;
        renderPasses[i]->subpasses[j]->renderPass = renderPasses[i];
        renderPasses[i]->subpasses[j]->subpass = j;

        for (auto& kv : renderPasses[i]->subpasses[j]->imageOps) {
          auto const& name = kv.first;
          auto& op = kv.second;

          op.renderPass = i;
          op.subpass = j;

          renderPasses[i]->imageOps[name].push_back(op);
          imageOps[name].push_back(op);
        }
      }
    }

    // aggregate image usage
    for (auto const& kv : imageOps) {
      auto const& name = kv.first;

      auto vi = vis[name];

      for (auto const& op : kv.second) {
        vi->usage |= op.usage;
      }
    }

    OnCreatePhysicalImages();

    for (auto const& kv : imageOps) {
      auto const& name = kv.first;
      auto const& ops = kv.second;

      if (ops.size() == 0) {
        return;
      }

      // synchronization only necessary between ranges
      std::vector<OperationRange> ranges = {
        { ops[0], 0u, static_cast<uint32_t>(ops.size()) }
      };

      for (uint32_t j = 1; j < ops.size(); ++j) {
        auto& currRange = ranges.back();
        if (ops[j].HasWriteFlags() || currRange.op.HasWriteFlags() ||
            ops[j].layout != currRange.op.layout) {
          // end current range here
          currRange.end = j;

          // begin new range
          ranges.push_back({ ops[j], j, static_cast<uint32_t>(ops.size()) });
        } else {
          // add to current range
          currRange.op.stageFlags |= ops[j].stageFlags;
          currRange.op.accessFlags |= ops[j].accessFlags;
        }
      }

      imageRanges[name] = ranges;

      // if a renderpass crosses a range boundary, the ranges have to be
      // synchronized with subpass dependencies; we are using the Overlap
      // struct to track where the renderpass crosses a range boundary

      struct Overlap
      {
        uint32_t start = 0;
        uint32_t end = 0;
      };

      for (uint32_t j = 1; j < ranges.size(); ++j) {
        // TODO: synchronization with previous frame

        Overlap overlap = {};
        overlap.start = ranges[j - 1].end;
        overlap.end = ranges[j].start;

        if (ops[ranges[j - 1].end - 1].renderPass ==
            ops[ranges[j].start].renderPass) {
          overlap.start = ranges[j - 1].end - 1;
          overlap.end = ranges[j].start + 1;

          while (overlap.start - 1 >= ranges[j - 1].start &&
                 ops[overlap.start - 1].renderPass ==
                   ops[overlap.start].renderPass) {
            --overlap.start;
          }

          while (overlap.end < ranges[j].end &&
                 ops[overlap.end].renderPass == ops[overlap.start].renderPass) {
            ++overlap.end;
          }
        }

        if (overlap.start < overlap.end) {
          // NOTE: The spec says that images that are used as attachments in
          // one subpass are not allowed to be used as e.g.
          // VK_IMAGE_USAGE_SAMPLED_BIT in another subpass. We can only
          // transition the image layout from one attachment layout to
          // another attachment layout between subpasses (The target layout
          // of an image for a subpass can only be specified in the
          // ATTACHMENT REFERENCE, so there is no way to specify a target
          // layout for non attachment images.)

          // Valid cases for synchronization between subpasses:
          // - synchronization with and with layout transition for
          // attachments
          // - synchronization without layout transition for non attachments

          bool attachmentUsageConsistent =
            std::all_of(ops.begin() + overlap.start,
                        ops.begin() + overlap.end,
                        [&ops, &overlap](const Operation& op) {
                          return op.HasAttachmentUsageFlags() ==
                                 ops[overlap.start].HasAttachmentUsageFlags();
                        });

          ASSERT_TRUE(attachmentUsageConsistent);

          bool layoutConsistent =
            std::all_of(ops.begin() + overlap.start,
                        ops.begin() + overlap.end,
                        [&ops, &overlap](const Operation& op) {
                          return op.layout == ops[overlap.start].layout;
                        });

          ASSERT_TRUE(ops[overlap.start].HasAttachmentUsageFlags() ||
                      layoutConsistent);

          for (uint32_t k = overlap.start; k < ranges[j - 1].end; ++k) {
            for (uint32_t l = ranges[j].start; l < overlap.end; ++l) {

              uint32_t renderPass = ops[overlap.start].renderPass;

              std::cout << "INFO: subpass dependency for image " << name.c_str()
                        << " in render pass " << renderPass
                        << " between subpass " << k << " and subpass " << l
                        << std::endl;

              auto& dependency =
                renderPasses[renderPass]
                  ->subpassDependencies[{ ops[k].subpass, ops[l].subpass }];

              dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

              dependency.srcAccessMask |= ops[k].stageFlags;
              dependency.srcStageMask |= ops[k].accessFlags;
              dependency.srcSubpass = ops[k].subpass;

              dependency.dstStageMask |= ops[l].stageFlags;
              dependency.dstAccessMask |= ops[l].accessFlags;
              dependency.dstSubpass = ops[l].subpass;
            }
          }
        }

        // 1. no overlap: sets the one necessary set/wait
        // 2.    overlap: sets first of two necessary set/wait
        if (ranges[j - 1].start < overlap.start) {
          SplitBarrier first = {};
          SplitBarrier second = {};

          for (uint32_t k = ranges[j - 1].start; k < overlap.start; ++k) {
            first.stageFlags |= ops[k].stageFlags;
            first.accessFlags |= ops[k].accessFlags;
            first.layout = ops[k].layout;
          }

          for (uint32_t k = ranges[j].start; k < ranges[j].end; ++k) {
            second.stageFlags |= ops[k].stageFlags;
            second.accessFlags |= ops[k].accessFlags;
            second.layout = ops[k].layout;
          }

          std::cout << "INFO: set event for image " << name.c_str()
                    << std::endl;
          std::cout << "INFO: wait event for image " << name.c_str()
                    << std::endl;

          setEvents[ops[overlap.start - 1].id] = first;
          waitEvents[ops[ranges[j].start].id] = { first, second };
        }

        // 1. no overlap: nop
        // 2.    overlap: sets the second of the two necessary set/wait
        if (overlap.start < overlap.end && overlap.end < ranges[j].end) {
          SplitBarrier first = {};
          SplitBarrier second = {};

          for (uint32_t k = overlap.start; k < ranges[j - 1].end; ++k) {
            first.stageFlags |= ops[k].stageFlags;
            first.accessFlags |= ops[k].accessFlags;
            first.layout = ops[k].layout;
          }

          for (uint32_t k = overlap.end; k < ranges[j].end; ++k) {
            second.stageFlags |= ops[k].stageFlags;
            second.accessFlags |= ops[k].accessFlags;
            second.layout = ops[k].layout;
          }

          std::cout << "INFO: set event for image " << name.c_str()
                    << std::endl;
          std::cout << "INFO: wait event for image " << name.c_str()
                    << std::endl;

          setEvents[ops[overlap.end - 1].id] = first;
          waitEvents[ops[overlap.end].id] = { first, second };
        }
      }
    }

    for (uint32_t i = 0; i < renderPasses.size(); ++i) {
      std::vector<VkAttachmentDescription> attachmentDescriptions;
      std::map<std::string, uint32_t> attachmentIndices = {};
      std::vector<VkSubpassDescription> subpassDescriptions = {};

      for (auto const& kv : renderPasses[i]->imageOps) {
        auto const& name = kv.first;
        auto const& ops = kv.second;

        // if image is not used as an attachment, we do not need to include
        // it in the renderpass
        if (!std::any_of(ops.begin(), ops.end(), [](const Operation& op) {
              return op.HasAttachmentUsageFlags();
            })) {
          continue;
        }

        bool writeAccess =
          std::any_of(ops.begin(), ops.end(), [](const Operation& op) {
            return op.HasWriteFlags();
          });

        auto vi = vis[name];
        attachmentDescriptions.push_back(
          vkiAttachmentDescription(vi->format,
                                   vi->samples,
                                   writeAccess ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                               : VK_ATTACHMENT_LOAD_OP_LOAD,
                                   VK_ATTACHMENT_STORE_OP_STORE,
                                   writeAccess ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                               : VK_ATTACHMENT_LOAD_OP_LOAD,
                                   VK_ATTACHMENT_STORE_OP_STORE,
                                   ops.front().layout,
                                   ops.back().layout));

        attachmentIndices[name] = attachmentDescriptions.size() - 1;

        VkClearValue clearValue = vi->HasStencilFormat()
                                    ? VkClearValue{ 0.f, 0 }
                                    : VkClearValue{ 0.f, 0.f, 0.f };
        renderPasses[i]->clearValues.push_back(clearValue);

        renderPasses[i]->renderArea.extent.width =
          std::max(vi->extent.width, renderPasses[i]->renderArea.extent.width);
        renderPasses[i]->renderArea.extent.height = std::max(
          vi->extent.height, renderPasses[i]->renderArea.extent.height);
      }

      std::vector<std::vector<VkAttachmentReference>> colorAttachmentRefs = {};
      std::vector<std::vector<VkAttachmentReference>>
        depthStencilAttachmentRefs = {};
      std::vector<std::vector<VkAttachmentReference>> inputAttachmentRefs = {};

      for (uint32_t j = 0; j < renderPasses[i]->subpasses.size(); ++j) {

        colorAttachmentRefs.push_back({});
        depthStencilAttachmentRefs.push_back({});
        inputAttachmentRefs.push_back({});

        std::vector<VkAttachmentReference>& color = colorAttachmentRefs.back();
        std::vector<VkAttachmentReference>& depthStencil =
          depthStencilAttachmentRefs.back();
        std::vector<VkAttachmentReference>& input = inputAttachmentRefs.back();

        for (auto const& kv : renderPasses[i]->subpasses[j]->imageOps) {
          auto const& name = kv.first;
          auto const& op = kv.second;

          // if subpass does not use this image as attachment,
          // we can ignore it
          if (!op.HasAttachmentUsageFlags()) {
            continue;
          }

          VkAttachmentReference ref = { attachmentIndices[name], op.layout };

          switch (op.usage) {
            case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
              color.push_back(ref);
              break;
            case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
              depthStencil.push_back(ref);
              break;
            case VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
              input.push_back(ref);
              break;
            default:
              ASSERT_TRUE(false);
              break;
          }
        }

        ASSERT_TRUE(depthStencil.size() <= 1);

        VkSubpassDescription subpassDescription = vkiSubpassDescription(
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          static_cast<uint32_t>(input.size()),
          input.data(),
          static_cast<uint32_t>(color.size()),
          color.data(),
          nullptr,
          depthStencil.size() == 0 ? nullptr : depthStencil.data(),
          0,
          nullptr);

        subpassDescriptions.push_back(subpassDescription);
      }

      std::vector<VkSubpassDependency> dependencies = {};
      for (auto const& kv : renderPasses[i]->subpassDependencies) {
        dependencies.push_back(kv.second);
      }

      auto renderPassCreateInfo = vkiRenderPassCreateInfo(
        static_cast<uint32_t>(attachmentDescriptions.size()),
        attachmentDescriptions.data(),
        static_cast<uint32_t>(subpassDescriptions.size()),
        subpassDescriptions.data(),
        static_cast<uint32_t>(dependencies.size()),
        dependencies.data());

      ASSERT_VK_SUCCESS(vkCreateRenderPass(
        device, &renderPassCreateInfo, nullptr, &renderPasses[i]->renderPass));
    }

    for (uint32_t i = 0; i < renderPasses.size(); ++i) {
      for (uint32_t j = 0; j < renderPasses[i]->subpasses.size(); ++j) {
        renderPasses[i]->subpasses[j]->OnBakeDone();
      }
    }
  }

  virtual void OnCreatePhysicalImages() {}
};
