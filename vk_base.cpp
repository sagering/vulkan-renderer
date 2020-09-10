#include "vk_base.h"

#include <algorithm> // find_if, find_first_of

#include "vk_init.h"
#include "vk_utils.h"

DeviceProps::DeviceProps(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
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
DeviceProps::GetGrahicsQueueFamiliyIdx()
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
DeviceProps::GetPresentQueueFamiliyIdx()
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
DeviceProps::GetSurfaceCapabilities()
{
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    handle, surface, &surfaceCapabilities);
  return surfaceCapabilities;
}

bool
DeviceProps::HasGraphicsSupport()
{
  return GetGrahicsQueueFamiliyIdx() != (uint32_t)-1;
}

bool
DeviceProps::HasPresentSupport()
{
  return GetPresentQueueFamiliyIdx() != (uint32_t)-1;
}

Swapchain::Swapchain(VkDevice device,
                     DeviceProps physicalDeviceProps,
                     VkSurfaceKHR surface)
  : device(device)
  , surface(surface)
{
  auto surfaceCapabilities = physicalDeviceProps.GetSurfaceCapabilities();

  imageCount = surfaceCapabilities.minImageCount + 1;
  if (surfaceCapabilities.maxImageCount > 0 &&
      imageCount > surfaceCapabilities.maxImageCount) {
    imageCount = surfaceCapabilities.maxImageCount;
  }

  extent = surfaceCapabilities.currentExtent;

  auto formatIter = std::find_if(
    physicalDeviceProps.surfaceFormats.begin(),
    physicalDeviceProps.surfaceFormats.end(),
    [](const VkSurfaceFormatKHR& format) {
      return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
             format.format == VK_FORMAT_B8G8R8A8_UNORM;
    });

  ASSERT_TRUE(formatIter != physicalDeviceProps.surfaceFormats.end());
  format = *formatIter;

  std::vector<VkPresentModeKHR> presentModes = { VK_PRESENT_MODE_MAILBOX_KHR,
                                                 VK_PRESENT_MODE_FIFO_KHR };

  auto presentModeIter =
    std::find_first_of(physicalDeviceProps.presentModes.begin(),
                       physicalDeviceProps.presentModes.end(),
                       presentModes.begin(),
                       presentModes.end());

  ASSERT_TRUE(presentModeIter != physicalDeviceProps.presentModes.end());
  presentMode = *presentModeIter;

  transform = surfaceCapabilities.currentTransform;
}

