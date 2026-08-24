#include "iselectioncontext.h"
#include "viewer2dviewfit.h"

#include <cassert>
#include <cmath>

namespace {

class SelectionContextStub final : public ISelectionContext {
public:
  std::unordered_map<std::string, BoundingBox> fixtureBounds;

  void ApplyHighlightUuid(const std::string &) override {}
  void ReplaceSelectedUuids(const std::vector<std::string> &) override {}
  bool IsCameraMoving() const override { return false; }
  const BoundingBox *FindFixtureBounds(const std::string &) const override {
    return nullptr;
  }
  const BoundingBox *FindTrussBounds(const std::string &) const override {
    return nullptr;
  }
  const BoundingBox *FindObjectBounds(const std::string &) const override {
    return nullptr;
  }
  const VisibleSet &GetVisibleSet(
      const ViewFrustumSnapshot &, const std::unordered_set<std::string> &,
      bool, float) const override {
    return visible;
  }
  const std::string &GetHighlightUuid() const override { return text; }
  const std::unordered_set<std::string> &GetSelectedUuids() const override {
    return selected;
  }
  const std::unordered_map<std::string, BoundingBox> &
  GetFixtureBoundsMap() const override {
    return fixtureBounds;
  }
  const std::unordered_map<std::string, BoundingBox> &
  GetTrussBoundsMap() const override {
    return emptyBounds;
  }
  const std::unordered_map<std::string, BoundingBox> &
  GetObjectBoundsMap() const override {
    return emptyBounds;
  }
  NVGcontext *GetNanoVGContext() const override { return nullptr; }
  int GetLabelFont() const override { return -1; }
  int GetLabelBoldFont() const override { return -1; }
  bool IsDarkMode() const override { return false; }
  ICanvas2D *GetCaptureCanvas() const override { return nullptr; }
  void RecordText(float, float, const std::string &,
                  const CanvasTextStyle &) const override {}
  PickReadResult ReadPickUuidAtDetailed(
      int, int, int, int, const std::unordered_set<std::string> &,
      std::string &) override {
    return PickReadResult::Unavailable;
  }
  bool ReadPickUuidAt(int, int, int, int,
                      const std::unordered_set<std::string> &,
                      std::string &) override {
    return false;
  }

private:
  VisibleSet visible;
  std::string text;
  std::unordered_set<std::string> selected;
  std::unordered_map<std::string, BoundingBox> emptyBounds;
};

// Reports whether two computed fit values agree within float precision.
bool Near(float left, float right) { return std::fabs(left - right) < 0.0001f; }

} // namespace

// Verifies fallback diagnosis and explicit-bounds fit parity for every view.
int main() {
  Viewer3DBoundingBox fallback;
  fallback.min = {-0.1f, -0.1f, -0.1f};
  fallback.max = {0.1f, 0.1f, 0.1f};
  viewer2d::ViewFitResult fallbackFit;
  assert(viewer2d::ComputeViewFitForBounds(
      fallback, Viewer2DView::Front, 1103, 1200, fallbackFit));
  assert(Near(fallbackFit.zoom, 153.460876f));
  assert(Near(fallbackFit.offsetXPixels, 0.0f));
  assert(Near(fallbackFit.offsetYPixels, 0.0f));

  Viewer3DBoundingBox actual;
  actual.min = {-0.45f, -0.32f, -0.58f};
  actual.max = {0.45f, 0.32f, 0.58f};
  SelectionContextStub selection;
  selection.fixtureBounds["fixture"] = actual;
  for (Viewer2DView view : {Viewer2DView::Top, Viewer2DView::Bottom,
                            Viewer2DView::Front, Viewer2DView::Side}) {
    viewer2d::ViewFitResult explicitFit;
    viewer2d::ViewFitResult selectionFit;
    assert(viewer2d::ComputeViewFitForBounds(actual, view, 1103, 1200,
                                             explicitFit));
    assert(viewer2d::ComputeViewFit(selection, view, 1103, 1200,
                                    selectionFit));
    assert(Near(explicitFit.zoom, selectionFit.zoom));
    assert(Near(explicitFit.offsetXPixels, selectionFit.offsetXPixels));
    assert(Near(explicitFit.offsetYPixels, selectionFit.offsetYPixels));
  }
  viewer2d::ViewFitResult actualFit;
  assert(viewer2d::ComputeViewFitForBounds(
      actual, Viewer2DView::Front, 1103, 1200, actualFit));
  assert(actualFit.zoom < fallbackFit.zoom);

  Viewer3DBoundingBox quantumWash;
  quantumWash.min = {-0.228000f, -0.158000f, -0.503910f};
  quantumWash.max = {0.228000f, 0.158000f, -0.001200f};
  viewer2d::ViewFitResult frontFit;
  assert(viewer2d::ComputeViewFitForBounds(
      quantumWash, Viewer2DView::Front, 1103, 1200, frontFit));
  assert(Near(frontFit.zoom, 83.028248f));
  assert(Near(frontFit.offsetXPixels, 0.0f));
  assert(Near(frontFit.offsetYPixels, 6.313875f));

  viewer2d::ViewFitResult sideFit;
  assert(viewer2d::ComputeViewFitForBounds(
      quantumWash, Viewer2DView::Side, 764, 1200, sideFit));
  assert(Near(sideFit.zoom, 83.028248f));
  assert(Near(sideFit.offsetXPixels, 0.0f));
  assert(Near(sideFit.offsetYPixels, 6.313875f));

  for (Viewer2DView view : {Viewer2DView::Top, Viewer2DView::Bottom}) {
    viewer2d::ViewFitResult planFit;
    assert(viewer2d::ComputeViewFitForBounds(quantumWash, view, 1200, 831,
                                             planFit));
    assert(Near(planFit.zoom, 91.469455f));
    assert(Near(planFit.offsetXPixels, 0.0f));
    assert(Near(planFit.offsetYPixels, 0.0f));
  }
  return 0;
}
