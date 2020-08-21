#pragma once

#include <vector>
#include <vulkan\vulkan_core.h>

#include "vk_init.h"
#include "vk_utils.h"

struct GraphicsPipeline
{
  VkDevice device = VK_NULL_HANDLE;
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {};
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  struct Builder;
  friend struct Builder;
  static Builder GetBuilder();

  GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
  GraphicsPipeline(const GraphicsPipeline&) = delete;

  ~GraphicsPipeline();

private:
  GraphicsPipeline() = default;
};

struct GraphicsPipeline::Builder
{
  VkDevice Device = VK_NULL_HANDLE;
  VkShaderModule VertexShader = VK_NULL_HANDLE;
  VkShaderModule FragmentShader = VK_NULL_HANDLE;
  std::vector<VkDescriptorSetLayout> SharedLayouts{};
  std::vector<std::vector<VkDescriptorSetLayoutBinding>> DescriptorSetLayouts{};
  std::vector<VkPushConstantRange> PushConstantRanges = {};
  std::vector<VkVertexInputBindingDescription> VertexBindings = {};
  std::vector<VkVertexInputAttributeDescription> VertexAttributes = {};
  VkPrimitiveTopology PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkBool32 PrimitiveRestartEnable = VK_FALSE;
  uint32_t PatchControlPoints = 0;
  std::vector<VkViewport> Viewports = {};
  std::vector<VkRect2D> Scissors = {};
  VkBool32 DepthClampEnable = VK_FALSE;
  VkBool32 RasterizerDiscardEnable = VK_FALSE;
  VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
  VkCullModeFlags CullMode = VK_CULL_MODE_NONE;
  VkFrontFace FrontFace = VK_FRONT_FACE_CLOCKWISE;
  VkBool32 DepthBiasEnable = VK_FALSE;
  float DepthBiasConstantFactor = 0.f;
  float DepthBiasClamp = 0.f;
  float DepthBiasSlopeFactor = 0.f;
  float LineWidth = 1.f;
  VkSampleCountFlagBits RasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkBool32 SampleShadingEnable = VK_FALSE;
  float MinSampleShading = 1.f;
  VkBool32 SampleMaskEnable = VK_FALSE;
  VkSampleMask SampleMask = 0;
  VkBool32 AlphaToCoverageEnable = VK_FALSE;
  VkBool32 AlphaToOneEnable = VK_FALSE;
  VkBool32 DepthTestEnable = VK_TRUE;
  VkBool32 DepthWriteEnable = VK_TRUE;
  VkCompareOp DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  VkBool32 DepthBoundsTestEnable = VK_FALSE;
  VkBool32 StencilTestEnable = VK_FALSE;
  VkStencilOpState Front = {};
  VkStencilOpState Back = {};
  float MinDepthBounds = 0.f;
  float MaxDepthBounds = 1.f;
  VkBool32 LogicOpEnable = VK_FALSE;
  VkLogicOp LogicOp = VK_LOGIC_OP_CLEAR;
  std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachments{};
  float BlendConstants[4] = { 0.f, 0.f, 0.f, 0.f };
  std::vector<VkDynamicState> DynamicStates = {};
  VkRenderPass RenderPass = VK_NULL_HANDLE;
  uint32_t Subpass = 0;
  VkPipeline BasePipelineHandle = VK_NULL_HANDLE;
  int32_t BasePipelineIndex = -1;

  // clang-format off
#define SETTER(type, ident)            \
	Builder& Set##ident(type ident) {  \
		this->ident = ident;           \
		return *this;                  \
	}

		SETTER(VkDevice, Device)
		SETTER(std::vector<VkDescriptorSetLayout>, SharedLayouts)
		SETTER(std::vector<std::vector<VkDescriptorSetLayoutBinding>>, DescriptorSetLayouts)
		SETTER(std::vector<VkPushConstantRange>, PushConstantRanges)
		SETTER(VkShaderModule, VertexShader)
		SETTER(VkShaderModule, FragmentShader)
		SETTER(std::vector<VkVertexInputBindingDescription>, VertexBindings)
		SETTER(std::vector<VkVertexInputAttributeDescription>, VertexAttributes)
		SETTER(VkPrimitiveTopology, PrimitiveTopology)
		SETTER(VkBool32, PrimitiveRestartEnable)
		SETTER(uint32_t, PatchControlPoints)
		SETTER(std::vector<VkViewport>, Viewports)
		SETTER(std::vector<VkRect2D>, Scissors)
		SETTER(VkBool32, DepthClampEnable)
		SETTER(VkBool32, RasterizerDiscardEnable)
		SETTER(VkPolygonMode, PolygonMode)
		SETTER(VkCullModeFlags, CullMode)
		SETTER(VkFrontFace, FrontFace)
		SETTER(VkBool32, DepthBiasEnable)
		SETTER(float, DepthBiasConstantFactor)
		SETTER(float, DepthBiasClamp)
		SETTER(float, DepthBiasSlopeFactor)
		SETTER(float, LineWidth)
		SETTER(VkSampleCountFlagBits, RasterizationSamples)
		SETTER(VkBool32, SampleShadingEnable)
		SETTER(float, MinSampleShading)
		SETTER(VkBool32, SampleMaskEnable)
		SETTER(VkSampleMask, SampleMask)
		SETTER(VkBool32, AlphaToCoverageEnable)
		SETTER(VkBool32, AlphaToOneEnable)
		SETTER(VkBool32, DepthTestEnable)
		SETTER(VkBool32, DepthWriteEnable)
		SETTER(VkCompareOp, DepthCompareOp)
		SETTER(VkBool32, DepthBoundsTestEnable)
		SETTER(VkBool32, StencilTestEnable)
		SETTER(VkStencilOpState, Front)
		SETTER(VkStencilOpState, Back)
		SETTER(float, MinDepthBounds)
		SETTER(float, MaxDepthBounds)
		SETTER(VkBool32, LogicOpEnable)
		SETTER(VkLogicOp, LogicOp)
		SETTER(std::vector<VkPipelineColorBlendAttachmentState>, ColorBlendAttachments)
		SETTER(std::vector<VkDynamicState>, DynamicStates)
		SETTER(VkRenderPass, RenderPass)
		SETTER(uint32_t, Subpass)
		SETTER(VkPipeline, BasePipelineHandle)
		SETTER(int32_t, BasePipelineIndex)

#undef SETTER
  // clang-format on

  Builder& SetBlendConstants(float blendConstants[4])
  {
    BlendConstants[0] = blendConstants[0];
    BlendConstants[1] = blendConstants[1];
    BlendConstants[2] = blendConstants[2];
    BlendConstants[3] = blendConstants[3];
    return *this;
  }

  GraphicsPipeline* Builder::Build();
};
