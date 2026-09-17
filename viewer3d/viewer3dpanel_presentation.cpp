#include "viewer3dpanel.h"

#include <chrono>

// Presents the newest transform at a bounded interactive frame cadence.
void Viewer3DPanel::PresentInteractiveTransformFrame() {
    Refresh(false);
    if (!m_paintInProgress &&
        m_interactivePresentationCadence.IsPresentationDue(
            std::chrono::steady_clock::now()))
        Update();
}

// Flushes the final transform and reconciles transform-dependent caches once.
void Viewer3DPanel::FinishInteractiveTransformPresentation() {
    if (m_activeTransformTargets.empty())
        return;
    m_activeTransformTargets.clear();
    m_controller.MarkSceneTransformsDirty();
    m_interactivePresentationCadence.Reset();
    Refresh(false);
    if (!m_paintInProgress)
        Update();
}

// Aligns the provisional fixture with the raw view-plane position under the
// pointer.
bool Viewer3DPanel::AlignContinuousElementToPointer(const wxPoint &mousePos) {
    const RenderSize renderSize = ResolveRenderSize(this);
    if (!renderSize.IsValid() ||
        !TryBindGlContextForInteraction("continuous element alignment")) {
        return false;
    }

    ApplyCameraMatrices(renderSize);
    const auto rawAnchor = CurrentRawSelectionDragAnchor();
    const auto pointer =
        ProjectMouseToSelectionDragViewPlane(mousePos, renderSize, rawAnchor);
    if (!pointer)
        return false;

    RestorePendingMagnetSnapPreview();
    ApplySelectionDragDelta(continuous_placement::AbsoluteAlignmentDelta(
        *pointer, m_selectionDragSession.AnchorMeters()));
    m_continuousPlacementSession.CompleteAlignmentAttempt(true);
    m_selectionDragSession.ClearAxis();
    m_continuousPlacementSession.ClearConstraintReferencePreservingAxisSwitch();
    m_continuousPlacementSession.SetAxisSwitchArmed(true);
    m_selectionDragActivation.MarkMoved();
    m_lastMousePos = mousePos;
    return true;
}
