#pragma once

struct Clock
{
  Clock();
  void Update();

  float GetTick() { return tick; }
  double GetNow() { return now; }

private:
  float tick;
  double now;
};
