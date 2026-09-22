/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#pragma once

#include "continuous_placement_type.h"

#include <string>
#include <vector>

namespace viewer2d::interaction {

enum class PlacementMode { None, Native, ClipboardSingle, ClipboardBatch };

// Owns GUI-independent continuous-placement lifecycle bookkeeping.
class Viewer2DPlacementSession {
public:
  // Starts native placement with a provisional scene element.
  void BeginNative(ContinuousPlacementType type, std::string provisionalUuid);
  // Starts single-item clipboard placement with a provisional scene element.
  void BeginClipboardSingle(ContinuousPlacementType type,
                            std::string provisionalUuid);
  // Starts clipboard batch placement without a single provisional element.
  void BeginClipboardBatch();
  // Records the current provisional UUID as confirmed.
  void RecordConfirmedUuid();
  // Replaces the provisional UUID while preserving confirmed history.
  void ReplaceProvisionalUuid(std::string provisionalUuid);
  // Restores confirmed UUID history after the panel rearms pointer state.
  void RestorePlacedUuids(std::vector<std::string> placedUuids);
  // Removes and returns the most recently confirmed UUID for restoration.
  std::string TakeLastConfirmedUuid();
  // Ends placement and clears all session-owned state.
  void Reset();

  // Reports whether any placement mode is active.
  bool IsActive() const { return m_mode != PlacementMode::None; }
  // Reports whether placement originated from clipboard data.
  bool IsClipboardPlacement() const {
    return m_mode == PlacementMode::ClipboardSingle || IsBatchPlacement();
  }
  // Reports whether a clipboard batch is active.
  bool IsBatchPlacement() const {
    return m_mode == PlacementMode::ClipboardBatch;
  }
  // Returns the explicit placement mode.
  PlacementMode Mode() const { return m_mode; }
  // Returns the provisional element type.
  ContinuousPlacementType Type() const { return m_type; }
  // Returns the current provisional element UUID.
  const std::string &ProvisionalUuid() const { return m_provisionalUuid; }
  // Returns the ordered confirmed UUID history.
  const std::vector<std::string> &PlacedUuids() const { return m_placedUuids; }

private:
  PlacementMode m_mode = PlacementMode::None;
  ContinuousPlacementType m_type = ContinuousPlacementType::None;
  std::string m_provisionalUuid;
  std::vector<std::string> m_placedUuids;
};

} // namespace viewer2d::interaction
