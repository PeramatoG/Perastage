#include "layout_viewer_interaction_session.h"

#include <cassert>

namespace {
using gui::layoutinteraction::FrameDragMode;
using gui::layoutinteraction::LayoutViewerInteractionSession;
using gui::layoutinteraction::Point;
using gui::layoutinteraction::Rect;

// Reports whether two layout frames contain identical geometry.
bool FramesEqual(const layouts::Layout2DViewFrame &lhs,
                 const layouts::Layout2DViewFrame &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
         lhs.height == rhs.height;
}

// Verifies transient frame, hover, pan, completion, and reset state.
void TestSessionLifecycle() {
  LayoutViewerInteractionSession session;
  assert(session.DragMode() == FrameDragMode::None);
  assert(session.HoverMode() == FrameDragMode::None);
  assert(!session.IsPanning());
  assert(!session.DeferredResize());

  const layouts::Layout2DViewFrame start{10, 20, 100, 80};
  session.UpdateHoverMode(FrameDragMode::ResizeRight);
  session.BeginFrameDrag(FrameDragMode::ResizeCorner, {30, 40}, start);
  assert(session.DragMode() == FrameDragMode::ResizeCorner);
  assert(session.HoverMode() == FrameDragMode::ResizeRight);
  assert(session.DragStartPointer().x == 30);
  assert(FramesEqual(session.DragStartFrame(), start));
  session.SetDeferredResize({10, 20, 120, 90});
  assert(session.DeferredResize()->width == 120);
  session.CompleteFrameDrag();
  assert(session.DragMode() == FrameDragMode::None);
  assert(!session.DeferredResize());

  session.BeginPan({5, 7});
  assert(session.IsPanning());
  const Point delta = session.UpdatePan({2, 12});
  assert(delta.x == -3 && delta.y == 5);
  session.EndPan();
  assert(!session.IsPanning());

  session.BeginFrameDrag(FrameDragMode::Move, {1, 2}, start);
  session.CancelAfterCaptureLoss();
  assert(session.DragMode() == FrameDragMode::None);
  assert(!session.IsPanning());
  session.UpdateHoverMode(FrameDragMode::Move);
  session.ResetForLayoutReplacement();
  assert(session.HoverMode() == FrameDragMode::None);
}

// Verifies resize handles, interior movement, padding boundaries, and misses.
void TestHitTesting() {
  const Rect frame{100, 100, 100, 80};
  assert(gui::layoutinteraction::HitTestFrame({120, 120}, frame) ==
         FrameDragMode::Move);
  assert(gui::layoutinteraction::HitTestFrame({199, 140}, frame) ==
         FrameDragMode::ResizeRight);
  assert(gui::layoutinteraction::HitTestFrame({150, 179}, frame) ==
         FrameDragMode::ResizeBottom);
  assert(gui::layoutinteraction::HitTestFrame({199, 179}, frame) ==
         FrameDragMode::ResizeCorner);
  assert(gui::layoutinteraction::HitTestFrame({50, 50}, frame) ==
         FrameDragMode::None);
  assert(gui::layoutinteraction::HitTestFrame({188, 129}, frame) ==
         FrameDragMode::ResizeRight);
  assert(gui::layoutinteraction::HitTestFrame({210, 129}, frame) ==
         FrameDragMode::None);
  assert(gui::layoutinteraction::HitTestFrame({103, 103}, {100, 100, 8, 8}) ==
         FrameDragMode::ResizeCorner);
}

// Verifies move and ordinary resize geometry at multiple zoom levels.
void TestFrameGeometry() {
  const layouts::Layout2DViewFrame start{10, 20, 100, 80};
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::Move, start, {0, 0}, {20, -10}, 1.0),
                     {30, 10, 100, 80}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::Move, start, {0, 0}, {-20, 10}, 2.0),
                     {0, 25, 100, 80}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::Move, start, {0, 0}, {5, 5}, 0.5),
                     {20, 30, 100, 80}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeRight, start, {0, 0}, {30, 9},
                         1.0),
                     {10, 20, 130, 80}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeBottom, start, {0, 0}, {9, 30},
                         1.0),
                     {10, 20, 100, 110}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeCorner, start, {0, 0}, {30, 40},
                         1.0),
                     {10, 20, 130, 120}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeCorner, start, {0, 0},
                         {-1000, -1000}, 1.0),
                     {10, 20, 24, 24}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::None, start, {0, 0}, {50, 50}, 1.0),
                     start));
}

// Verifies the existing width-, height-, and dominant-axis image ratio rules.
void TestAspectRatioGeometry() {
  const layouts::Layout2DViewFrame start{10, 20, 100, 50};
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeRight, start, {0, 0}, {20, 5},
                         1.0, 2.0),
                     {10, 20, 120, 60}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeBottom, start, {0, 0}, {5, 20},
                         1.0, 2.0),
                     {10, 20, 140, 70}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeCorner, start, {0, 0}, {30, 10},
                         1.0, 2.0),
                     {10, 20, 130, 65}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeCorner, start, {0, 0}, {10, 30},
                         1.0, 2.0),
                     {10, 20, 160, 80}));
  assert(FramesEqual(gui::layoutinteraction::ComputeDraggedFrame(
                         FrameDragMode::ResizeRight, start, {0, 0}, {-200, 0},
                         1.0, 2.0),
                     {10, 20, 24, 24}));
}

// Verifies grid snapping occurs only in explicit finalization helpers.
void TestFinalization() {
  assert(FramesEqual(gui::layoutinteraction::FinalizeMovedFrame(
                         {12, -7, 101, 79}),
                     {10, -5, 101, 79}));
  assert(FramesEqual(gui::layoutinteraction::FinalizeResizedFrame(
                         {12, -7, 102, 78}),
                     {12, -7, 100, 80}));
  assert(FramesEqual(gui::layoutinteraction::FinalizeResizedFrame(
                         {0, 0, 23, 1}),
                     {0, 0, 25, 24}));
}
} // namespace

// Runs deterministic interaction-session and frame-policy checks.
int main() {
  TestSessionLifecycle();
  TestHitTesting();
  TestFrameGeometry();
  TestAspectRatioGeometry();
  TestFinalization();
  return 0;
}
