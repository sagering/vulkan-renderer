#include "pipeline.h"
#include "vk_utils.h"
#include <algorithm>
#include <map>
#include <set>
#include <spirv_cross\spirv_cross.hpp>
#include <string>
#include <vulkan\vulkan.h>

#pragma comment(lib, "spirv-cross-cored.lib")

std::string
ReadFile(const char* fileName)
{
  std::string buff;

  FILE* file = 0;
  fopen_s(&file, fileName, "rb");
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

uint32_t
ReadDescriptorCount(const spirv_cross::SPIRType& type)
{
  if (type.array.size() == 0) {
    return 1;
  }

  if (type.array.size() > 1) {
    ASSERT_TRUE(
      false); // vulkan only supports single array level for descriptors.
  }

  if (!type.array_size_literal.back()) {
    ASSERT_TRUE(false); // size cannot be statically resolved
  }

  if (type.array.back() == 0) {
    ASSERT_TRUE(false); // avoid bindless complexity
  }

  return type.array.back();
}

void
ReflectDescriptor(const spirv_cross::Compiler& comp,
                  const spirv_cross::Resource& res,
                  VkDescriptorType type,
                  VkShaderStageFlags stage,
                  Pipeline::ShaderLayout& layout)
{
  uint32_t set = comp.get_decoration(res.id, spv::DecorationDescriptorSet);
  uint32_t binding = comp.get_decoration(res.id, spv::DecorationBinding);
  uint32_t count = ReadDescriptorCount(comp.get_type(res.type_id));

  uint32_t& bindingCount = layout.bindingCount;
  ASSERT_TRUE(bindingCount <
              Pipeline::ShaderLayout::MAX_NUM_DESCRIPTOR_SET_LAYOUT_BINDINGS);
  layout.bindings[layout.bindingCount].set = set;
  layout.bindings[layout.bindingCount].binding =
    VkDescriptorSetLayoutBinding{ binding, type, count, stage, nullptr };
  ++layout.bindingCount;
}

void
ReflectLayout(const std::string& code,
              VkShaderStageFlags stage,
              Pipeline::ShaderLayout& layout)
{
  spirv_cross::Compiler comp((uint32_t*)code.data(),
                             code.size() / sizeof(uint32_t));

  auto resources = comp.get_shader_resources();

  // sampler2D
  for (auto& res : resources.sampled_images) {
    ReflectDescriptor(
      comp, res, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage, layout);
  }

  // texture2D or samplerBuffer
  for (auto& res : resources.separate_images) {
    auto type = comp.get_type(res.base_type_id);
    if (type.image.dim == spv::DimBuffer) {
      ReflectDescriptor(
        comp, res, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, stage, layout);
    } else {
      ReflectDescriptor(
        comp, res, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage, layout);
    }
  }

  // image2D or imageBuffer
  for (auto& res : resources.storage_images) {
    auto type = comp.get_type(res.base_type_id);
    if (type.image.dim == spv::DimBuffer) {
      ReflectDescriptor(
        comp, res, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, stage, layout);
    } else {
      ReflectDescriptor(
        comp, res, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage, layout);
    }
  }

  // sampler
  for (auto& res : resources.separate_samplers) {
    ReflectDescriptor(comp, res, VK_DESCRIPTOR_TYPE_SAMPLER, stage, layout);
  }

  // uniform UBO {}
  for (auto& res : resources.uniform_buffers) {
    ReflectDescriptor(
      comp, res, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage, layout);
  }

  // layout(push_constant) uniform Push
  if (resources.push_constant_buffers.size() > 0) {
    auto& res = resources.push_constant_buffers.back();

    layout.pushConstantRanges[0].offset = 0;
    layout.pushConstantRanges[0].size =
      comp.get_declared_struct_size(comp.get_type(res.type_id));
    layout.pushConstantRanges[0].stageFlags = stage;
    layout.pushConstantRangeCount += 1;
  }

  for (auto& res : resources.subpass_inputs) {
    layout.inputAttachmentMask |=
      1 << comp.get_decoration(res.id, spv::DecorationInputAttachmentIndex);
  }

  if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
    for (auto& res : resources.stage_inputs) {
      layout.inputLocationMask |=
        1 << comp.get_decoration(res.id, spv::DecorationLocation);
    }
  }

  if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
    for (auto& res : resources.stage_outputs) {
      layout.outputLocationMask |=
        1 << comp.get_decoration(res.id, spv::DecorationLocation);
    }
  }

  // buffer SSBO {}
  for (auto& res : resources.storage_buffers) {
    ReflectDescriptor(
      comp, res, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage, layout);
  }
}