void
Swapchain::CreatePhysicalSwapchain(VkImageUsageFlags usage)
{
  auto swapchainCreateInfo =
    vkiSwapchainCreateInfoKHR(surface,
                              imageCount,
                              format.format,
                              format.colorSpace,
                              extent,
                              1,
                              usage,
                              VK_SHARING_MODE_EXCLUSIVE,
                              VK_QUEUE_FAMILY_IGNORED,
                              nullptr,
                              transform,
                              VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                              presentMode,
                              VK_TRUE,
                              VK_NULL_HANDLE);

  ASSERT_VK_SUCCESS(
    vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain));

  std::vector<VkImage> vkImages;

  ASSERT_VK_SUCCESS(
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
  vkImages.resize(imageCount);

  ASSERT_VK_SUCCESS(
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, vkImages.data()));

  images.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; ++i) {
    images[i].image = vkImages[i];
    images[i].memory = VK_NULL_HANDLE;
    images[i].stageFlags =
      VK_PIPELINE_STAGE_HOST_BIT; // VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    images[i].accessFlags = 0;
    images[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto eventCreateInfo = vkiEventCreateInfo();
    vkCreateEvent(device, &eventCreateInfo, nullptr, &images[i].event);
    vkSetEvent(device, images[i].event);

    auto imageViewCreateInfo =
      vkiImageViewCreateInfo(images[i].image,
                             VK_IMAGE_VIEW_TYPE_2D,
                             format.format,
                             { VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY },
                             { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    ASSERT_VK_SUCCESS(vkCreateImageView(
      device, &imageViewCreateInfo, nullptr, &images[i].view));
  }
}

Swapchain::~Swapchain()
{
  for (auto image : images) {
    vkDestroyImageView(device, image.view, nullptr);
  }

  if (swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }
}

PhysicalImage*
Swapchain::AcquireImage(VkSemaphore imageAvailable)
{
  ASSERT_VK_VALID_HANDLE(swapchain);
  ASSERT_VK_SUCCESS(vkAcquireNextImageKHR(device,
                                          swapchain,
                                          UINT64_MAX,
                                          imageAvailable,
                                          VK_NULL_HANDLE,
                                          &nextImageIdx));

  return &images[nextImageIdx];
}

void
Swapchain::Present(VkQueue queue, VkSemaphore renderFinished)
{
  ASSERT_VK_VALID_HANDLE(swapchain);
  VkPresentInfoKHR presentInfo = vkiPresentInfoKHR(
    1, &renderFinished, 1, &swapchain, &nextImageIdx, nullptr);
  ASSERT_VK_SUCCESS(vkQueuePresentKHR(queue, &presentInfo));
}

VulkanBase::VulkanBase(VulkanWindow* window)
  : window(window)
{
  CreateResources();
}

VulkanBase::~VulkanBase()
{
  DestroyResources();
}

void
VulkanBase::CreateResources()
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
    deviceProps = DeviceProps(dev, surface);
    if (deviceProps.HasGraphicsSupport() && deviceProps.HasPresentSupport()) {
      break;
    }
  }

  ASSERT_VK_VALID_HANDLE(deviceProps.handle);
  ASSERT_TRUE(deviceProps.GetGrahicsQueueFamiliyIdx() ==
              deviceProps.GetPresentQueueFamiliyIdx());

  float queuePriority = 1.0f;
  uint32_t queueFamiliyIdx = deviceProps.GetGrahicsQueueFamiliyIdx();

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

  ASSERT_VK_SUCCESS(
    vkCreateDevice(deviceProps.handle, &deviceCreateInfo, nullptr, &device));

  // queue
  vkGetDeviceQueue(device, queueFamiliyIdx, 0, &queue);

  // commandPool
  VkCommandPoolCreateInfo commandPoolCreateInfo =
    vkiCommandPoolCreateInfo(deviceProps.GetGrahicsQueueFamiliyIdx());

  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  ASSERT_VK_SUCCESS(
    vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &cmdPool));

  // semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo = vkiSemaphoreCreateInfo();

  ASSERT_VK_SUCCESS(vkCreateSemaphore(
    device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore));

  ASSERT_VK_SUCCESS(vkCreateSemaphore(
    device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore));

  // command buffers
  commandBuffers.resize(MAX_NUMBER_CMD_BUFFERS);
  VkCommandBufferAllocateInfo allocateInfo = vkiCommandBufferAllocateInfo(
    cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, MAX_NUMBER_CMD_BUFFERS);

  ASSERT_VK_SUCCESS(
    vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()));

  // fences
  VkFenceCreateInfo fenceInfo = vkiFenceCreateInfo();
  fences.resize(MAX_NUMBER_CMD_BUFFERS);
  for (size_t i = 0; i < fences.size(); ++i) {
    ASSERT_VK_SUCCESS(vkCreateFence(device, &fenceInfo, nullptr, &fences[i]));

    // put fences in a signalled state
    VkSubmitInfo submitInfo =
      vkiSubmitInfo(0, nullptr, 0, 0, nullptr, 0, nullptr);

    ASSERT_VK_SUCCESS(vkQueueSubmit(queue, 1, &submitInfo, fences[i]));
    vkQueueWaitIdle(queue);
  }
}

void
VulkanBase::DestroyResources()
{
  for (auto fence : fences) {
    vkDestroyFence(device, fence, nullptr);
  }
  vkFreeCommandBuffers(device,
                       cmdPool,
                       static_cast<uint32_t>(commandBuffers.size()),
                       commandBuffers.data());

  vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
  vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
  vkDestroyCommandPool(device, cmdPool, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
}

VulkanBase::CommandBuffer
VulkanBase::NextCmdBuffer()
{
  auto cmdBuffer = commandBuffers[nextCmdBufferIdx];
  ASSERT_VK_SUCCESS(
    vkWaitForFences(device, 1, &fences[nextCmdBufferIdx], true, (uint64_t)-1));
  ASSERT_VK_SUCCESS(vkResetFences(device, 1, &fences[nextCmdBufferIdx]));
  ASSERT_VK_SUCCESS(vkResetCommandBuffer(cmdBuffer, 0));

  auto fence = fences[nextCmdBufferIdx];
  nextCmdBufferIdx = (nextCmdBufferIdx + 1) % MAX_NUMBER_CMD_BUFFERS;

  return { cmdBuffer, fence };
}
