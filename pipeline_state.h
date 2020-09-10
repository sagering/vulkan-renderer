#pragma once

#include <vulkan\vulkan.h>

struct PipelineState
{
  struct BlendState
  {
    static const uint32_t MAX_NUM_COLOR_BLEND_ATTACHMENTS = 8;

    VkBool32 logicOpEnable = VK_FALSE;
    VkLogicOp logicOp = {};
    VkPipelineColorBlendAttachmentState
      colorBlendAttachments[MAX_NUM_COLOR_BLEND_ATTACHMENTS] = {};
    uint32_t colorBlendAttachmentCount = 0;
    float blendConstants[4] = {};

    BlendState()
    {
      for (uint32_t i = 0; i < MAX_NUM_COLOR_BLEND_ATTACHMENTS; ++i) {
        colorBlendAttachments[i].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      }
    }
  } blend = {};

  struct DepthStencilState
  {
    VkBool32 depthTestEnable = VK_FALSE;
    VkBool32 depthWriteEnable = VK_FALSE;
    VkCompareOp depthCompareOp = {};
    VkBool32 depthBoundsTestEnable = VK_FALSE;
    VkBool32 stencilTestEnable = VK_FALSE;
    VkStencilOpState front = {};
    VkStencilOpState back = {};
    float minDepthBounds = 0.f;
    float maxDepthBounds = 0.f;
  } depthStencil = {};

  struct DynamicState
  {
    static const uint32_t MAX_NUM_DYNAMIC_STATES = 8;

    VkDynamicState dynamicStates[MAX_NUM_DYNAMIC_STATES] = {};
    uint32_t dynamicStateCount = 0;
  } dynamic = {};

  struct InputAssemblyState
  {
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 primitiveRestartEnable = VK_FALSE;
  } inputAssembly = {};

  struct MultiSampleState
  {
    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkBool32 sampleShadingEnable = VK_FALSE;
    float minSampleShading = 1.f;
    VkSampleMask sampleMask = {};
    VkBool32 alphaToCoverageEnable = VK_FALSE;
    VkBool32 alphaToOneEnable = VK_FALSE;
  } multiSample = {};

  struct RasterizationState
  {
    VkBool32 depthClampEnable = VK_FALSE;
    VkBool32 rasterizerDiscardEnable = VK_FALSE;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkBool32 depthBiasEnable = VK_FALSE;
    float depthBiasConstantFactor = 0.f;
    float depthBiasClamp = 0.f;
    float depthBiasSlopeFactor = 0.f;
    float lineWidth = 1.f;
  } rasterization = {};

  struct ShaderState
  {
    struct ShaderStage
    {
      struct Specialization
      {
        static const uint32_t MAX_ENTRY_POINT_NAME_LENGTH = 64; // includes \0
        static const uint32_t MAX_NUM_MAP_ENTRIES = 8;
        static const uint32_t MAX_DATA_SIZE = MAX_NUM_MAP_ENTRIES * 64;
        char entryPoint[MAX_ENTRY_POINT_NAME_LENGTH] = {};

        VkSpecializationMapEntry mapEntries[MAX_NUM_MAP_ENTRIES] = {};
        uint32_t mapEntryCount = 0;
        char data[MAX_DATA_SIZE] = {};
        size_t dataSize = 0;
      };

      static const uint32_t MAX_SHADER_NAME_LENGTH = 64; // includes \0

      VkShaderStageFlagBits stage = {};
      Specialization specialization = {};
      const char* shaderName = nullptr;
    };

    static const uint32_t MAX_NUM_SHADER_STAGES = 2;

    ShaderStage stages[MAX_NUM_SHADER_STAGES] = {};
    uint32_t stageCount = 0;
  } shader = {};

  struct TesselationState
  {
    uint32_t patchControlPoints = 0;
  } tesselation = {};

  struct VertexInputState
  {
    static const uint32_t MAX_NUM_VERTEX_BINDING_DESCRIPTIONS = 8;
    static const uint32_t MAX_NUM_VERTEX_INPUT_DESCRIPTIONS = 8;

