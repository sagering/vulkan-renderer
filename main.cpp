// clang-format off
#include <vulkan\vulkan_core.h>
#include <GLFW\glfw3.h>
// clang-format on

#include <glm\gtx\transform.hpp>  // lookAt, perspective
#include <glm\gtx\quaternion.hpp> // quat

#include "window.h"
#include "playground.h"
#include "clock.h"

#include <iostream>

int
main()
{
  {
    Window window(1280, 920, "Playground");
    VulkanBase base(&window);
    ExampleRenderGraph rg(&base);
    rg.Setup();

    Clock clock = {};

    float timePassed = 0;
    uint32_t fps = 0;

    while (window.keyboardState.key[GLFW_KEY_ESCAPE] != 1) {
      window.Update();
      clock.Update();

      rg.Render();
      timePassed += clock.GetTick();
      fps += 1;

      if (timePassed > 1.f) {
        std::cout << "FPS: " << fps << std::endl;
        timePassed = 0.f;
        fps = 0;
      }
    }
  }

  return 0;
}
