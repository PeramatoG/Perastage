#include "render_pipeline.h"

#include "viewer3dcontroller.h"

#include <cassert>

RenderPipeline::RenderPipeline(Viewer3DController &controller)
    : m_controller(controller) {}

RenderPipeline::~RenderPipeline() {
  if (m_framePrepared) {
    assert(false && "RenderPipeline invariant violated: FinalizeFrame() must be called");
    FinalizeFrame();
  }
}

void RenderPipeline::Execute(const RenderFrameContext &context) {
  PrepareFrame(context);

  struct FinalizeGuard {
    RenderPipeline &pipeline;
    ~FinalizeGuard() { pipeline.FinalizeFrame(); }
  } guard{*this};

  RenderOpaque();
  RenderOverlays();
}

void RenderPipeline::PrepareFrame(const RenderFrameContext &context) {
  assert(!m_framePrepared && "RenderPipeline invariant violated: frame already prepared");
  m_context = context;
  m_visibleSet = &m_controller.PrepareRenderFrame(m_context, m_frustum);
  m_framePrepared = true;
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
