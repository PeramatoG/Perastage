/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "positiontoolpanel.h"

#include "configmanager.h"
#include "project_mutation_host.h"
#include "selection_movement_settings.h"

#include <algorithm>
#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wrapsizer.h>

namespace {
Units::DistanceUnitSystem DistanceUnits(const ConfigManager &config) {
  return Units::ParseDistanceUnitSystem(config.GetValue("ui_distance_unit_system"));
}

position_selection::Snapshot Read(const ConfigManager &config) {
  return position_selection::Read(
      config.GetScene(), position_selection::SelectionFrom(config),
      selection_movement_settings::LoadInteractiveTransformPolicy(config));
}
} // namespace

PositionToolPanel::PositionToolPanel(
    wxWindow *parent, ConfigManager &config,
    std::function<void(const scene_grouping::ObjectSelection &, bool)> refresh)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxTAB_TRAVERSAL),
      config_(config), refresh_(std::move(refresh)) {
  SetScrollRate(0, FromDIP(10));
  auto *outer = new wxBoxSizer(wxVERTICAL);
  auto *heading = new wxBoxSizer(wxHORIZONTAL);
  status_ = new wxStaticText(this, wxID_ANY, _("No selection"),
                             wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  status_->SetMinSize(FromDIP(wxSize(1, -1)));
  heading->Add(status_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
  auto *group = new wxButton(this, wxID_ANY, _("Select group…"),
                              wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  group->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { SelectGroup(); });
  heading->Add(group, 0);
  outer->Add(heading, 0, wxEXPAND | wxALL, FromDIP(4));

  // Whole axis controls wrap together when a pane is narrow.
  auto *coordinates = new wxWrapSizer(wxHORIZONTAL);
  const wxString labels[] = {_("X:"), _("Y:"), _("Z:")};
  for (int axis = 0; axis < 3; ++axis) {
    auto *cell = new wxPanel(this);
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(cell, wxID_ANY, labels[axis]), 0,
             wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(3));
    auto *field = new wxTextCtrl(cell, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, FromDIP(wxSize(92, -1)),
                                 wxTE_PROCESS_ENTER);
    fields_[axis] = field;
    field->SetName(labels[axis]);
    row->Add(field, 1);
    cell->SetSizer(row);
    coordinates->Add(cell, 0, wxALL, FromDIP(3));
    field->Bind(wxEVT_SET_FOCUS, [this, axis](wxFocusEvent &event) {
      BeginEdit(axis);
      event.Skip();
    });
    field->Bind(wxEVT_TEXT, [this, axis](wxCommandEvent &) {
      if (!modified_[axis]) BeginEdit(axis);
      modified_[axis] = true;
    });
    field->Bind(wxEVT_TEXT_ENTER, [this, axis](wxCommandEvent &) {
      Commit(axis);
      if (fields_[axis]->IsEnabled()) fields_[axis]->SetFocus();
    });
    field->Bind(wxEVT_KILL_FOCUS, [this, axis](wxFocusEvent &event) {
      Commit(axis);
      // Invalid text is discarded on blur; it must not hold the unit display stale.
      modified_[axis] = false;
      QueueRefresh(false);
      event.Skip();
    });
    field->Bind(wxEVT_CHAR_HOOK, [this, axis](wxKeyEvent &event) {
      if (event.GetKeyCode() == WXK_ESCAPE) Cancel(axis);
      else event.Skip();
    });
  }
  unit_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
  coordinates->Add(unit_, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(3));
  outer->Add(coordinates, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(2));
  SetSizer(outer);
  Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    Layout();
    FitInside();
    event.Skip();
  });
  subscription_ = config_.SubscribeChanges([this](ConfigManager::Change change) {
    QueueRefresh(change == ConfigManager::Change::Selection);
  });
  RefreshValues();
}

PositionToolPanel::~PositionToolPanel() { config_.UnsubscribeChanges(subscription_); }

