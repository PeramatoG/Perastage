#include "viewer3dpanel.h"

#include "ui_render_size.h"

#include <algorithm>
#include <cmath>
#include "configmanager.h"
#include "selection_movement_settings.h"

// Requests a paint so pending pointer events can be coalesced by the event loop.
void Viewer3DPanel::PresentInteractiveTransformFrame() {
    if (m_transformPresentation.RequestPaint())
        Refresh(false);
}

// Clears provisional ownership and reconciles transform-dependent caches.
void Viewer3DPanel::FinishInteractiveTransformPresentation() {
    m_placementPerformance.Finish("Viewer3D");
    m_transformPresentation.ResetTransform();
    m_activeTransformRevision.reset();
    m_transformProjection = {};
    m_fixtureMagnetTargets.Clear();
    if (m_runtimeState.EndTransformInteraction())
        m_runtimeState.MarkSceneChanged();
    if (m_activeTransformTargets.empty())
        return;
    m_activeTransformTargets.clear();
    m_controller.MarkSceneTransformsDirty();
    Refresh(false);
}

// Aligns the provisional fixture with the raw view-plane position under the
// pointer.
bool Viewer3DPanel::AlignContinuousElementToPointer(const wxPoint &mousePos) {
    const RenderSize renderSize = ResolveRenderSize(this);
    if (!PrepareTransformProjection(renderSize))
        return false;

    const auto rawAnchor = CurrentRawSelectionDragAnchor();
    const auto pointer =
        ProjectMouseToSelectionDragViewPlane(mousePos, renderSize, rawAnchor);
    if (!pointer)
        return false;

    ApplySelectionDragDelta(
        continuous_placement::AbsoluteAlignmentDelta(*pointer, rawAnchor), true);
    m_continuousPlacementSession.CompleteAlignmentAttempt(true);
    m_selectionDragSession.ClearAxis();
    m_continuousPlacementSession.ClearConstraintReferencePreservingAxisSwitch();
    m_continuousPlacementSession.SetAxisSwitchArmed(true);
    m_selectionDragActivation.MarkMoved();
    m_lastMousePos = mousePos;
    return true;
}

// Applies group-expanded highlights while retaining the direct selection identity.
void Viewer3DPanel::SetSelectedFixtures(const std::vector<std::string> &uuids) {
    const scene_grouping::ObjectSelection typedSelection{
        .fixtures = uuids, .trusses = uuids, .supports = uuids,
        .sceneObjects = uuids, .groupObjects = uuids};
    const std::vector<std::string> expandedUuids =
        scene_grouping::ExpandSelectionForGroupHighlights(
            ConfigManager::Get().GetScene(), typedSelection,
            selection_movement_settings::LoadInteractiveTransformPolicy(
                ConfigManager::Get()));
    if (expandedUuids == m_lastAppliedSelectionUuids &&
        uuids == m_lastAppliedPrimarySelectionUuids)
        return;
    m_lastAppliedSelectionUuids = expandedUuids;
    m_lastAppliedPrimarySelectionUuids = uuids;
    m_runtimeState.MarkSelectionChanged();
    m_controller.SetSelectedUuids(expandedUuids, uuids);
    Refresh();
}

// External feedback changes outlines only, without anchoring a viewport tooltip.
void Viewer3DPanel::SetExternalHoverUuid(const std::string &uuid) {
  if (m_hoverUuid == uuid && !m_hasHover) return;
  m_runtimeState.MarkHighlightChanged();
  m_hoverUuid = uuid;
  m_hasHover = false;
  m_controller.SetHighlightUuid(uuid);
  Refresh(false);
}

