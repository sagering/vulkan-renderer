#pragma once

#include <vulkan/vulkan.h>

#include <map>
#include <vector>

#include "pipeline_state.h"

class Pipeline
{

public:
  Pipeline(VkDevice device,
           PipelineState state,
           VkRenderPass renderPass,
           uint32_t subpass)
    : device(device)
    , state(state)
    , renderPass(renderPass)
    , subpass(subpass)
  {}

  void Compile();
  void Bind(VkCommandBuffer cmdBuffer)
  {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  }

  void BindDescriptorSets(VkCommandBuffer cmdBuffer,
                          uint32_t firstSet,
                          uint32_t descriptorSetCount,
                          VkDescriptorSet* descriptorSets,
                          uint32_t dynamicOffsetCount,
                          uint32_t* dynamicOffsets)
  {
    vkCmdBindDescriptorSets(cmdBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            firstSet,
                            descriptorSetCount,
                            descriptorSets,
                            dynamicOffsetCount,
                            dynamicOffsets);
  }

  struct ShaderLayout
  {
    static const uint32_t MAX_NUM_DESCRIPTOR_SET_LAYOUT_BINDINGS = 16;
    static const uint32_t MAX_NUM_PUSH_CONSTANT_RANGES = 1;

    struct DescriptorSetLayoutBinding
    {
      uint32_t set = 0;
      VkDescriptorSetLayoutBinding binding = {};
    };

    DescriptorSetLayoutBinding
      bindings[MAX_NUM_DESCRIPTOR_SET_LAYOUT_BINDINGS] = {};
    uint32_t bindingCount = 0;

    VkPushConstantRange pushConstantRanges[MAX_NUM_PUSH_CONSTANT_RANGES] = {};
    uint32_t pushConstantRangeCount = 0;

    uint32_t inputLocationMask = 0;
    uint32_t outputLocationMask = 0;
    uint32_t inputAttachmentMask = 0;
  };

  VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set)
  {
    return descriptorSetLayouts[set];
  }

private:
  // passed into constructor
  VkDevice device;
  PipelineState state;
  VkRenderPass renderPass;
  uint32_t subpass;

  // reflection info
  std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> sets = {};
  VkPushConstantRange
    pushConstantRanges[ShaderLayout::MAX_NUM_PUSH_CONSTANT_RANGES] = {};
  uint32_t pushConstantRangeCount = 0;

  // create these on demand
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {};
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

  VkPipeline pipeline = VK_NULL_HANDLE;
};
