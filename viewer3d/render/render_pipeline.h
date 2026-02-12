#pragma once

#include "viewer3d_types.h"

class Viewer3DController;

class RenderPipeline {
public:
  explicit RenderPipeline(Viewer3DController &controller);

  void PrepareFrame(const RenderFrameContext &context);
  void RenderOpaque();
  void RenderOverlays();
  void FinalizeFrame();

private:
  Viewer3DController &m_controller;
  RenderFrameContext m_context;
  Viewer3DViewFrustumSnapshot m_frustum{};
  const Viewer3DVisibleSet *m_visibleSet = nullptr;
};
