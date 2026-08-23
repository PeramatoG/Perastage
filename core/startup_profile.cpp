#include "startup_profile.h"

namespace startup {

// Resolves the heavyweight viewport required by a semantic saved view mode.
ViewportRequirement
ResolveViewportRequirement(const std::string &viewMode,
                           const std::string *legacyPerspective) {
  if (viewMode == "3d_layout_view")
    return ViewportRequirement::Viewer3D;
  if (viewMode == "2d_layout_view")
    return ViewportRequirement::Viewer2D;
  if (viewMode == "layout_mode_view")
    return ViewportRequirement::None;

  if (legacyPerspective) {
    if (legacyPerspective->find("2DViewport") != std::string::npos ||
        legacyPerspective->find("2DRenderOptions") != std::string::npos)
      return ViewportRequirement::Viewer2D;
    if (legacyPerspective->find("3DViewport") != std::string::npos)
      return ViewportRequirement::Viewer3D;
  }
  return ViewportRequirement::Viewer3D;
}

// Invokes only the lazy viewport factory required by the resolved view mode.
ViewportRequirement
PrepareRequiredViewport(const std::string &viewMode,
                        const std::string *legacyPerspective,
                        const std::function<void()> &ensure2D,
                        const std::function<void()> &ensure3D) {
  const ViewportRequirement requirement =
      ResolveViewportRequirement(viewMode, legacyPerspective);
  if (requirement == ViewportRequirement::Viewer2D && ensure2D)
    ensure2D();
  else if (requirement == ViewportRequirement::Viewer3D && ensure3D)
    ensure3D();
  return requirement;
}

// Formats the stable final startup record for every diagnostic sink.
std::string FormatInteractiveReadySummary(const Metrics &metrics,
                                          long long durationMs) {
  return "StartupProfile event=InteractiveReady duration_ms=" +
         std::to_string(durationMs) + " view_mode=" +
         (metrics.finalViewMode.empty() ? "unknown" : metrics.finalViewMode) +
         " active_layout=" +
         (metrics.finalActiveLayout.empty() ? "none"
                                            : metrics.finalActiveLayout) +
         " apply_saved_layout=" +
         std::to_string(metrics.applySavedLayoutCalls) +
         " aui_load_perspective=" +
         std::to_string(metrics.auiPerspectiveLoads) +
         " aui_updates=" + std::to_string(metrics.auiUpdates) +
         " activate_layout=" + std::to_string(metrics.activateLayoutCalls) +
         " ensure_3d=" + std::to_string(metrics.ensure3DCalls) +
         " construct_3d=" + std::to_string(metrics.viewer3DConstructions) +
         " ensure_2d=" + std::to_string(metrics.ensure2DCalls) +
         " construct_2d=" + std::to_string(metrics.viewer2DConstructions) +
         " table_reloads=" + std::to_string(metrics.fixtureReloads) + "," +
         std::to_string(metrics.trussReloads) + "," +
         std::to_string(metrics.hoistReloads) + "," +
         std::to_string(metrics.sceneObjectReloads) +
         " layer_reloads=" + std::to_string(metrics.layerReloads) +
         " pstg_open_attempts=" + std::to_string(metrics.projectOpenAttempts) +
         " pstg_open_successes=" +
         std::to_string(metrics.projectOpenSuccesses) +
         " archive_traversals=" + std::to_string(metrics.archiveTraversals) +
         " cache_entries=" + std::to_string(metrics.cacheEntriesTransferred) +
         " cache_bytes=" + std::to_string(metrics.cacheBytesTransferred) +
         " cache_rejected=" + std::to_string(metrics.cacheEntriesRejected) +
         " cache_deep_validations=" +
         std::to_string(metrics.cacheDeepValidations) +
         " layout_cache_fast=" +
         std::to_string(metrics.layoutCacheFastValidationAttempts) + "," +
         std::to_string(metrics.layoutCacheFastValidationHits) + "," +
         std::to_string(metrics.layoutCacheFastValidationRejects) +
         " hydrated_rasters=" +
         std::to_string(metrics.hydratedViewRasters) + "," +
         std::to_string(metrics.hydratedLegendRasters) +
         " layout_cache_validation_ms=" +
         std::to_string(metrics.layoutCacheValidationMs) +
         " scene_mvr_writes=" + std::to_string(metrics.sceneMvrWrites) +
         " scene_mvr_bytes=" + std::to_string(metrics.sceneMvrBytes) +
         " scene_mvr_memory_restores=" +
         std::to_string(metrics.sceneMvrMemoryRestores) +
         " scene_mvr_temp_fallbacks=" +
         std::to_string(metrics.sceneMvrTempFallbacks) +
         " scene_mvr_temp_bytes=" +
         std::to_string(metrics.sceneMvrTempBytesWritten) +
         " config_memory_loads=" +
         std::to_string(metrics.configMemoryLoads) +
         " config_patch_ms=" +
         std::to_string(metrics.projectConfigPatchMs) +
         " project_archive_ms=" + std::to_string(metrics.projectArchiveLoadMs) +
         " mvr_restore_ms=" + std::to_string(metrics.mvrRestoreMs) +
         " user_config_ms=" + std::to_string(metrics.userConfigMs) +
         " main_window_ms=" + std::to_string(metrics.mainWindowConstructionMs) +
         " path_resolution_ms=" +
         std::to_string(metrics.startupPathResolutionMs);
}

} // namespace startup
