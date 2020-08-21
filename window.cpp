#include "window.h"

void
onKey(GLFWwindow* glfwWindow, int key, int scanCode, int action, int mods)
{
  struct Window* window =
    (struct Window*)(glfwGetWindowUserPointer(glfwWindow));
  if (window != 0) {
    window->keyInput.updated = 1;
    window->keyInput.key = key;
    window->keyInput.scanCode = scanCode;
    window->keyInput.action = action;

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
      window->keyboardState.key[key] = GLFW_PRESS;
    } else {
      window->keyboardState.key[key] = GLFW_RELEASE;
    }
  }
}

void
onCursorMove(GLFWwindow* glfwWindow, double x, double y)
{
  struct Window* window =
    (struct Window*)(glfwGetWindowUserPointer(glfwWindow));
  if (window != 0) {
    window->cursorInput.updated = 1;
    window->cursorInput.dx = x - window->cursorInput.x;
    window->cursorInput.dy = y - window->cursorInput.y;
    window->cursorInput.x = x;
    window->cursorInput.y = y;
  }
}

void
onMouseButton(GLFWwindow* glfwWindow, int button, int action, int mods)
{
  struct Window* window =
    (struct Window*)(glfwGetWindowUserPointer(glfwWindow));
  if (window != 0) {
    window->mouseInput.updated = 1;
    window->mouseInput.button = button;
    window->mouseInput.action = action;
    window->mouseInput.mods = mods;
  }
}

void
onWindowSize(GLFWwindow* glfwWindow, int width, int height)
{
  struct Window* window =
    (struct Window*)(glfwGetWindowUserPointer(glfwWindow));
  if (window != 0) {
    window->windowSize.updated = 1;
    window->windowSize.width = width;
    window->windowSize.height = height;
  }
}

Window::Window(int width, int height, char* title)
{
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  glfwWindow = glfwCreateWindow(width, height, title, 0, 0);

  glfwSetWindowUserPointer(glfwWindow, this);
  glfwSetWindowSizeCallback(glfwWindow, &onWindowSize);
  glfwSetKeyCallback(glfwWindow, &onKey);
  glfwSetCursorPosCallback(glfwWindow, &onCursorMove);
  glfwSetMouseButtonCallback(glfwWindow, &onMouseButton);
  glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPos(glfwWindow, 0.0, 0.0);

  cursorInput.x = 0;
  cursorInput.y = 0;
  windowSize.updated = 1;
  windowSize.width = width;
  windowSize.height = height;

  for (int i = 0; i < GLFW_KEY_LAST; ++i) {
    keyboardState.key[i] = GLFW_RELEASE;
  }
}

void
Window::Update()
{
  cursorInput.updated = 0;
  mouseInput.updated = 0;
  keyInput.updated = 0;
  windowSize.updated = 0;
  glfwPollEvents();
}

VkExtent2D
Window::GetExtent()
{
  return { static_cast<uint32_t>(windowSize.width),
           static_cast<uint32_t>(windowSize.height) };
}

VkSurfaceKHR
Window::CreateSurface(VkInstance instance)
{
  VkSurfaceKHR result;
  glfwCreateWindowSurface(instance, glfwWindow, nullptr, &result);
  return result;
}
