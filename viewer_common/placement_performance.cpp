#include "placement_performance.h"

#include "logger.h"
#include "performance_metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <span>

namespace performance {
namespace {

double Milliseconds(PlacementPerformance::Clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

void AppendDistribution(std::ostringstream &out, const char *name,
                        std::span<double> values) {
  std::sort(values.begin(), values.end());
  const auto percentile = [&](double fraction) {
    return values[static_cast<std::size_t>(std::ceil(values.size() * fraction)) - 1];
  };
  out << name << "_ms(n=" << values.size()
      << ",avg=" << std::accumulate(values.begin(), values.end(), 0.0) / values.size()
      << ",p95=" << percentile(0.95) << ",p99=" << percentile(0.99)
      << ",max=" << values.back() << ") ";
}

} // namespace

void PlacementPerformance::Applied() {
  if (!PlacementEnabled())
    return;
  ++m_applied;
  m_latestTransform = Clock::now();
}

void PlacementPerformance::Presented(const char *viewport,
                                     Clock::time_point frameStart,
                                     Clock::time_point submitted) {
  if (m_latestTransform == Clock::time_point{})
    return;
  const auto returned = Clock::now();
  m_cpuMs[m_frames] = Milliseconds(submitted - frameStart);
  m_withSwapMs[m_frames] = Milliseconds(returned - frameStart);
  m_inputMs[m_frames++] = Milliseconds(returned - m_latestTransform);
  m_latestTransform = {};
  if (m_frames == kBatchSize)
    Finish(viewport);
}

void PlacementPerformance::Finish(const char *viewport) {
  if (m_applied == 0)
    return;
  std::ostringstream out;
  out << viewport << " transform performance applied=" << m_applied
      << " presented=" << m_frames << ' ';
  if (m_frames != 0) {
    AppendDistribution(out, "frame_cpu", {m_cpuMs.data(), m_frames});
    AppendDistribution(out, "frame_with_swap", {m_withSwapMs.data(), m_frames});
    AppendDistribution(out, "latest_transform_to_swap", {m_inputMs.data(), m_frames});
  }
  out << Summary(Work::PlacementPointer, Work::Count);
  Logger::Instance().Log(out.str());
  for (auto i = static_cast<std::size_t>(Work::PlacementPointer);
       i < static_cast<std::size_t>(Work::Count); ++i)
    samples[i] = {};
  m_applied = 0;
  m_frames = 0;
  m_latestTransform = {};
}

} // namespace performance
