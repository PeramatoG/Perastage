#include "render_pipeline.h"

#include "logger.h"
#include "viewer3dcontroller.h"

#include <cassert>
#include <sstream>

namespace {
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

RenderPipeline::RenderPipeline(Viewer3DController &controller)
    : m_controller(controller) {}

RenderPipeline::~RenderPipeline() {
  if (m_framePrepared) {
    Logger::Instance().Log(Logger::Level::Warn,
                           BuildTeardownDiagnostic(m_context, m_visibleSet,
                                                   m_framePrepared));
    FinalizeFrame();
  }
}

void RenderPipeline::Execute(const RenderFrameContext &context) {
  struct FinalizeGuard {
    RenderPipeline &pipeline;
    ~FinalizeGuard() { pipeline.FinalizeFrame(); }
  } guard{*this};

  PrepareFrame(context);
  RenderOpaque();
  RenderOverlays();
}

void RenderPipeline::PrepareFrame(const RenderFrameContext &context) {
  assert(!m_framePrepared && "RenderPipeline invariant violated: frame already prepared");
  assert(!m_visibleSet &&
         "RenderPipeline invariant violated: visible set must be null before preparation");
  m_context = context;
  m_framePrepared = true;
  m_visibleSet = &m_controller.PrepareRenderFrame(m_context, m_frustum);
}

void RenderPipeline::RenderOpaque() {
  EnsureVisibleSetPrepared("RenderOpaque");
  m_controller.RenderOpaqueFrame(m_context, *m_visibleSet);
}

void RenderPipeline::RenderOverlays() {
  EnsureVisibleSetPrepared("RenderOverlays");
  m_controller.RenderOverlayFrame(m_context, *m_visibleSet);
}

void RenderPipeline::FinalizeFrame() {
  if (!m_framePrepared)
    return;

  m_controller.FinalizeRenderFrame();
  m_visibleSet = nullptr;
  m_framePrepared = false;
}

void RenderPipeline::EnsureVisibleSetPrepared(const char *phase) const {
  (void)phase;
  assert(m_framePrepared && "RenderPipeline invariant violated: frame not prepared");
  assert(m_visibleSet && "RenderPipeline invariant violated: visible set must be prepared");
}
