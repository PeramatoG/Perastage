#pragma once

#include "canvas2d.h"
#include <string>

class IRenderContext {
public:
  virtual ~IRenderContext() = default;

  virtual bool IsInteracting() const = 0;
  virtual bool UseAdaptiveLineProfile() const = 0;
  virtual bool SkipOutlinesForCurrentFrame() const = 0;
  virtual bool IsSelectionOutlineEnabled2D() const = 0;
  virtual bool IsUuidHighlighted(const std::string &uuid) const = 0;
  virtual bool IsUuidSelected(const std::string &uuid) const = 0;
  virtual bool IsCaptureOnly() const = 0;
  virtual ICanvas2D *GetCaptureCanvas() const = 0;
  virtual bool CaptureIncludesGrid() const = 0;
  virtual bool IsWhiteModelStyleEnabled() const = 0;
  virtual bool IsSketchRenderStyleEnabled() const = 0;
  virtual bool IsPureWhiteRenderStyleEnabled() const = 0;
  virtual bool IsTexturedRenderStyleEnabled() const = 0;

  virtual void SetGLColor(float r, float g, float b) const = 0;
  virtual void RecordLine(const std::array<float, 3> &a,
                          const std::array<float, 3> &b,
                          const CanvasStroke &stroke) const = 0;
  virtual void RecordPolygon(const std::vector<std::array<float, 3>> &points,
                             const CanvasStroke &stroke,
                             const CanvasFill *fill) const = 0;
};
