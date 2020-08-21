#pragma once

#include <glm\glm.hpp>

#include "graphics_pipeline.h"
#include "vk_base.h"

struct Vertex
{
  glm::vec3 pos;
  glm::vec2 uv;

  static VkVertexInputBindingDescription GetBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
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
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, uv);
    return attributeDescriptions;
  }
};

struct SimpleVertex
{
  glm::vec3 pos;

  static VkVertexInputBindingDescription GetBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(SimpleVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions()
  {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(SimpleVertex, pos);
    return attributeDescriptions;
  }
};

struct Renderer : VulkanBase
{
public:
  Renderer(VulkanWindow* window);
  ~Renderer();

  void pushVertices(const std::vector<float>&);

  void drawFrame();

private:
  virtual void OnSwapchainReinitialized();

  const int DYN_VERTEX_BUFFER_SIZE = 1024 * 1024 * 2;
  const int DYN_VERTEX_BUFFER_PARTITION_SIZE = DYN_VERTEX_BUFFER_SIZE / 2;
  uint32_t curDynVertexBufferPartition = 0;
  uint32_t totalNumVertices = 0;

  GraphicsPipeline* pipeline;
  VkShaderModule vshader;
  VkShaderModule fshader;

  VkBuffer vbuffer;
  VkDeviceMemory vbufferMemory;

  uint8_t* vbufferHostMemory; // will leak

  VkImage depthStencilImage = VK_NULL_HANDLE;
  VkImageView depthStencilImageView = VK_NULL_HANDLE;
  VkDeviceMemory depthStencilImageMemory = {};

  std::vector<VkFramebuffer> framebuffers = {};

  VkRenderPass renderpass = VK_NULL_HANDLE;

  void recordCommandBuffer(uint32_t idx);

private:
  void createResources();
  void destroyResources();
};
