#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <cstdlib>

// Aggregate, thread-local diagnostics: no per-object logging or GUI dependency.
namespace performance {
enum class Work {
  Extraction, SceneParse, SceneApply, ResourceSync, CacheLookup, CacheHit,
  CacheMiss, CacheRead, CacheWrite, ThreeDsLoad, MeshProcessing,
  Representation, GpuUpload, Bounds, VisibleSet, SceneFingerprint,
  Frame, MovingFrame, References, UniqueResources, CacheRoot, FirstVisible, SceneSignature, Count
};
struct Sample { std::uint64_t count = 0, nanoseconds = 0; };
inline thread_local std::array<Sample, static_cast<size_t>(Work::Count)> samples{};
inline void Count(Work work, std::uint64_t count = 1) {
  samples[static_cast<size_t>(work)].count += count;
}
inline void Reset() { samples = {}; }
inline bool Enabled() {
  static const bool enabled = std::getenv("PERASTAGE_PERF_DIAGNOSTICS") != nullptr;
  return enabled;
}
class Scope {
public:
  explicit Scope(Work work) : work_(work), start_(Clock::now()) { Count(work); }
  ~Scope() {
    samples[static_cast<size_t>(work_)].nanoseconds +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
  }
private:
  using Clock = std::chrono::steady_clock;
  Work work_;
  Clock::time_point start_;
};
inline std::string Summary() {
  constexpr const char *names[] = {
    "extraction", "scene_parse", "scene_apply", "resource_sync", "cache_lookup",
    "cache_hit", "cache_miss", "cache_read", "cache_write", "3ds_load",
    "mesh_processing", "representation", "gpu_upload", "bounds", "visible_set",
    "scene_fingerprint", "frame", "moving_frame", "references", "unique_resources",
    "cache_root", "first_visible", "scene_signature"};
  std::ostringstream out;
  for (size_t i = 0; i < samples.size(); ++i)
    if (samples[i].count)
      out << names[i] << '=' << samples[i].count << '/' <<
          samples[i].nanoseconds / 1000000.0 << "ms ";
  return out.str();
}
} // namespace performance
