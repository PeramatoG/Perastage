#include "viewer3dpanel.h"

// Repaints an interactive transform before further mouse events run.
void Viewer3DPanel::PresentInteractiveTransformFrame() {
    wxASSERT_MSG(wxIsMainThread(),
                 "Interactive presentation must run on the UI thread.");
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
