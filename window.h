#pragma once

// clang-format off
#include <vulkan\vulkan_core.h>
#include <GLFW\glfw3.h>
// clang-format on

#include "vk_base.h"

struct Window : VulkanBase::VulkanWindow
{
  GLFWwindow* glfwWindow;

  Window(int width, int height, char* title);
  void Update();

  VkExtent2D GetExtent();
  VkSurfaceKHR CreateSurface(VkInstance instance);

  struct KeyInput
  {
    int updated;
    int key;
    int scanCode;
    int action;
    int mods;
  } keyInput;

  struct KeyboardState
  {
    int key[GLFW_KEY_LAST];
  } keyboardState;

  struct CursorInput
  {
    int updated;
    double dx;
    double dy;
    double x;
    double y;
  } cursorInput;

  struct MouseInput
  {
    int updated;
    int button;
    int action;
    int mods;
  } mouseInput;

  struct WindowSize
  {
    int updated;
    int width;
    int height;
  } windowSize;
};
