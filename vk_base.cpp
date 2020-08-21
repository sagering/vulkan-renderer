#include "vk_base.h"

#include <algorithm> // find_if, find_first_of

#include "vk_init.h"
#include "vk_utils.h"

VulkanBase::VulkanBase(VulkanWindow* window)
  : window(window)
{
  CreateSwapchainIndependentResources();
  swapchain = new Swapchain(device, physicalDeviceProps, surface);
  CreateSwapchainDependentResources();
}

VulkanBase::~VulkanBase()
{
  DestroySwapchainDependentResources();
  delete swapchain;
  DestroySwapchainIndependentResources();
}

void
VulkanBase::Update()
{
  auto windowExtent = window->GetExtent();

  if (windowExtent.width != swapchain->imageExtent.width ||
      windowExtent.height != swapchain->imageExtent.height) {
    ReinitSwapchain();
  }
}

void
VulkanBase::ReinitSwapchain()
{
  vkDeviceWaitIdle(device);

  delete swapchain;
  swapchain = new Swapchain(device, physicalDeviceProps, surface);

  DestroySwapchainDependentResources();
  CreateSwapchainDependentResources();

  OnSwapchainReinitialized();
}

void
VulkanBase::CreateSwapchainIndependentResources()
{
  // instance
  instanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
  instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  instanceExtensions.push_back("VK_KHR_win32_surface");
  instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

  uint32_t count;
  vkEnumerateInstanceLayerProperties(&count, nullptr);

  std::vector<VkLayerProperties> layerProperties;
  layerProperties.resize(count);
  ASSERT_VK_SUCCESS(
    vkEnumerateInstanceLayerProperties(&count, layerProperties.data()));

  VkApplicationInfo appInfo = vkiApplicationInfo(nullptr, 0, nullptr, 0, 1);

  VkInstanceCreateInfo instInfo =
    vkiInstanceCreateInfo(&appInfo,
                          static_cast<uint32_t>(instanceLayers.size()),
                          instanceLayers.data(),
                          static_cast<uint32_t>(instanceExtensions.size()),
                          instanceExtensions.data());

  ASSERT_VK_SUCCESS(vkCreateInstance(&instInfo, nullptr, &instance));

  // surface
  surface = window->CreateSurface(instance);

  // device
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  uint32_t physicalDeviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> physicalsDevices(physicalDeviceCount);
  vkEnumeratePhysicalDevices(
    instance, &physicalDeviceCount, physicalsDevices.data());

  for (const auto& dev : physicalsDevices) {
    physicalDeviceProps = PhysicalDeviceProps(dev, surface);
    if (physicalDeviceProps.HasGraphicsSupport() &&
        physicalDeviceProps.HasPresentSupport()) {
      break;
    }
  }

  ASSERT_VK_VALID_HANDLE(physicalDeviceProps.handle);
  ASSERT_TRUE(physicalDeviceProps.GetGrahicsQueueFamiliyIdx() ==
              physicalDeviceProps.GetPresentQueueFamiliyIdx());

  float queuePriority = 1.0f;
  uint32_t queueFamiliyIdx = physicalDeviceProps.GetGrahicsQueueFamiliyIdx();

  VkDeviceQueueCreateInfo queueCreateInfo =
    vkiDeviceQueueCreateInfo(queueFamiliyIdx, 1, &queuePriority);

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.textureCompressionBC = true;
  deviceFeatures.fillModeNonSolid = true;
  deviceFeatures.multiDrawIndirect = true;

  VkDeviceCreateInfo deviceCreateInfo =
    vkiDeviceCreateInfo(1,
                        &queueCreateInfo,
                        0,
                        nullptr,
                        static_cast<uint32_t>(deviceExtensions.size()),
                        deviceExtensions.data(),
                        &deviceFeatures);

  ASSERT_VK_SUCCESS(vkCreateDevice(
    physicalDeviceProps.handle, &deviceCreateInfo, nullptr, &device));

  // queue
  vkGetDeviceQueue(device, queueFamiliyIdx, 0, &queue);

  // commandPool
  VkCommandPoolCreateInfo commandPoolCreateInfo =
    vkiCommandPoolCreateInfo(physicalDeviceProps.GetGrahicsQueueFamiliyIdx());

  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  ASSERT_VK_SUCCESS(
    vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &cmdPool));

  // semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo = vkiSemaphoreCreateInfo();

  ASSERT_VK_SUCCESS(vkCreateSemaphore(
    device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore));

  ASSERT_VK_SUCCESS(vkCreateSemaphore(
    device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore));
}

void
VulkanBase::DestroySwapchainIndependentResources()
{
  vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
  vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
  vkDestroyCommandPool(device, cmdPool, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
}

void
VulkanBase::CreateSwapchainDependentResources()
{
  // commandBuffers
  commandBuffers.resize(swapchain->imageCount);
  VkCommandBufferAllocateInfo allocateInfo = vkiCommandBufferAllocateInfo(
    cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, swapchain->imageCount);

  ASSERT_VK_SUCCESS(
    vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()));

  // fences
  VkFenceCreateInfo fenceInfo = vkiFenceCreateInfo();
  fences.resize(swapchain->imageCount);
  for (size_t i = 0; i < fences.size(); ++i) {
    ASSERT_VK_SUCCESS(vkCreateFence(device, &fenceInfo, nullptr, &fences[i]));

    // put fences in a signalled state
    VkSubmitInfo submitInfo =
      vkiSubmitInfo(0, nullptr, 0, 0, nullptr, 0, nullptr);

    ASSERT_VK_SUCCESS(vkQueueSubmit(queue, 1, &submitInfo, fences[i]));
  }
}

