#include "startup_profile.h"

#include <cassert>
#include <string>

int main() {
  using startup::ViewportRequirement;

  assert(startup::ResolveViewportRequirement("3d_layout_view") ==
         ViewportRequirement::Viewer3D);
  assert(startup::ResolveViewportRequirement("2d_layout_view") ==
         ViewportRequirement::Viewer2D);
  assert(startup::ResolveViewportRequirement("layout_mode_view") ==
         ViewportRequirement::None);

  const std::string legacy2D = "name=2DViewport;name=2DRenderOptions";
  const std::string legacy3D = "name=3DViewport";
  assert(startup::ResolveViewportRequirement("", &legacy2D) ==
         ViewportRequirement::Viewer2D);
  assert(startup::ResolveViewportRequirement("legacy", &legacy3D) ==
         ViewportRequirement::Viewer3D);
  assert(startup::ResolveViewportRequirement("unknown") ==
         ViewportRequirement::Viewer3D);

  int ensure2DCount = 0;
  int ensure3DCount = 0;
  auto ensure2D = [&ensure2DCount]() { ++ensure2DCount; };
  auto ensure3D = [&ensure3DCount]() { ++ensure3DCount; };
  startup::PrepareRequiredViewport("2d_layout_view", nullptr, ensure2D,
                                   ensure3D);
  assert(ensure2DCount == 1 && ensure3DCount == 0);
  startup::PrepareRequiredViewport("layout_mode_view", nullptr, ensure2D,
                                   ensure3D);
  assert(ensure2DCount == 1 && ensure3DCount == 0);
  startup::PrepareRequiredViewport("3d_layout_view", nullptr, ensure2D,
                                   ensure3D);
  assert(ensure2DCount == 1 && ensure3DCount == 1);
  startup::PrepareRequiredViewport("2d_layout_view", nullptr, ensure2D,
                                   ensure3D);
  assert(ensure2DCount == 2 && ensure3DCount == 1);

  startup::Metrics metrics;
  ++metrics.ensure2DCalls;
  ++metrics.viewer2DConstructions;
  assert(metrics.viewer3DConstructions == 0);
  ++metrics.ensure3DCalls;
  ++metrics.viewer3DConstructions;
  assert(metrics.viewer2DConstructions == 1);
  assert(metrics.viewer3DConstructions == 1);
  metrics.finalViewMode = "3d_layout_view";
  metrics.finalActiveLayout = "Plot";
  metrics.projectOpenAttempts = 1;
  metrics.projectOpenSuccesses = 1;
  metrics.archiveTraversals = 1;
  const std::string summary =
      startup::FormatInteractiveReadySummary(metrics, 42);
  assert(summary.find("event=InteractiveReady duration_ms=42") !=
         std::string::npos);
  assert(summary.find("view_mode=3d_layout_view") != std::string::npos);
  assert(summary.find("active_layout=Plot") != std::string::npos);
  assert(summary.find("pstg_open_attempts=1") != std::string::npos);
  return 0;
}
