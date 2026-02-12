#pragma once

#include "../viewer3d_types.h"
#include "../canvas2d.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct NVGcontext;

class ISelectionContext {
public:
  using BoundingBox = Viewer3DBoundingBox;
  using VisibleSet = Viewer3DVisibleSet;
  using ViewFrustumSnapshot = Viewer3DViewFrustumSnapshot;

  virtual ~ISelectionContext() = default;

  virtual void ApplyHighlightUuid(const std::string &uuid) = 0;
  virtual void ReplaceSelectedUuids(const std::vector<std::string> &uuids) = 0;
  virtual bool IsCameraMoving() const = 0;

  virtual const BoundingBox *FindFixtureBounds(const std::string &uuid) const = 0;
  virtual const BoundingBox *FindTrussBounds(const std::string &uuid) const = 0;
  virtual const BoundingBox *FindObjectBounds(const std::string &uuid) const = 0;

  virtual const VisibleSet &
  GetVisibleSet(const ViewFrustumSnapshot &frustum,
                const std::unordered_set<std::string> &hiddenLayers,
                bool useFrustumCulling, float minPixels) const = 0;

  virtual const std::string &GetHighlightUuid() const = 0;
  virtual const std::unordered_map<std::string, BoundingBox> &
  GetFixtureBoundsMap() const = 0;
  virtual const std::unordered_map<std::string, BoundingBox> &
  GetTrussBoundsMap() const = 0;
  virtual const std::unordered_map<std::string, BoundingBox> &
  GetObjectBoundsMap() const = 0;
  virtual NVGcontext *GetNanoVGContext() const = 0;
  virtual int GetLabelFont() const = 0;
  virtual int GetLabelBoldFont() const = 0;
  virtual bool IsDarkMode() const = 0;
  virtual ICanvas2D *GetCaptureCanvas() const = 0;
  virtual void RecordText(float x, float y, const std::string &text,
                          const CanvasTextStyle &style) const = 0;
};
