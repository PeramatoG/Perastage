#pragma once

#include <array>
#include <chrono>
#include <cstddef>

namespace performance {

class PlacementPerformance {
public:
  using Clock = std::chrono::steady_clock;

  void Applied();
  void Presented(const char *viewport, Clock::time_point frameStart,
                 Clock::time_point submitted);
  void Finish(const char *viewport);

private:
  static constexpr std::size_t kBatchSize = 120;
  std::array<double, kBatchSize> m_cpuMs{}, m_withSwapMs{}, m_inputMs{};
  std::size_t m_frames = 0;
  std::size_t m_applied = 0;
  Clock::time_point m_latestTransform{};
};

} // namespace performance