void
Pipeline::Compile()
{
  const char*
    shaderNames[PipelineState::ShaderState::MAX_NUM_SHADER_STAGES] = {};

  for (uint32_t i = 0; i < state.shader.stageCount; ++i) {
    const char* name = state.shader.stages[i].shaderName;

    shaderNames[i] = name;
  }

  // TODO: check if pipeline layout exists

  ShaderLayout layouts[PipelineState::ShaderState::MAX_NUM_SHADER_STAGES] = {};
  VkShaderModule
    shaderModules[PipelineState::ShaderState::MAX_NUM_SHADER_STAGES] = {};

  for (uint32_t i = 0; i < state.shader.stageCount; ++i) {
    auto code = ReadFile(shaderNames[i]);
    ReflectLayout(code, state.shader.stages[i].stage, layouts[i]);

    // TODO: check if module exists

    shaderModules[i] = vkuCreateShaderModule(
      device, code.size(), (uint32_t*)code.data(), nullptr);
  }

  uint32_t last = std::numeric_limits<uint32_t>::min();

  for (uint32_t i = 0; i < state.shader.stageCount; ++i) {
    for (uint32_t j = 0; j < layouts[i].bindingCount; ++j) {
      auto set = layouts[i].bindings[j].set;
      auto binding = layouts[i].bindings[j].binding;
      last = std::max(last, set);
      sets[set].push_back(binding);
    }

    if (layouts[i].pushConstantRangeCount > 0) {
      pushConstantRanges[0].offset = layouts[i].pushConstantRanges[0].offset;
      pushConstantRanges[0].size = layouts[i].pushConstantRanges[0].size;
      pushConstantRanges[0].stageFlags |=
        layouts[i].pushConstantRanges[0].stageFlags;
      pushConstantRangeCount = 1;
    }
  }

  if (!sets.empty()) {
    for (uint32_t set = 0; set <= last; ++set) {
      auto iter = sets.find(set);

      // TOOD: check if set exists

      if (iter != sets.end()) {
        auto bindings = (*iter).second;
        auto createInfo = vkiDescriptorSetLayoutCreateInfo(
          static_cast<uint32_t>(bindings.size()), bindings.data());
        descriptorSetLayouts.push_back(VK_NULL_HANDLE);
        ASSERT_VK_SUCCESS(vkCreateDescriptorSetLayout(
          device, &createInfo, nullptr, &descriptorSetLayouts.back()));
      } else {
        // fill gaps
        sets[set] = {};
        auto createInfo = vkiDescriptorSetLayoutCreateInfo(0, nullptr);
        descriptorSetLayouts.push_back(VK_NULL_HANDLE);
        ASSERT_VK_SUCCESS(vkCreateDescriptorSetLayout(
          device, &createInfo, nullptr, &descriptorSetLayouts.back()));
      }
    }
  }

  {
    auto createInfo = vkiPipelineLayoutCreateInfo(
      static_cast<uint32_t>(descriptorSetLayouts.size()),
      descriptorSetLayouts.data(),
      pushConstantRangeCount,
      pushConstantRanges);

    ASSERT_VK_SUCCESS(
      vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout));
  }
  {
    VkPipelineShaderStageCreateInfo shaderStageCreateInfos
      [PipelineState::ShaderState::MAX_NUM_SHADER_STAGES] = {};

    for (uint32_t i = 0; i < state.shader.stageCount; ++i) {
      VkSpecializationInfo specializationInfo = {};
      specializationInfo.dataSize =
        state.shader.stages[i].specialization.dataSize;
      specializationInfo.pData = state.shader.stages[i].specialization.data;
      specializationInfo.mapEntryCount =
        state.shader.stages[i].specialization.mapEntryCount;
      specializationInfo.pMapEntries =
        state.shader.stages[i].specialization.mapEntries;

      shaderStageCreateInfos[i] =
        vkiPipelineShaderStageCreateInfo(state.shader.stages[i].stage,
                                         shaderModules[i],
                                         "main",
                                         &specializationInfo);
    }

    VkPipelineColorBlendStateCreateInfo blendStateCreateInfo = {};
    blendStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendStateCreateInfo.pNext = nullptr;
    blendStateCreateInfo.flags = 0;
    blendStateCreateInfo.logicOpEnable = state.blend.logicOpEnable;
    blendStateCreateInfo.logicOp = state.blend.logicOp;
    blendStateCreateInfo.attachmentCount =
      state.blend.colorBlendAttachmentCount;
    blendStateCreateInfo.pAttachments = state.blend.colorBlendAttachments;
    memcpy(&blendStateCreateInfo.blendConstants,
           &state.blend.blendConstants,
           sizeof(float) * 4);

    pipeline = vkuCreateGraphicsPipeline(
      device,
      shaderStageCreateInfos,
      state.shader.stageCount,
      vkiPipelineVertexInputStateCreateInfo(
        state.vertexInput.vertexBindingDescriptionCount,
        state.vertexInput.vertexBindingDescriptions,
        state.vertexInput.vertexAttributeDescriptionCount,
        state.vertexInput.vertexAttributeDescriptions),
      vkiPipelineInputAssemblyStateCreateInfo(
        state.inputAssembly.topology,
        state.inputAssembly.primitiveRestartEnable),
      vkiPipelineTessellationStateCreateInfo(
        state.tesselation.patchControlPoints),
      vkiPipelineViewportStateCreateInfo(state.viewport.viewportCount,
                                         state.viewport.viewports,
                                         state.viewport.scissorCount,
                                         state.viewport.scissors),
      vkiPipelineRasterizationStateCreateInfo(
        state.rasterization.depthClampEnable,
        state.rasterization.rasterizerDiscardEnable,
        state.rasterization.polygonMode,
        state.rasterization.cullMode,
        state.rasterization.frontFace,
        state.rasterization.depthBiasEnable,
        state.rasterization.depthBiasConstantFactor,
        state.rasterization.depthBiasClamp,
        state.rasterization.depthBiasSlopeFactor,
        state.rasterization.lineWidth),
      vkiPipelineMultisampleStateCreateInfo(
        state.multiSample.rasterizationSamples,
        state.multiSample.sampleShadingEnable,
        state.multiSample.minSampleShading,
        state.multiSample.sampleShadingEnable ? &state.multiSample.sampleMask
                                              : nullptr,
        state.multiSample.alphaToCoverageEnable,
        state.multiSample.alphaToCoverageEnable),
      vkiPipelineDepthStencilStateCreateInfo(
        state.depthStencil.depthTestEnable,
        state.depthStencil.depthWriteEnable,
        state.depthStencil.depthCompareOp,
        state.depthStencil.depthBoundsTestEnable,
        state.depthStencil.stencilTestEnable,
        state.depthStencil.front,
        state.depthStencil.back,
        state.depthStencil.minDepthBounds,
        state.depthStencil.maxDepthBounds),
      blendStateCreateInfo,
      vkiPipelineDynamicStateCreateInfo(state.dynamic.dynamicStateCount,
                                        state.dynamic.dynamicStates),
      pipelineLayout,
      renderPass,
      subpass,
      VK_NULL_HANDLE,
      -1);
  }
}
