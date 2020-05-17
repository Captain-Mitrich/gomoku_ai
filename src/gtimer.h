#ifndef GTIMER_H
#define GTIMER_H

#include <chrono>

namespace nsg
{

class GTimer
{
public:
  GTimer(bool to_start = true)
  {
    if (to_start)
      start();
  }

  void start()
  {
    m_start = Clock::now();
  }

  uint elapsed() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - m_start).count();
  }

protected:
  using Clock = std::chrono::steady_clock;
  using Time = std::chrono::time_point<Clock>;

  Time m_start;
};

} //namespace nsg

#endif
