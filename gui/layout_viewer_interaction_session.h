#pragma once

#include "LayoutCollection.h"

#include <optional>

namespace gui::layoutinteraction {

struct Point {
  int x = 0;
  int y = 0;
};

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

enum class FrameDragMode {
  None,
  Move,
  ResizeRight,
  ResizeBottom,
  ResizeCorner
};

class LayoutViewerInteractionSession {
public:
  void BeginFrameDrag(FrameDragMode mode, Point pointer,
                      const layouts::Layout2DViewFrame &frame);
  void CompleteFrameDrag();
  void UpdateHoverMode(FrameDragMode mode);
  void SetDeferredResize(const layouts::Layout2DViewFrame &frame);
  void BeginPan(Point pointer);
  Point UpdatePan(Point pointer);
  void EndPan();
  void CancelAfterCaptureLoss();
  void ResetForLayoutReplacement();

  FrameDragMode DragMode() const;
  FrameDragMode HoverMode() const;
  Point DragStartPointer() const;
  const layouts::Layout2DViewFrame &DragStartFrame() const;
  const std::optional<layouts::Layout2DViewFrame> &DeferredResize() const;
  bool IsPanning() const;

private:
  FrameDragMode dragMode_ = FrameDragMode::None;
  FrameDragMode hoverMode_ = FrameDragMode::None;
  Point dragStartPointer_;
  layouts::Layout2DViewFrame dragStartFrame_;
  std::optional<layouts::Layout2DViewFrame> deferredResize_;
  bool isPanning_ = false;
  Point lastPanPointer_;
};

FrameDragMode HitTestFrame(Point pointer, Rect frame);
layouts::Layout2DViewFrame ComputeDraggedFrame(
    FrameDragMode mode, const layouts::Layout2DViewFrame &startFrame,
    Point startPointer, Point currentPointer, double zoom,
    std::optional<double> imageAspectRatio = std::nullopt);
layouts::Layout2DViewFrame FinalizeMovedFrame(
    const layouts::Layout2DViewFrame &frame);
layouts::Layout2DViewFrame FinalizeResizedFrame(
    const layouts::Layout2DViewFrame &frame);

} // namespace gui::layoutinteraction
