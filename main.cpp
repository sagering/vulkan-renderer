// clang-format off
#include <vulkan\vulkan_core.h>
#include <GLFW\glfw3.h>
// clang-format on

#include <glm\gtx\transform.hpp>

#include "window.h"
#include "renderer.h"
#include "clock.h"
#include "teapot.h"
#include <algorithm> // min, max

void
transformPot(std::vector<float>& pot)
{
  float xMin = std::numeric_limits<float>::max();
  float yMin = std::numeric_limits<float>::max();
  float zMin = std::numeric_limits<float>::max();

  float xMax = std::numeric_limits<float>::min();
  float yMax = std::numeric_limits<float>::min();
  float zMax = std::numeric_limits<float>::min();

  for (int i = 0; i < pot.size(); i += 3) {
    xMin = std::min(xMin, pot[i]);
    yMin = std::min(yMin, pot[i + 1]);
    zMin = std::min(zMin, pot[i + 2]);

    xMax = std::max(xMax, pot[i]);
    yMax = std::max(yMax, pot[i + 1]);
    zMax = std::max(zMax, pot[i + 2]);
  }

  for (int i = 0; i < pot.size(); i += 3) {
    pot[i] = 2 * (pot[i] - xMin) / (xMax - xMin) - 1;
    pot[i + 1] = 2 * (pot[i + 1] - yMin) / (yMax - yMin) - 1;
    pot[i + 1] = -pot[i + 1];
    pot[i + 2] = 2 * (pot[i + 2] - zMin) / (zMax - zMin) - 1;
  }
}

int
main()
{
  {
    Window window(1280, 920, "Teapot");
    Renderer renderer(&window);
    Clock clock = {};

    transformPot(teapot);

    while (window.keyboardState.key[GLFW_KEY_ESCAPE] != 1) {
      window.Update();
      renderer.Update();
      clock.Update();

      renderer.pushVertices(teapot);
      renderer.drawFrame();
    }
  }

  return 0;
}
