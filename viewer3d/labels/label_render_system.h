#pragma once

#include "iselectioncontext.h"
#include "viewer3d_types.h"

class LabelRenderSystem {
public:
  explicit LabelRenderSystem(ISelectionContext &controller)
      : m_controller(controller) {}

  void DrawFixtureLabels(int width, int height);
  void DrawTrussLabels(int width, int height);
  void DrawSceneObjectLabels(int width, int height);
  void DrawAllFixtureLabels(int width, int height, Viewer2DView view,
                            float zoom);

private:
  ISelectionContext &m_controller;
};