void Viewer3DPanel::UpdateContinuousPlacementPointer(const wxPoint &pos) {
    if (m_continuousPlacementSession.NeedsAlignment()) {
        m_lastMousePos = pos;
        AlignContinuousElementToPointer(pos);
        PresentInteractiveTransformFrame();
        return;
    }
    if (pos == m_lastMousePos)
        return;
    bool changed = false;
    const RenderSize renderSize = ResolveRenderSize(this);
    if (PrepareTransformProjection(renderSize)) {
        const auto rawAnchor = CurrentRawSelectionDragAnchor();
        if (m_axisConstrainedMovementEnabled &&
            !m_continuousPlacementSession.IsBatchActive()) {
            if (!m_continuousPlacementSession.HasConstraintReference()) {
                m_continuousPlacementSession.SetConstraintReference(
                    {m_lastMousePos.x, m_lastMousePos.y}, rawAnchor);
            }
            const int totalDx =
                pos.x - m_continuousPlacementSession.ConstraintPointerOrigin().x;
            const int totalDy =
                pos.y - m_continuousPlacementSession.ConstraintPointerOrigin().y;
            const auto projectedAxes = BuildProjectedDragAxes(
                renderSize, m_continuousPlacementSession.ConstraintWorldOriginMeters());
            const bool hasValidAxis = std::any_of(
                projectedAxes.begin(), projectedAxes.end(),
                [](const viewer3d::ProjectedAxis &axis) {
                    return axis.valid;
                });
            if (!hasValidAxis) {
                m_lastMousePos = pos;
                m_continuousPlacementSession.InvalidateView();
                PresentInteractiveTransformFrame();
                return;
            }
            const auto candidateAxis =
                viewer3d::SelectDragAxisFromMouseDelta(
                    totalDx, -totalDy, projectedAxes);
            if (m_selectionDragSession.Axis() == viewer3d::SelectionDragAxis::None &&
                candidateAxis != viewer3d::SelectionDragAxis::None) {
                m_selectionDragSession.SetAxis(candidateAxis);
                changed = true;
            } else if (m_selectionDragSession.Axis() !=
                       viewer3d::SelectionDragAxis::None) {
                const double activeTravelPixels = std::abs(
                    viewer3d::ComputeProjectedPixelsOnAxis(
                        totalDx, -totalDy, m_selectionDragSession.Axis(),
                        projectedAxes));
                if (!m_continuousPlacementSession.IsAxisSwitchArmed() &&
                    activeTravelPixels >= 24.0) {
                    m_continuousPlacementSession.SetAxisSwitchArmed(true);
                }
                const auto switchIntent = m_continuousPlacementSession.IsAxisSwitchArmed()
                    ? viewer3d::DetectAxisSwitchIntent(
                          totalDx, -totalDy, m_selectionDragSession.Axis(),
                          projectedAxes)
                    : viewer3d::AxisSwitchIntent{};
                if (switchIntent.axis !=
                    viewer3d::SelectionDragAxis::None) {
                    const auto switchedAxis = switchIntent.axis;
                    if (AlignContinuousElementToPointer(pos)) {
                        m_selectionDragSession.SetAxis(switchedAxis);
                        m_continuousPlacementSession.SetConstraintReference(
                            {pos.x, pos.y}, CurrentRawSelectionDragAnchor());
                        m_continuousPlacementSession.SetAxisSwitchArmed(false);
                        PresentInteractiveTransformFrame();
                        return;
                    }
                }
            }
            const int constrainedDx =
                pos.x - m_continuousPlacementSession.ConstraintPointerOrigin().x;
            const int constrainedDy =
                pos.y - m_continuousPlacementSession.ConstraintPointerOrigin().y;
            const double axisDeltaMeters =
                viewer3d::ComputeDragMetersOnAxis(
                    constrainedDx, -constrainedDy, m_selectionDragSession.Axis(),
                    projectedAxes);
            if (m_selectionDragSession.Axis() != viewer3d::SelectionDragAxis::None) {
                const auto axis =
                    GetSelectionDragAxisVector(m_selectionDragSession.Axis());
                const std::array<float, 3> desiredAnchor{
                    m_continuousPlacementSession.ConstraintWorldOriginMeters()[0] +
                        axis[0] * static_cast<float>(axisDeltaMeters),
                    m_continuousPlacementSession.ConstraintWorldOriginMeters()[1] +
                        axis[1] * static_cast<float>(axisDeltaMeters),
                    m_continuousPlacementSession.ConstraintWorldOriginMeters()[2] +
                        axis[2] * static_cast<float>(axisDeltaMeters)};
                const std::array<float, 3> worldDelta{
                    desiredAnchor[0] - rawAnchor[0],
                    desiredAnchor[1] - rawAnchor[1],
                    desiredAnchor[2] - rawAnchor[2]};
                if (worldDelta[0] != 0.0f || worldDelta[1] != 0.0f ||
                    worldDelta[2] != 0.0f) {
                    if (ApplySelectionDragDelta(worldDelta)) {
                        m_selectionDragActivation.MarkMoved();
                        changed = true;
                    }
                }
            }
        } else {
            m_selectionDragSession.ClearAxis();
            m_continuousPlacementSession.ClearConstraintReferencePreservingAxisSwitch();
            const auto lastPoint =
                ProjectMouseToSelectionDragViewPlane(
                    m_lastMousePos, renderSize, rawAnchor);
            const auto currentPoint =
                ProjectMouseToSelectionDragViewPlane(
                    pos, renderSize, rawAnchor);
            if (lastPoint && currentPoint) {
                changed = ApplySelectionDragDelta(
                    {(*currentPoint)[0] - (*lastPoint)[0],
                     (*currentPoint)[1] - (*lastPoint)[1],
                     (*currentPoint)[2] - (*lastPoint)[2]});
                if (changed)
                    m_selectionDragActivation.MarkMoved();
            } else {
                m_lastMousePos = pos;
                m_continuousPlacementSession.InvalidateView();
                PresentInteractiveTransformFrame();
                return;
            }
        }
        if (changed)
            PresentInteractiveTransformFrame();
    } else {
        m_continuousPlacementSession.InvalidateView();
    }
    m_lastMousePos = pos;
}
