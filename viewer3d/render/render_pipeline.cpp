#include "render_pipeline.h"

#include "../viewer3dcontroller.h"

RenderPipeline::RenderPipeline(Viewer3DController &controller)
    : m_controller(controller) {}

void RenderPipeline::PrepareFrame(const RenderFrameContext &context) {
  m_context = context;
  m_visibleSet = &m_controller.PrepareRenderFrame(m_context, m_frustum);
}

void RenderPipeline::RenderOpaque() {
  if (!m_visibleSet)
    return;
  m_controller.RenderOpaqueFrame(m_context, *m_visibleSet);
}

void RenderPipeline::RenderOverlays() {
  if (!m_visibleSet)
    return;
  m_controller.RenderOverlayFrame(m_context, *m_visibleSet);
}

void RenderPipeline::FinalizeFrame() { m_visibleSet = nullptr; }
