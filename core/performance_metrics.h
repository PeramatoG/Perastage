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
  Frame, MovingFrame, References, UniqueResources, CacheRoot, FirstVisible, SceneSignature,
  View2DScene, View2DResources, View2DResourceSync, View2DBounds, View2DSort,
  View2DVisibility, View2DGrid, View2DRuler, View2DHover, View2DOverlays, View2DLabelTotal, View2DLabelCompose, View2DLabelCull,
  View2DLabelDraw, View2DNavigation, View2DCompositions, View2DLabels,
  View2DGlyphs, View2DCulledLabels,
  View2DLabelNew, View2DLabelContent, View2DLabelAnchor, View2DLabelStyle,
  View2DLabelOrientation, View2DLabelFont, View2DLabelRemoved, Count
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
  explicit Scope(Work work, bool enabled = true)
      : work_(work), enabled_(enabled) {
    if (enabled_) {
      start_ = Clock::now();
      Count(work);
    }
  }
  ~Scope() { Finish(); }
  void Finish() {
    if (!enabled_)
      return;
    enabled_ = false;
    samples[static_cast<size_t>(work_)].nanoseconds +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
  }
private:
  using Clock = std::chrono::steady_clock;
  Work work_;
  bool enabled_;
  Clock::time_point start_{};
};
inline bool Viewer2DEnabled() {
  static const bool enabled = std::getenv("PERASTAGE_VIEWER2D_PERF") != nullptr;
  return enabled;
}

inline thread_local bool viewer2DFrameActive = false;
class Viewer2DFrameScope {
public:
  explicit Viewer2DFrameScope(bool enabled)
      : previous_(viewer2DFrameActive) { viewer2DFrameActive = enabled; }
  ~Viewer2DFrameScope() { viewer2DFrameActive = previous_; }
private:
  bool previous_;
};

inline std::string Summary(Work first = Work::Extraction,
                           Work end = Work::View2DScene) {
  constexpr const char *names[] = {
    "extraction", "scene_parse", "scene_apply", "resource_sync", "cache_lookup",
    "cache_hit", "cache_miss", "cache_read", "cache_write", "3ds_load",
    "mesh_processing", "representation", "gpu_upload", "bounds", "visible_set",
    "scene_fingerprint", "frame", "moving_frame", "references", "unique_resources",
    "cache_root", "first_visible", "scene_signature",
    "scene", "resources", "resource_sync", "bounds", "sort", "visibility",
    "grid", "ruler", "hover", "overlays",
    "labels_total", "label_compose", "label_cull", "label_draw", "navigation",
    "compositions", "composed_labels_drawn", "composed_glyphs_drawn", "culled_labels",
    "label_new", "label_content",
    "label_anchor", "label_style", "label_orientation", "label_font", "label_removed"};
  static_assert(sizeof(names) / sizeof(names[0]) == static_cast<size_t>(Work::Count));
  std::ostringstream out;
  for (size_t i = static_cast<size_t>(first); i < static_cast<size_t>(end); ++i)
    if (samples[i].count)
      out << names[i] << '=' << samples[i].count << '/' <<
          samples[i].nanoseconds / 1000000.0 << "ms ";
  return out.str();
}
} // namespace performance
