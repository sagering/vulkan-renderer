#pragma once

// clang-format off
#include <vulkan\vulkan.h>
#include <GLFW\glfw3.h>
// clang-format on

#include <vector>

struct DeviceProps
{
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice handle = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties props = {};
  VkPhysicalDeviceFeatures features = {};
  VkPhysicalDeviceMemoryProperties memProps = {};
  std::vector<VkQueueFamilyProperties> queueFamilyProps = {};
  std::vector<VkSurfaceFormatKHR> surfaceFormats = {};
  std::vector<VkPresentModeKHR> presentModes = {};

  DeviceProps() = default;
  DeviceProps(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

  uint32_t GetGrahicsQueueFamiliyIdx();
  uint32_t GetPresentQueueFamiliyIdx();

  // surface capabilities are not static, e.g. currentExtent might change
  VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

  bool HasGraphicsSupport();
  bool HasPresentSupport();
};

struct PhysicalImage
{
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;

  VkPipelineStageFlags stageFlags;
  VkAccessFlags accessFlags;
  VkImageLayout layout;
};

struct Swapchain
{
  VkDevice device = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSurfaceFormatKHR format = {};
  VkExtent2D extent = {};
  VkPresentModeKHR presentMode = {};
  uint32_t imageCount = {};
  VkSurfaceTransformFlagBitsKHR transform = {};

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<PhysicalImage> images = {};

  Swapchain(VkDevice device, DeviceProps deviceProps, VkSurfaceKHR surface);

  void CreatePhysicalSwapchain(VkImageUsageFlags usage);

  Swapchain() = delete;
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain& other) = delete;

  ~Swapchain();

  PhysicalImage* AcquireImage(VkSemaphore imageAvailable);
  void Present(VkQueue queue, VkSemaphore renderFinished);

  uint32_t nextImageIdx = -1;
};

struct VulkanBase
{
  struct VulkanWindow
  {
    virtual VkSurfaceKHR CreateSurface(VkInstance instance) = 0;
    virtual VkExtent2D GetExtent() = 0;
  };

  VulkanWindow* window = nullptr;

  VkInstance instance = VK_NULL_HANDLE;
  std::vector<const char*> instanceLayers = {};
  std::vector<const char*> instanceExtensions = {};

  VkSurfaceKHR surface = VK_NULL_HANDLE;

  std::vector<const char*> deviceExtensions = {};
  VkDevice device = VK_NULL_HANDLE;
  DeviceProps deviceProps = {};
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool cmdPool = VK_NULL_HANDLE;

  struct CommandBuffer
  {
    VkCommandBuffer cmdBuffer;
    VkFence fence;
  };

  const uint32_t MAX_NUMBER_CMD_BUFFERS = 5;
  std::vector<VkCommandBuffer> commandBuffers = {};
  std::vector<VkFence> fences = {};
  uint32_t nextCmdBufferIdx = 0;

  VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
  VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  CommandBuffer NextCmdBuffer();

  VulkanBase(VulkanWindow* window);
  ~VulkanBase();

private:
  void CreateResources();
  void DestroyResources();
};