void
VulkanBase::DestroySwapchainDependentResources()
{
  for (auto fence : fences) {
    vkDestroyFence(device, fence, nullptr);
  }
  vkFreeCommandBuffers(device,
                       cmdPool,
                       static_cast<uint32_t>(commandBuffers.size()),
                       commandBuffers.data());
}

VulkanBase::Swapchain::Swapchain(VkDevice device,
                                 PhysicalDeviceProps physicalDeviceProps,
                                 VkSurfaceKHR surface)
  : device(device)
{
  auto surfaceCapabilities = physicalDeviceProps.GetSurfaceCapabilities();

  imageCount = surfaceCapabilities.minImageCount + 1;
  if (surfaceCapabilities.maxImageCount > 0 &&
      imageCount > surfaceCapabilities.maxImageCount) {
    imageCount = surfaceCapabilities.maxImageCount;
  }

  imageExtent = surfaceCapabilities.currentExtent;

  auto formatIter = std::find_if(
    physicalDeviceProps.surfaceFormats.begin(),
    physicalDeviceProps.surfaceFormats.end(),
    [](const VkSurfaceFormatKHR& format) {
      return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
             format.format == VK_FORMAT_B8G8R8A8_UNORM;
    });

  ASSERT_TRUE(formatIter != physicalDeviceProps.surfaceFormats.end());
  surfaceFormat = *formatIter;

  std::vector<VkPresentModeKHR> presentModes = { VK_PRESENT_MODE_MAILBOX_KHR,
                                                 VK_PRESENT_MODE_FIFO_KHR };

  auto presentModeIter =
    std::find_first_of(physicalDeviceProps.presentModes.begin(),
                       physicalDeviceProps.presentModes.end(),
                       presentModes.begin(),
                       presentModes.end());

  ASSERT_TRUE(presentModeIter != physicalDeviceProps.presentModes.end());
  presentMode = *presentModeIter;

  auto swapchainCreateInfo =
    vkiSwapchainCreateInfoKHR(surface,
                              imageCount,
                              surfaceFormat.format,
                              surfaceFormat.colorSpace,
                              imageExtent,
                              1,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              VK_SHARING_MODE_EXCLUSIVE,
                              VK_QUEUE_FAMILY_IGNORED,
                              nullptr,
                              surfaceCapabilities.currentTransform,
                              VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                              presentMode,
                              VK_TRUE,
                              VK_NULL_HANDLE);

  ASSERT_VK_SUCCESS(
    vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &handle));

  ASSERT_VK_SUCCESS(
    vkGetSwapchainImagesKHR(device, handle, &imageCount, nullptr));
  images.resize(imageCount);
  ASSERT_VK_SUCCESS(
    vkGetSwapchainImagesKHR(device, handle, &imageCount, images.data()));

  imageViews.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; ++i) {
    auto imageViewCreateInfo =
      vkiImageViewCreateInfo(images[i],
                             VK_IMAGE_VIEW_TYPE_2D,
                             surfaceFormat.format,
                             { VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY },
                             { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    ASSERT_VK_SUCCESS(
      vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]));
  }
}

VulkanBase::Swapchain::~Swapchain()
{
  for (auto imageView : imageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(device, handle, nullptr);
}

VulkanBase::PhysicalDeviceProps::PhysicalDeviceProps(
  VkPhysicalDevice physicalDevice,
  VkSurfaceKHR surface)
  : handle(physicalDevice)
  , surface(surface)
{
  vkGetPhysicalDeviceProperties(handle, &props);
  vkGetPhysicalDeviceFeatures(handle, &features);
  vkGetPhysicalDeviceMemoryProperties(handle, &memProps);

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, nullptr);
  queueFamilyProps.resize(count);
  vkGetPhysicalDeviceQueueFamilyProperties(
    handle, &count, queueFamilyProps.data());

  count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(handle, surface, &count, nullptr);
  surfaceFormats.resize(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(
    handle, surface, &count, surfaceFormats.data());

  count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(handle, surface, &count, nullptr);
  presentModes.resize(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    handle, surface, &count, presentModes.data());
}

uint32_t
VulkanBase::PhysicalDeviceProps::GetGrahicsQueueFamiliyIdx()
{
  for (uint32_t idx = 0; idx < queueFamilyProps.size(); ++idx) {
    if (queueFamilyProps[idx].queueCount == 0)
      continue;
    if (queueFamilyProps[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      return idx;
  }
  return -1;
}

uint32_t
VulkanBase::PhysicalDeviceProps::GetPresentQueueFamiliyIdx()
{
  for (uint32_t idx = 0; idx < queueFamilyProps.size(); ++idx) {
    if (queueFamilyProps[idx].queueCount == 0)
      continue;
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(handle, idx, surface, &presentSupport);
    if (presentSupport)
      return idx;
  }
  return -1;
}

VkSurfaceCapabilitiesKHR
VulkanBase::PhysicalDeviceProps::GetSurfaceCapabilities()
{
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    handle, surface, &surfaceCapabilities);
  return surfaceCapabilities;
}

bool
VulkanBase::PhysicalDeviceProps::HasGraphicsSupport()
{
  return GetGrahicsQueueFamiliyIdx() != (uint32_t)-1;
}

bool
VulkanBase::PhysicalDeviceProps::HasPresentSupport()
{
  return GetPresentQueueFamiliyIdx() != (uint32_t)-1;
}
