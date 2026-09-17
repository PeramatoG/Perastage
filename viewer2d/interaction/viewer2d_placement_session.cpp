#include "viewer2d_placement_session.h"

#include <utility>

namespace viewer2d::interaction {

// Starts native placement with a provisional scene element.
void Viewer2DPlacementSession::BeginNative(ContinuousPlacementType type,
                                           std::string provisionalUuid) {
  Reset();
  m_mode = PlacementMode::Native;
  m_type = type;
  m_provisionalUuid = std::move(provisionalUuid);
}

// Starts single-item clipboard placement with a provisional scene element.
void Viewer2DPlacementSession::BeginClipboardSingle(
    ContinuousPlacementType type, std::string provisionalUuid) {
  Reset();
  m_mode = PlacementMode::ClipboardSingle;
  m_type = type;
  m_provisionalUuid = std::move(provisionalUuid);
}

// Starts clipboard batch placement without a single provisional element.
void Viewer2DPlacementSession::BeginClipboardBatch() {
  Reset();
  m_mode = PlacementMode::ClipboardBatch;
}

// Records the current provisional UUID as confirmed.
void Viewer2DPlacementSession::RecordConfirmedUuid() {
  if (IsActive() && !m_provisionalUuid.empty())
    m_placedUuids.push_back(m_provisionalUuid);
}

// Replaces the provisional UUID while preserving confirmed history.
void Viewer2DPlacementSession::ReplaceProvisionalUuid(
    std::string provisionalUuid) {
  m_provisionalUuid = std::move(provisionalUuid);
}

// Restores confirmed UUID history after the panel rearms pointer state.
void Viewer2DPlacementSession::RestorePlacedUuids(
    std::vector<std::string> placedUuids) {
  m_placedUuids = std::move(placedUuids);
}

// Removes and returns the most recently confirmed UUID for restoration.
std::string Viewer2DPlacementSession::TakeLastConfirmedUuid() {
  if (m_placedUuids.empty())
    return {};
  std::string uuid = std::move(m_placedUuids.back());
  m_placedUuids.pop_back();
  return uuid;
}

// Ends placement and clears all session-owned state.
void Viewer2DPlacementSession::Reset() {
  m_mode = PlacementMode::None;
  m_type = ContinuousPlacementType::None;
  m_provisionalUuid.clear();
  m_placedUuids.clear();
}

} // namespace viewer2d::interaction
