/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#pragma once

#include "position_selection.h"

#include <functional>
#include <wx/scrolwin.h>

class ConfigManager;
class wxStaticText;
class wxTextCtrl;
class wxSpinButton;

// Presentation only: selection resolution, units and mutation live in Core.
class PositionToolPanel final : public wxScrolledWindow {
public:
  PositionToolPanel(wxWindow *parent, ConfigManager &config,
                    std::function<void(const scene_grouping::ObjectSelection &, bool)> refresh);
  ~PositionToolPanel() override;
  void RefreshValues();
  void SelectGroup();

private:
  void QueueRefresh(bool selectionChanged);
  void BeginEdit(int axis);
  void Commit(int axis);
  void Cancel(int axis);
  void Step(int axis, int direction);

  ConfigManager &config_;
  std::function<void(const scene_grouping::ObjectSelection &, bool)> refresh_;
  wxStaticText *status_ = nullptr;
  wxStaticText *unit_ = nullptr;
  std::array<wxTextCtrl *, 3> fields_{};
  std::array<wxSpinButton *, 3> steppers_{};
  std::array<bool, 3> modified_{};
  std::array<std::size_t, 3> editRevision_{};
  std::array<position_selection::Snapshot, 3> editSelection_;
  std::array<Units::DistanceUnitSystem, 3> editUnits_{};
  Units::DistanceUnitSystem displayedUnits_ = Units::DistanceUnitSystem::Metric;
  std::size_t subscription_ = 0;
  bool refreshQueued_ = false;
  bool selectionChanged_ = false;
};
