#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace startup {

enum class ViewportRequirement { None, Viewer2D, Viewer3D };

struct Metrics {
  using Clock = std::chrono::steady_clock;

  Clock::time_point startedAt = Clock::now();
  long long userConfigMs = 0;
  long long mainWindowConstructionMs = 0;
  long long startupPathResolutionMs = 0;
  long long projectArchiveLoadMs = 0;
  long long mvrRestoreMs = 0;
  size_t projectOpenAttempts = 0;
  size_t projectOpenSuccesses = 0;
  size_t archiveTraversals = 0;
  size_t sceneMvrWrites = 0;
  size_t sceneMvrBytes = 0;
  size_t cacheEntriesTransferred = 0;
  size_t cacheBytesTransferred = 0;
  size_t cacheEntriesRejected = 0;
  size_t applySavedLayoutCalls = 0;
  size_t auiPerspectiveLoads = 0;
  size_t auiUpdates = 0;
  size_t activateLayoutCalls = 0;
  size_t ensure3DCalls = 0;
  size_t ensure2DCalls = 0;
  size_t viewer3DConstructions = 0;
  size_t viewer2DConstructions = 0;
  size_t fixtureReloads = 0;
  size_t trussReloads = 0;
  size_t hoistReloads = 0;
  size_t sceneObjectReloads = 0;
  size_t layerReloads = 0;
  size_t cacheDeepValidations = 0;
  bool interactiveReady = false;
  std::string finalViewMode;
  std::string finalActiveLayout;
};

ViewportRequirement
ResolveViewportRequirement(const std::string &viewMode,
                           const std::string *legacyPerspective = nullptr);
std::string FormatInteractiveReadySummary(const Metrics &metrics,
                                          long long durationMs);
ViewportRequirement
PrepareRequiredViewport(const std::string &viewMode,
                        const std::string *legacyPerspective,
                        const std::function<void()> &ensure2D,
                        const std::function<void()> &ensure3D);

} // namespace startup
