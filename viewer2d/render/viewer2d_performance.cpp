#include "viewer2d_performance.h"

#include "logger.h"
#include "performance_metrics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <span>

namespace viewer2d::render {
namespace {

double Milliseconds(ViewportPerformance::Clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

void AppendDistribution(std::ostringstream &out, const char *name,
                        std::span<double> samples) {
  if (samples.empty())
    return;
  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  std::sort(samples.begin(), samples.end());
  const auto percentile = [&](double fraction) {
    return samples[static_cast<std::size_t>(std::ceil(samples.size() * fraction)) - 1];
  };
  out << name << "_ms(n=" << samples.size() << ",avg=" << sum / samples.size()
      << ",p95=" << percentile(.95) << ",p99=" << percentile(.99)
      << ",max=" << samples.back() << ") ";
}

} // namespace

void ViewportPerformance::NavigationChanged() {
  if (performance::Viewer2DEnabled() && m_pendingInput == Clock::time_point{})
    m_pendingInput = Clock::now();
}

void ViewportPerformance::Presented(Clock::time_point start,
                                    Clock::time_point submitted) {
  const auto returned = Clock::now();
  m_cpuMs[m_frameCount] = Milliseconds(submitted - start);
  m_withSwapMs[m_frameCount++] = Milliseconds(returned - start);
  if (m_pendingInput != Clock::time_point{}) {
    m_inputMs[m_inputCount++] = Milliseconds(returned - m_pendingInput);
    m_pendingInput = {};
  }
  if (m_frameCount == kBatchSize)
    Report();
}

void ViewportPerformance::Report() {
  if (m_frameCount == 0)
    return;
  std::ostringstream out;
  out << "Viewer2D performance " << std::fixed << std::setprecision(3);
  AppendDistribution(out, "frame_cpu", {m_cpuMs.data(), m_frameCount});
  AppendDistribution(out, "frame_with_swap", {m_withSwapMs.data(), m_frameCount});
  AppendDistribution(out, "input_to_swap", {m_inputMs.data(), m_inputCount});
  out << performance::Summary(performance::Work::View2DScene,
                              performance::Work::Count);
  Logger::Instance().Log(out.str());
  for (auto i = static_cast<size_t>(performance::Work::View2DScene);
       i < static_cast<size_t>(performance::Work::Count); ++i)
    performance::samples[i] = {};
  m_frameCount = 0;
  m_inputCount = 0;
}

} // namespace viewer2d::render
