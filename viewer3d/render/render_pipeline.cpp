#include "render_pipeline.h"

#include "logger.h"
#include "sketch_post_process_pass.h"
#include "viewer3dcontroller.h"

#include <cassert>
#include <sstream>

namespace {
// Builds a diagnostic for a pipeline destroyed before frame finalization.
std::string BuildTeardownDiagnostic(const RenderFrameContext &context,
                                    const Viewer3DVisibleSet *visibleSet,
                                    bool framePrepared) {
  std::ostringstream oss;
  oss << "RenderPipeline teardown with pending frame"
      << " [framePrepared=" << framePrepared
      << ", visibleSetPrepared=" << (visibleSet != nullptr)
      << ", is2DViewer=" << context.is2DViewer
      << ", wireframe=" << context.wireframe
      << ", useFrustumCulling=" << context.useFrustumCulling
      << ", hiddenLayers=" << context.hiddenLayers.size() << ']';
  return oss.str();
}
} // namespace

// Initializes a render pipeline for one controller.
RenderPipeline::RenderPipeline(Viewer3DController &controller)
    : m_controller(controller) {}

// Finalizes a prepared frame if pipeline execution exits early.
RenderPipeline::~RenderPipeline() {
  if (m_framePrepared) {
    Logger::Instance().Log(Logger::Level::Warn,
                           BuildTeardownDiagnostic(m_context, m_visibleSet,
                                                   m_framePrepared));
    FinalizeFrame();
  }
}

// Executes opaque, optional Sketch post-process, and overlay stages for one frame.
void RenderPipeline::Execute(const RenderFrameContext &context) {
  struct FinalizeGuard {
    RenderPipeline &pipeline;
    ~FinalizeGuard() { pipeline.FinalizeFrame(); }
  } guard{*this};

  PrepareFrame(context);
  if (ShouldApplySketchPostProcess(m_context.sketchPostProcess,
                                   m_context.wireframe,
                                   m_context.idOnlyPass)) {
    RenderFrameContext baseContext = m_context;
    baseContext.whiteModelStyle = false;
    baseContext.sketchBasePass = true;
    // Keep transient selection colors out of the posterized Sketch image;
    // the post-composite overlay owns their final filled-style appearance.
    baseContext.suppressSelectionStyling =
        ShouldSuppressSketchBaseSelection(true);
    m_controller.SetSketchBasePassActive(true);
    const bool hasIntermediateTarget = m_controller.BeginSketchPostProcess();
    m_controller.RenderOpaqueFrame(baseContext, *m_visibleSet);
    m_controller.SetSketchBasePassActive(false);
    if (hasIntermediateTarget)
      m_controller.CompleteSketchPostProcess();
  } else {
    RenderOpaque();
  }
  RenderOverlays();
}

// Prepares frame state and resolves the visible scene set.
void RenderPipeline::PrepareFrame(const RenderFrameContext &context) {
  assert(!m_framePrepared && "RenderPipeline invariant violated: frame already prepared");
  assert(!m_visibleSet &&
         "RenderPipeline invariant violated: visible set must be null before preparation");
  m_context = context;
  m_framePrepared = true;
  m_visibleSet = &m_controller.PrepareRenderFrame(m_context, m_frustum);
}

// Renders the normal opaque scene stage.
void RenderPipeline::RenderOpaque() {
  EnsureVisibleSetPrepared("RenderOpaque");
  m_controller.RenderOpaqueFrame(m_context, *m_visibleSet);
}

// Renders overlays after opaque and optional Sketch composition.
void RenderPipeline::RenderOverlays() {
  EnsureVisibleSetPrepared("RenderOverlays");
  m_controller.RenderOverlayFrame(m_context, *m_visibleSet);
}

// Finalizes controller frame state and clears pipeline references.
void RenderPipeline::FinalizeFrame() {
  if (!m_framePrepared)
    return;

  m_controller.FinalizeRenderFrame();
  m_visibleSet = nullptr;
  m_framePrepared = false;
}

// Verifies that frame preparation produced a visible set.
void RenderPipeline::EnsureVisibleSetPrepared(const char *phase) const {
  (void)phase;
  assert(m_framePrepared && "RenderPipeline invariant violated: frame not prepared");
  assert(m_visibleSet && "RenderPipeline invariant violated: visible set must be prepared");
}
