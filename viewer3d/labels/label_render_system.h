#pragma once

#include "../viewer3dcontroller.h"

class LabelRenderSystem {
public:
  explicit LabelRenderSystem(Viewer3DController &controller)
      : m_controller(controller) {}

  void DrawFixtureLabels(int width, int height);
  void DrawTrussLabels(int width, int height);
  void DrawSceneObjectLabels(int width, int height);
  void DrawAllFixtureLabels(int width, int height, Viewer2DView view,
                            float zoom);

private:
  Viewer3DController &m_controller;
};
