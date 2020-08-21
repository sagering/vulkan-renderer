#include "clock.h"
#include <chrono>

double
nowd()
{
  return std::chrono::duration_cast<std::chrono::duration<double>>(
           std::chrono::steady_clock::now().time_since_epoch())
    .count();
}

Clock::Clock()
{
  now = nowd();
  tick = 0.f;
}

void
Clock::Update()
{
  double last = now;
  double nnow = nowd();
  double ntick = nnow - last;

  if (ntick < 0.016) {
    tick = 0.f;
  } else {
    now = nnow;
    tick = (float)ntick;
  }
}