    VkVertexInputBindingDescription
      vertexBindingDescriptions[MAX_NUM_VERTEX_BINDING_DESCRIPTIONS] = {};
    uint32_t vertexBindingDescriptionCount = 0;
    VkVertexInputAttributeDescription
      vertexAttributeDescriptions[MAX_NUM_VERTEX_INPUT_DESCRIPTIONS] = {};
    uint32_t vertexAttributeDescriptionCount = 0;
  } vertexInput = {};

  struct ViewportState
  {
    static const uint32_t MAX_NUM_VIEWPORTS = 8;
    static const uint32_t MAX_NUM_SCISSORS = 8;

    VkViewport viewports[MAX_NUM_VIEWPORTS] = {};
    uint32_t viewportCount = 0;
    VkRect2D scissors[MAX_NUM_SCISSORS] = {};
    uint32_t scissorCount = 0;
  } viewport = {};
};

using Flags = uint32_t;

enum VertexAttributeFlagBits
{
  POSITION = 0x00000001,
  NORMAL = 0x00000002,
  TEXTURE_COORD = 0x00000004,
  COLOR = 0x00000008,
};
using VertexAttributeFlags = Flags;

struct SimplifiedVertexInputState
{
  static const uint32_t MAX_NUM_VERTEX_BINDINGS = 4;
  VertexAttributeFlags attributeFlags[MAX_NUM_VERTEX_BINDINGS] = {};
  uint32_t attributeFlagsCount = 0;

  void Apply(PipelineState* pipelineState)
  {
    VkVertexInputAttributeDescription* attributeDescriptions =
      pipelineState->vertexInput.vertexAttributeDescriptions;
    uint32_t& attributeDescriptionCount =
      pipelineState->vertexInput.vertexAttributeDescriptionCount;

    VkVertexInputBindingDescription* bindingDescriptions =
      pipelineState->vertexInput.vertexBindingDescriptions;
    uint32_t& bindingDescriptionCount =
      pipelineState->vertexInput.vertexBindingDescriptionCount;

    for (uint32_t i = 0; i < attributeFlagsCount; ++i) {
      auto flags = attributeFlags[i];
      uint32_t offset = 0;

      if (flags & VertexAttributeFlagBits::POSITION) {
        attributeDescriptions[attributeDescriptionCount].binding = i;
        attributeDescriptions[attributeDescriptionCount].location =
          attributeDescriptionCount;
        attributeDescriptions[attributeDescriptionCount].format =
          VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[attributeDescriptionCount].offset = offset;

        attributeDescriptionCount += 1;
        offset += sizeof(float) * 3;
      }

      if (flags & VertexAttributeFlagBits::NORMAL) {
        attributeDescriptions[attributeDescriptionCount].binding = i;
        attributeDescriptions[attributeDescriptionCount].location =
          attributeDescriptionCount;
        attributeDescriptions[attributeDescriptionCount].format =
          VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[attributeDescriptionCount].offset = offset;

        attributeDescriptionCount += 1;
        offset += sizeof(float) * 3;
      }

      if (flags & VertexAttributeFlagBits::TEXTURE_COORD) {
        attributeDescriptions[attributeDescriptionCount].binding = i;
        attributeDescriptions[attributeDescriptionCount].location =
          attributeDescriptionCount;
        attributeDescriptions[attributeDescriptionCount].format =
          VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[attributeDescriptionCount].offset = offset;

        attributeDescriptionCount += 1;
        offset += sizeof(float) * 2;
      }

      if (flags & VertexAttributeFlagBits::COLOR) {
        attributeDescriptions[attributeDescriptionCount].binding = i;
        attributeDescriptions[attributeDescriptionCount].location =
          attributeDescriptionCount;
        attributeDescriptions[attributeDescriptionCount].format =
          VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[attributeDescriptionCount].offset = offset;

        attributeDescriptionCount += 1;
        offset += sizeof(float) * 4;
      }

      bindingDescriptions[bindingDescriptionCount].binding = i;
      bindingDescriptions[bindingDescriptionCount].stride =
        offset; // assumes tightly packed
      bindingDescriptions[bindingDescriptionCount].inputRate =
        VK_VERTEX_INPUT_RATE_VERTEX; // just always assume this for now
      bindingDescriptionCount += 1;
    }
  }
};
