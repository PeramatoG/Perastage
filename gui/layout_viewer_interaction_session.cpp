#include "layout_viewer_interaction_session.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kHandleSize = 10;
constexpr int kHandleHalf = kHandleSize / 2;
constexpr int kHandleHoverPadding = 6;
constexpr int kMinimumFrameSize = 24;
constexpr int kLayoutGridStep = 5;

// Reports whether a point lies in a rectangle using inclusive pixel origins.
bool Contains(gui::layoutinteraction::Rect rect,
              gui::layoutinteraction::Point point) {
  return rect.width > 0 && rect.height > 0 && point.x >= rect.x &&
         point.y >= rect.y && point.x < rect.x + rect.width &&
         point.y < rect.y + rect.height;
}

// Snaps an integer coordinate to the layout editor grid.
int SnapToGrid(int value) {
  return static_cast<int>(
      std::lround(static_cast<double>(value) / kLayoutGridStep) *
      kLayoutGridStep);
}
} // namespace

namespace gui::layoutinteraction {

// Starts a frame-edit gesture from the supplied pointer and frame snapshot.
void LayoutViewerInteractionSession::BeginFrameDrag(
    FrameDragMode mode, Point pointer,
    const layouts::Layout2DViewFrame &frame) {
  dragMode_ = mode;
  dragStartPointer_ = pointer;
  dragStartFrame_ = frame;
  deferredResize_.reset();
  isPanning_ = false;
}

// Returns the session to a neutral state after a completed frame edit.
void LayoutViewerInteractionSession::CompleteFrameDrag() {
  dragMode_ = FrameDragMode::None;
  deferredResize_.reset();
}

// Records the frame interaction currently under the pointer.
void LayoutViewerInteractionSession::UpdateHoverMode(FrameDragMode mode) {
  hoverMode_ = mode;
}

// Stores the latest resize preview without mutating the layout model.
void LayoutViewerInteractionSession::SetDeferredResize(
    const layouts::Layout2DViewFrame &frame) {
  deferredResize_ = frame;
}

// Starts a viewport pan gesture at the supplied pointer.
void LayoutViewerInteractionSession::BeginPan(Point pointer) {
  isPanning_ = true;
  lastPanPointer_ = pointer;
  dragMode_ = FrameDragMode::None;
  deferredResize_.reset();
}

// Advances a pan gesture and returns its pointer delta.
Point LayoutViewerInteractionSession::UpdatePan(Point pointer) {
  const Point delta{pointer.x - lastPanPointer_.x,
                    pointer.y - lastPanPointer_.y};
  lastPanPointer_ = pointer;
  return delta;
}

// Ends the active viewport pan gesture.
void LayoutViewerInteractionSession::EndPan() { isPanning_ = false; }

// Cancels all gesture state after native mouse capture is lost.
void LayoutViewerInteractionSession::CancelAfterCaptureLoss() {
  isPanning_ = false;
  dragMode_ = FrameDragMode::None;
  deferredResize_.reset();
}

// Clears pointer interaction state when the represented layout is replaced.
void LayoutViewerInteractionSession::ResetForLayoutReplacement() {
  CancelAfterCaptureLoss();
  hoverMode_ = FrameDragMode::None;
}

// Returns the active frame drag mode.
FrameDragMode LayoutViewerInteractionSession::DragMode() const {
  return dragMode_;
}

// Returns the current frame hover mode.
FrameDragMode LayoutViewerInteractionSession::HoverMode() const {
  return hoverMode_;
}

// Returns the pointer position captured at frame-drag start.
Point LayoutViewerInteractionSession::DragStartPointer() const {
  return dragStartPointer_;
}

// Returns the frame snapshot captured at frame-drag start.
const layouts::Layout2DViewFrame &
LayoutViewerInteractionSession::DragStartFrame() const {
  return dragStartFrame_;
}

// Returns the current resize preview, if any.
const std::optional<layouts::Layout2DViewFrame> &
LayoutViewerInteractionSession::DeferredResize() const {
  return deferredResize_;
}

// Reports whether the viewport is currently being panned.
bool LayoutViewerInteractionSession::IsPanning() const { return isPanning_; }

// Resolves frame interior and resize-handle hits using editor tolerances.
FrameDragMode HitTestFrame(Point pointer, Rect frame) {
  const int right = frame.x + frame.width - 1;
  const int bottom = frame.y + frame.height - 1;
  const int handleExtent = kHandleSize + kHandleHoverPadding * 2;
  const Rect rightHandle{right - kHandleHalf - kHandleHoverPadding,
                         frame.y + frame.height / 2 - kHandleHalf -
                             kHandleHoverPadding,
                         handleExtent, handleExtent};
  const Rect bottomHandle{frame.x + frame.width / 2 - kHandleHalf -
                              kHandleHoverPadding,
                          bottom - kHandleHalf - kHandleHoverPadding,
                          handleExtent, handleExtent};
  const Rect cornerHandle{right - kHandleHalf - kHandleHoverPadding,
                          bottom - kHandleHalf - kHandleHoverPadding,
                          handleExtent, handleExtent};
  if (Contains(cornerHandle, pointer))
    return FrameDragMode::ResizeCorner;
  if (Contains(rightHandle, pointer))
    return FrameDragMode::ResizeRight;
  if (Contains(bottomHandle, pointer))
    return FrameDragMode::ResizeBottom;
  if (Contains(frame, pointer))
    return FrameDragMode::Move;
  return FrameDragMode::None;
}

// Computes the unsnapped candidate frame for a pointer drag.
layouts::Layout2DViewFrame ComputeDraggedFrame(
    FrameDragMode mode, const layouts::Layout2DViewFrame &startFrame,
    Point startPointer, Point currentPointer, double zoom,
    std::optional<double> imageAspectRatio) {
  const Point logicalDelta{
      static_cast<int>(std::lround((currentPointer.x - startPointer.x) / zoom)),
      static_cast<int>(std::lround((currentPointer.y - startPointer.y) / zoom))};
  layouts::Layout2DViewFrame frame = startFrame;
  if (mode == FrameDragMode::Move) {
    frame.x += logicalDelta.x;
    frame.y += logicalDelta.y;
    return frame;
  }

  const double ratio = imageAspectRatio.value_or(0.0);
  if (ratio > 0.0) {
    const bool useHeight =
        mode == FrameDragMode::ResizeBottom ||
        (mode == FrameDragMode::ResizeCorner &&
         std::abs(logicalDelta.y) > std::abs(logicalDelta.x));
    if (mode == FrameDragMode::ResizeRight ||
        mode == FrameDragMode::ResizeCorner) {
      frame.width =
          std::max(kMinimumFrameSize, startFrame.width + logicalDelta.x);
      frame.height = std::max(
          kMinimumFrameSize,
          static_cast<int>(std::lround(frame.width / ratio)));
    }
    if (mode == FrameDragMode::ResizeBottom ||
        mode == FrameDragMode::ResizeCorner) {
      const int candidateHeight =
          std::max(kMinimumFrameSize, startFrame.height + logicalDelta.y);
      const int candidateWidth = std::max(
          kMinimumFrameSize,
          static_cast<int>(std::lround(candidateHeight * ratio)));
      if (mode == FrameDragMode::ResizeBottom ||
          std::abs(logicalDelta.y) > std::abs(logicalDelta.x)) {
        frame.height = candidateHeight;
        frame.width = candidateWidth;
      }
    }
    if (useHeight) {
      frame.height = std::max(kMinimumFrameSize, frame.height);
      frame.width = std::max(
          kMinimumFrameSize,
          static_cast<int>(std::lround(frame.height * ratio)));
    } else {
      frame.width = std::max(kMinimumFrameSize, frame.width);
      frame.height = std::max(
          kMinimumFrameSize,
          static_cast<int>(std::lround(frame.width / ratio)));
    }
    return frame;
  }

  if (mode == FrameDragMode::ResizeRight ||
      mode == FrameDragMode::ResizeCorner) {
    frame.width =
        std::max(kMinimumFrameSize, startFrame.width + logicalDelta.x);
  }
  if (mode == FrameDragMode::ResizeBottom ||
      mode == FrameDragMode::ResizeCorner) {
    frame.height =
        std::max(kMinimumFrameSize, startFrame.height + logicalDelta.y);
  }
  return frame;
}

// Snaps a completed move while preserving the frame dimensions.
layouts::Layout2DViewFrame FinalizeMovedFrame(
    const layouts::Layout2DViewFrame &frame) {
  layouts::Layout2DViewFrame result = frame;
  result.x = SnapToGrid(result.x);
  result.y = SnapToGrid(result.y);
  return result;
}

// Snaps a completed resize while enforcing the editor minimum dimensions.
layouts::Layout2DViewFrame FinalizeResizedFrame(
    const layouts::Layout2DViewFrame &frame) {
  layouts::Layout2DViewFrame result = frame;
  result.width =
      std::max(kMinimumFrameSize, SnapToGrid(result.width));
  result.height =
      std::max(kMinimumFrameSize, SnapToGrid(result.height));
  return result;
}

} // namespace gui::layoutinteraction
