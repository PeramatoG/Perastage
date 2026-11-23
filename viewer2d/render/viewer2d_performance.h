#pragma once

#include <array>
#include <chrono>
#include <cstddef>

namespace viewer2d::render {

class ViewportPerformance {
public:
  using Clock = std::chrono::steady_clock;

  void NavigationChanged();
  void Presented(Clock::time_point start, Clock::time_point submitted);
  void Report();

private:
  static constexpr std::size_t kBatchSize = 120;
  std::array<double, kBatchSize> m_cpuMs{}, m_withSwapMs{}, m_inputMs{};
  std::size_t m_frameCount = 0, m_inputCount = 0;
  Clock::time_point m_pendingInput{};
};

} // namespace viewer2d::render