void PositionToolPanel::QueueRefresh(bool selectionChanged) {
  selectionChanged_ = selectionChanged_ || selectionChanged;
  if (refreshQueued_) return;
  refreshQueued_ = true;
  CallAfter([this] {
    refreshQueued_ = false;
    if (selectionChanged_) modified_.fill(false);
    const bool reset = selectionChanged_;
    selectionChanged_ = false;
    if (reset) {
      // A pending value belongs only to the selection on which editing began.
      for (int axis = 0; axis < 3; ++axis) {
        fields_[axis]->ChangeValue(wxEmptyString);
        BeginEdit(axis);
      }
    }
    RefreshValues();
  });
}

void PositionToolPanel::RefreshValues() {
  const auto snapshot = Read(config_);
  if (std::none_of(modified_.begin(), modified_.end(), [](bool v) { return v; }))
    displayedUnits_ = DistanceUnits(config_);
  const auto units = displayedUnits_;
  const bool empty = snapshot.targets.empty();
  if (empty) modified_.fill(false);
  status_->SetLabel(empty ? _("No selection")
                    : snapshot.targets.size() > 1 ? _("Multiple objects selected")
                    : snapshot.targets.front().type == MvrNodeType::GroupObject
                        ? _("Group selected") : _("Object selected"));
  unit_->SetLabel(wxString::FromUTF8(Units::DistanceUnitSuffix(units)));
  for (int axis = 0; axis < 3; ++axis) {
    auto *field = fields_[axis];
    field->Enable(!empty);
    if (!modified_[axis]) {
      const wxString value = snapshot.commonMm[axis]
          ? wxString::FromUTF8(position_selection::FormatCoordinate(
                *snapshot.commonMm[axis], units)) : wxString{};
      if (field->GetValue() != value) field->ChangeValue(value);
      field->SetToolTip(empty ? _("Select an object to edit its world position.")
                             : _("Absolute world coordinate. An empty field means mixed values."));
    }
  }
  Layout();
  FitInside();
}

void PositionToolPanel::BeginEdit(int axis) {
  editRevision_[axis] = config_.GetSelectionRevision();
  editSelection_[axis] = Read(config_);
  editUnits_[axis] = displayedUnits_;
}

void PositionToolPanel::Cancel(int axis) {
  modified_[axis] = false;
  RefreshValues();
  fields_[axis]->SelectAll();
}

void PositionToolPanel::Commit(int axis) {
  if (!modified_[axis]) return;
  if (editRevision_[axis] != config_.GetSelectionRevision() ||
      !position_selection::SameTargets(editSelection_[axis], Read(config_))) {
    Cancel(axis);
    return;
  }
  const auto text = fields_[axis]->GetValue().ToStdString();
  if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
    Cancel(axis);
    return;
  }
  if (!position_selection::ParseCoordinate(text, editUnits_[axis])) {
    fields_[axis]->SetToolTip(_("Enter a valid distance."));
    status_->SetLabel(_("Enter a valid distance."));
    return;
  }
  auto selection = position_selection::SelectionFrom(config_);
  GuiProjectMutationHost host(config_);
  perastage::command::ExecutionContext context{config_.GetScene(), selection, host};
  const auto result = position_selection::SetAxis(
      context, selection_movement_settings::LoadInteractiveTransformPolicy(config_),
      axis, text, editUnits_[axis]);
  if (!result.Success()) {
    status_->SetLabel(_("Position could not be updated."));
    return;
  }
  // Clear before publishing: focus changes and refreshes cannot commit twice.
  modified_[axis] = false;
  if (result.mutation.sceneChanged) refresh_(selection, true);
  RefreshValues();
}

void PositionToolPanel::SelectGroup() {
  const auto &scene = static_cast<const ConfigManager &>(config_).GetScene();
  std::vector<std::pair<std::string, std::string>> groups;
  for (const auto &[uuid, group] : scene.groupObjects)
    groups.emplace_back(group.name.empty() ? uuid : group.name, uuid);
  std::sort(groups.begin(), groups.end());
  wxArrayString labels;
  for (const auto &[name, uuid] : groups)
    labels.Add(wxString::FromUTF8(name + " (" + uuid + ")"));
  wxSingleChoiceDialog dialog(this, _("Select a group to position."),
                               _("Position Tool"), labels);
  if (dialog.ShowModal() != wxID_OK || dialog.GetSelection() < 0) return;
  scene_grouping::ObjectSelection selection;
  selection.groupObjects = {groups[dialog.GetSelection()].second};
  refresh_(selection, false);
}
