#include "graphics_pipeline.h"

GraphicsPipeline::~GraphicsPipeline()
{
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

  for (auto dsl : descriptorSetLayouts) {
    vkDestroyDescriptorSetLayout(device, dsl, nullptr);
  }
}

GraphicsPipeline::Builder
GraphicsPipeline::GetBuilder()
{
  return Builder();
}

GraphicsPipeline*
GraphicsPipeline::Builder::Build()
{
  GraphicsPipeline* graphicsPipeline = new GraphicsPipeline;

  graphicsPipeline->descriptorSetLayouts.resize(DescriptorSetLayouts.size());
  uint32_t i = 0;

  for (auto bindings : DescriptorSetLayouts) {
    auto info = vkiDescriptorSetLayoutCreateInfo(
      static_cast<uint32_t>(bindings.size()), bindings.data());
    vkCreateDescriptorSetLayout(
      Device, &info, nullptr, &graphicsPipeline->descriptorSetLayouts[i]);
    SharedLayouts.push_back(graphicsPipeline->descriptorSetLayouts[i]);
    ++i;
  }

  // --------------------------------------------------------------------------
  // PipelineLayout
  // --------------------------------------------------------------------------
  auto info = vkiPipelineLayoutCreateInfo(
    static_cast<uint32_t>(SharedLayouts.size()),
    SharedLayouts.data(),
    static_cast<uint32_t>(PushConstantRanges.size()),
    PushConstantRanges.data());

  vkCreatePipelineLayout(
    Device, &info, nullptr, &graphicsPipeline->pipelineLayout);

  // --------------------------------------------------------------------------
  // Pipeline
  // --------------------------------------------------------------------------

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

  if (VertexShader != VK_NULL_HANDLE) {
    shaderStages.push_back(vkiPipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_VERTEX_BIT, VertexShader, "main", nullptr));
  }

  if (FragmentShader != VK_NULL_HANDLE) {
    shaderStages.push_back(vkiPipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT, FragmentShader, "main", nullptr));
  }

  VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo = {};
  PipelineColorBlendStateCreateInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  PipelineColorBlendStateCreateInfo.pNext = nullptr;
  PipelineColorBlendStateCreateInfo.flags = 0;
  PipelineColorBlendStateCreateInfo.logicOpEnable = LogicOpEnable;
  PipelineColorBlendStateCreateInfo.logicOp = LogicOp;
  PipelineColorBlendStateCreateInfo.attachmentCount =
    static_cast<uint32_t>(ColorBlendAttachments.size());
  PipelineColorBlendStateCreateInfo.pAttachments = ColorBlendAttachments.data();
  PipelineColorBlendStateCreateInfo.blendConstants[0] = BlendConstants[0];
  PipelineColorBlendStateCreateInfo.blendConstants[1] = BlendConstants[1];
  PipelineColorBlendStateCreateInfo.blendConstants[2] = BlendConstants[2];
  PipelineColorBlendStateCreateInfo.blendConstants[3] = BlendConstants[3];

  graphicsPipeline->pipeline = vkuCreateGraphicsPipeline(
    Device,
    std::initializer_list<VkPipelineShaderStageCreateInfo>(
      shaderStages.begin()._Ptr, shaderStages.end()._Ptr),
    vkiPipelineVertexInputStateCreateInfo(
      static_cast<uint32_t>(VertexBindings.size()),
      VertexBindings.data(),
      static_cast<uint32_t>(VertexAttributes.size()),
      VertexAttributes.data()),
    vkiPipelineInputAssemblyStateCreateInfo(PrimitiveTopology,
                                            PrimitiveRestartEnable),
    vkiPipelineTessellationStateCreateInfo(PatchControlPoints),
    vkiPipelineViewportStateCreateInfo(static_cast<uint32_t>(Viewports.size()),
                                       Viewports.data(),
                                       static_cast<uint32_t>(Scissors.size()),
                                       Scissors.data()),
    vkiPipelineRasterizationStateCreateInfo(DepthClampEnable,
                                            RasterizerDiscardEnable,
                                            PolygonMode,
                                            CullMode,
                                            FrontFace,
                                            DepthBiasEnable,
                                            DepthBiasConstantFactor,
                                            DepthBiasClamp,
                                            DepthBiasSlopeFactor,
                                            LineWidth),
    vkiPipelineMultisampleStateCreateInfo(RasterizationSamples,
                                          SampleShadingEnable,
                                          MinSampleShading,
                                          SampleMaskEnable ? &SampleMask
                                                           : nullptr,
                                          AlphaToCoverageEnable,
                                          AlphaToCoverageEnable),
    vkiPipelineDepthStencilStateCreateInfo(DepthTestEnable,
                                           DepthWriteEnable,
                                           DepthCompareOp,
                                           DepthBoundsTestEnable,
                                           StencilTestEnable,
                                           Front,
                                           Back,
                                           MinDepthBounds,
                                           MaxDepthBounds),
    PipelineColorBlendStateCreateInfo,
    vkiPipelineDynamicStateCreateInfo(
      static_cast<uint32_t>(DynamicStates.size()), DynamicStates.data()),
    graphicsPipeline->pipelineLayout,
    RenderPass,
    Subpass,
    VK_NULL_HANDLE,
    -1);

  graphicsPipeline->device = Device;

  return graphicsPipeline;
}
