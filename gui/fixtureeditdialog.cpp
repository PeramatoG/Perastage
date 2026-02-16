/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "fixtureeditdialog.h"

#include "fixturepreviewpanel.h"
#include "fixturetablepanel.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "guiconfigservices.h"
#include "projectutils.h"
#include "viewer3dpanel.h"

#include <wx/clrpicker.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>

#include <algorithm>
#include <cmath>

namespace {
int ToSliderValue(double value, double limit) {
  if (limit <= 0.0)
    return 50;
  const double normalized = (value + limit) / (2.0 * limit);
  return static_cast<int>(std::round(std::clamp(normalized, 0.0, 1.0) * 100.0));
}

double FromSliderValue(int value, double limit) {
  if (limit <= 0.0)
    return 0.0;
  const double normalized = std::clamp(static_cast<double>(value) / 100.0, 0.0, 1.0);
  return (normalized * 2.0 - 1.0) * limit;
}
} // namespace

FixtureEditDialog::FixtureEditDialog(FixtureTablePanel *p, int r)
    : wxDialog(p, wxID_ANY, "Edit Fixture", wxDefaultPosition, wxSize(820, 650)),
      panel(p), row(r) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *hSizer = new wxBoxSizer(wxHORIZONTAL);
  wxFlexGridSizer *grid = new wxFlexGridSizer(2, 5, 5);
  grid->AddGrowableCol(1, 1);

  auto *table = panel->table;
  ctrls.resize(panel->columnLabels.size(), nullptr);

  if (static_cast<size_t>(row) < panel->rowUuids.size())
    editedFixtureUuid = panel->rowUuids[row];

  wxVariant initType;
  table->GetValue(initType, row, 2);
  originalType = initType.GetString();

  for (size_t i = 0; i < panel->columnLabels.size(); ++i) {
    grid->Add(new wxStaticText(this, wxID_ANY, panel->columnLabels[i]), 0,
              wxALIGN_CENTER_VERTICAL);
    wxVariant v;
    table->GetValue(v, row, i);
    if (i == 7) {
      wxString gdtfPath;
      if ((size_t)row < panel->gdtfPaths.size())
        gdtfPath = panel->gdtfPaths[row];
      modeChoice = new wxChoice(this, wxID_ANY);
      auto modes = GetGdtfModes(gdtfPath.ToStdString());
      for (const auto &m : modes)
        modeChoice->Append(wxString::FromUTF8(m));
      int sel = modeChoice->FindString(v.GetString());
      if (sel != wxNOT_FOUND)
        modeChoice->SetSelection(sel);
      ctrls[i] = modeChoice;
      grid->Add(modeChoice, 1, wxEXPAND);
    } else if (i == 8) {
      chCountCtrl = new wxTextCtrl(this, wxID_ANY, v.GetString(), wxDefaultPosition,
                                   wxDefaultSize, wxTE_READONLY);
      ctrls[i] = chCountCtrl;
      grid->Add(chCountCtrl, 1, wxEXPAND);
    } else if (i == 9) {
      wxBoxSizer *hs = new wxBoxSizer(wxHORIZONTAL);
      modelCtrl = new wxTextCtrl(this, wxID_ANY);
      if ((size_t)row < panel->gdtfPaths.size())
        modelCtrl->SetValue(panel->gdtfPaths[row]);
      hs->Add(modelCtrl, 1, wxEXPAND | wxRIGHT, 5);
      wxButton *browse = new wxButton(this, wxID_ANY, "...");
      hs->Add(browse, 0);
      browse->Bind(wxEVT_BUTTON, &FixtureEditDialog::OnBrowse, this);
      ctrls[i] = modelCtrl;
      grid->Add(hs, 1, wxEXPAND);
    } else if (i == 18) {
      wxString colorString;
      if (v.GetType() == "wxDataViewIconText") {
        wxDataViewIconText icon;
        icon << v;
        colorString = icon.GetText();
      } else {
        colorString = v.GetString();
      }
      wxColour initial(colorString);
      if (colorString.IsEmpty() || !initial.IsOk())
        initial = *wxWHITE;
      auto *picker = new wxColourPickerCtrl(this, wxID_ANY, initial);
      ctrls[i] = picker;
      grid->Add(picker, 1, wxEXPAND);
    } else {
      wxTextCtrl *tc = new wxTextCtrl(this, wxID_ANY, v.GetString());
      ctrls[i] = tc;
      grid->Add(tc, 1, wxEXPAND);
    }
  }

  hSizer->Add(grid, 1, wxALL | wxEXPAND, 10);

  wxBoxSizer *rightSizer = new wxBoxSizer(wxVERTICAL);
  preview = new FixturePreviewPanel(this);
  rightSizer->Add(preview, 1, wxEXPAND | wxBOTTOM, 5);

  wxStaticBoxSizer *motionBox = new wxStaticBoxSizer(wxVERTICAL, this, "Pan / Tilt / Dimmer");
  auto addMotionRow = [&](const wxString &label, wxSlider **sliderOut,
                          wxTextCtrl **ctrlOut, bool normalized) {
    wxBoxSizer *rowSizer = new wxBoxSizer(wxHORIZONTAL);
    rowSizer->Add(new wxStaticText(this, wxID_ANY, label), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    *sliderOut = new wxSlider(this, wxID_ANY, 50, 0, 100);
    *ctrlOut = new wxTextCtrl(this, wxID_ANY, normalized ? "1.00" : "0.0");
    rowSizer->Add(*sliderOut, 1, wxRIGHT, 6);
    rowSizer->Add(*ctrlOut, 0, wxALIGN_CENTER_VERTICAL);
    motionBox->Add(rowSizer, 0, wxEXPAND | wxBOTTOM, 4);
  };
  addMotionRow("Pan (deg)", &panSlider, &panCtrl, false);
  addMotionRow("Tilt (deg)", &tiltSlider, &tiltCtrl, false);
  addMotionRow("Dimmer [0..1]", &dimmerSlider, &dimmerCtrl, true);

  wxBoxSizer *limitsSizer = new wxBoxSizer(wxHORIZONTAL);
  limitsSizer->Add(new wxStaticText(this, wxID_ANY, "Pan limit"), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  panLimitCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
  panLimitCtrl->SetRange(10.0, 540.0);
  panLimitCtrl->SetIncrement(5.0);
  panLimitCtrl->SetDigits(1);
  panLimitCtrl->SetValue(panLimitDeg);
  limitsSizer->Add(panLimitCtrl, 0, wxRIGHT, 8);

  limitsSizer->Add(new wxStaticText(this, wxID_ANY, "Tilt limit"), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  tiltLimitCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
  tiltLimitCtrl->SetRange(10.0, 270.0);
  tiltLimitCtrl->SetIncrement(5.0);
  tiltLimitCtrl->SetDigits(1);
  tiltLimitCtrl->SetValue(tiltLimitDeg);
  limitsSizer->Add(tiltLimitCtrl, 0, wxRIGHT, 8);

  wxButton *resetMotion = new wxButton(this, wxID_ANY, "Reset Pan/Tilt/Dimmer");
  limitsSizer->Add(resetMotion, 0);
  motionBox->Add(limitsSizer, 0, wxEXPAND | wxTOP, 4);

  emitterDebugLabel = new wxStaticText(this, wxID_ANY, "Emitter debug: n/a");
  motionBox->Add(emitterDebugLabel, 0, wxTOP, 6);
  rightSizer->Add(motionBox, 0, wxEXPAND | wxBOTTOM, 6);

  channelList = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 120),
                               wxTE_MULTILINE | wxTE_READONLY);
  rightSizer->Add(channelList, 1, wxEXPAND);
  hSizer->Add(rightSizer, 1, wxTOP | wxBOTTOM | wxRIGHT | wxEXPAND, 10);

  topSizer->Add(hSizer, 1, wxEXPAND);

  wxStdDialogButtonSizer *btns = new wxStdDialogButtonSizer();
  btns->AddButton(new wxButton(this, wxID_APPLY));
  btns->AddButton(new wxButton(this, wxID_OK));
  btns->AddButton(new wxButton(this, wxID_CANCEL));
  btns->Realize();
  topSizer->Add(btns, 0, wxALL | wxEXPAND, 10);

  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnApply, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnOk, this, wxID_OK);
  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnCancel, this, wxID_CANCEL);
  if (modeChoice)
    modeChoice->Bind(wxEVT_CHOICE, &FixtureEditDialog::OnModeChanged, this);

  panCtrl->Bind(wxEVT_TEXT, &FixtureEditDialog::OnMotionControlChanged, this);
  tiltCtrl->Bind(wxEVT_TEXT, &FixtureEditDialog::OnMotionControlChanged, this);
  dimmerCtrl->Bind(wxEVT_TEXT, &FixtureEditDialog::OnMotionControlChanged, this);
  panSlider->Bind(wxEVT_SLIDER, &FixtureEditDialog::OnMotionSliderChanged, this);
  tiltSlider->Bind(wxEVT_SLIDER, &FixtureEditDialog::OnMotionSliderChanged, this);
  dimmerSlider->Bind(wxEVT_SLIDER, &FixtureEditDialog::OnMotionSliderChanged, this);
  panLimitCtrl->Bind(wxEVT_SPINCTRLDOUBLE, &FixtureEditDialog::OnMotionLimitChanged, this);
  tiltLimitCtrl->Bind(wxEVT_SPINCTRLDOUBLE, &FixtureEditDialog::OnMotionLimitChanged, this);
  resetMotion->Bind(wxEVT_BUTTON, &FixtureEditDialog::OnResetMotion, this);

  SetSizerAndFit(topSizer);
  SyncMotionControlsFromFixture();
  UpdateChannels();
}

void FixtureEditDialog::SyncMotionControlsFromFixture() {
  if (!panel || editedFixtureUuid.empty())
    return;
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.fixtures.find(editedFixtureUuid);
  if (it == scene.fixtures.end())
    return;

  const Fixture &f = it->second;
  if (std::abs(f.panDeg) > 0.001f)
    panCtrl->SetValue(wxString::Format("%.1f", f.panDeg));
  if (std::abs(f.tiltDeg) > 0.001f)
    tiltCtrl->SetValue(wxString::Format("%.1f", f.tiltDeg));
  dimmerCtrl->SetValue(wxString::Format("%.2f", f.dimmer));
  SyncMotionControlsToUi();
}

void FixtureEditDialog::SyncMotionControlsToUi() {
  if (!panCtrl || !tiltCtrl || !dimmerCtrl)
    return;
  syncingMotionUi = true;

  double pan = 0.0;
  panCtrl->GetValue().ToDouble(&pan);
  pan = std::clamp(pan, -panLimitDeg, panLimitDeg);
  panCtrl->SetValue(wxString::Format("%.1f", pan));
  panSlider->SetValue(ToSliderValue(pan, panLimitDeg));

  double tilt = 0.0;
  tiltCtrl->GetValue().ToDouble(&tilt);
  tilt = std::clamp(tilt, -tiltLimitDeg, tiltLimitDeg);
  tiltCtrl->SetValue(wxString::Format("%.1f", tilt));
  tiltSlider->SetValue(ToSliderValue(tilt, tiltLimitDeg));

  double dimmer = 1.0;
  dimmerCtrl->GetValue().ToDouble(&dimmer);
  dimmer = std::clamp(dimmer, 0.0, 1.0);
  dimmerCtrl->SetValue(wxString::Format("%.2f", dimmer));
  dimmerSlider->SetValue(static_cast<int>(std::round(dimmer * 100.0)));

  syncingMotionUi = false;
  RefreshPreviewMotion();
}

void FixtureEditDialog::RefreshPreviewMotion() {
  if (!preview)
    return;
  double pan = 0.0;
  double tilt = 0.0;
  double dimmer = 1.0;
  panCtrl->GetValue().ToDouble(&pan);
  tiltCtrl->GetValue().ToDouble(&tilt);
  dimmerCtrl->GetValue().ToDouble(&dimmer);
  preview->SetMotionPose(static_cast<float>(pan), static_cast<float>(tilt),
                         static_cast<float>(dimmer));
  if (emitterDebugLabel)
    emitterDebugLabel->SetLabel(wxString::FromUTF8(preview->GetEmitterDebugText()));
}

void FixtureEditDialog::OnMotionControlChanged(wxCommandEvent &) {
  if (syncingMotionUi)
    return;
  SyncMotionControlsToUi();
}

void FixtureEditDialog::OnMotionSliderChanged(wxCommandEvent &) {
  if (syncingMotionUi)
    return;

  syncingMotionUi = true;
  panCtrl->SetValue(wxString::Format("%.1f", FromSliderValue(panSlider->GetValue(), panLimitDeg)));
  tiltCtrl->SetValue(wxString::Format("%.1f", FromSliderValue(tiltSlider->GetValue(), tiltLimitDeg)));
  dimmerCtrl->SetValue(wxString::Format("%.2f", dimmerSlider->GetValue() / 100.0));
  syncingMotionUi = false;
  RefreshPreviewMotion();
}

void FixtureEditDialog::OnMotionLimitChanged(wxSpinDoubleEvent &) {
  panLimitDeg = panLimitCtrl ? panLimitCtrl->GetValue() : 270.0;
  tiltLimitDeg = tiltLimitCtrl ? tiltLimitCtrl->GetValue() : 135.0;
  SyncMotionControlsToUi();
}

void FixtureEditDialog::OnResetMotion(wxCommandEvent &) {
  panCtrl->SetValue("0.0");
  tiltCtrl->SetValue("0.0");
  dimmerCtrl->SetValue("1.00");
  SyncMotionControlsToUi();
}

void FixtureEditDialog::ApplyMotionToFixture() {
  if (!panel || editedFixtureUuid.empty())
    return;
  auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.fixtures.find(editedFixtureUuid);
  if (it == scene.fixtures.end())
    return;

  double pan = 0.0;
  double tilt = 0.0;
  double dimmer = 1.0;
  panCtrl->GetValue().ToDouble(&pan);
  tiltCtrl->GetValue().ToDouble(&tilt);
  dimmerCtrl->GetValue().ToDouble(&dimmer);

  it->second.panDeg = static_cast<float>(pan);
  it->second.tiltDeg = static_cast<float>(tilt);
  it->second.dimmer = static_cast<float>(std::clamp(dimmer, 0.0, 1.0));
}

void FixtureEditDialog::OnBrowse(wxCommandEvent &) {
  wxString fixDir = wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                    wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fdlg.ShowModal() != wxID_OK)
    return;
  wxString path = fdlg.GetPath();
  modelCtrl->SetValue(path);
  // update type/power/weight fields
  if (ctrls.size() > 2) {
    wxString typeName = wxString::FromUTF8(GetGdtfFixtureName(std::string(path.ToUTF8())));
    if (typeName.empty())
      typeName = fdlg.GetFilename();
    static_cast<wxTextCtrl *>(ctrls[2])->SetValue(typeName);
    float w = 0.f, p = 0.f;
    GetGdtfProperties(std::string(path.ToUTF8()), w, p);
    if (ctrls.size() > 16)
      static_cast<wxTextCtrl *>(ctrls[16])->SetValue(wxString::Format("%.1f", p));
    if (ctrls.size() > 17)
      static_cast<wxTextCtrl *>(ctrls[17])->SetValue(wxString::Format("%.2f", w));
  }
  if (modeChoice) {
    modeChoice->Clear();
    auto modes = GetGdtfModes(std::string(path.ToUTF8()));
    for (const auto &m : modes)
      modeChoice->Append(wxString::FromUTF8(m));
    if (!modeChoice->IsEmpty())
      modeChoice->SetSelection(0);
  }
  UpdateChannels();
}

void FixtureEditDialog::OnModeChanged(wxCommandEvent &) { UpdateChannels(); }

void FixtureEditDialog::UpdateChannels() {
  wxString gdtfPath = modelCtrl ? modelCtrl->GetValue() : wxString();
  wxString mode = modeChoice ? modeChoice->GetStringSelection() : wxString();
  if (preview)
    preview->LoadFixture(std::string(gdtfPath.ToUTF8()));
  RefreshPreviewMotion();

  if (gdtfPath.empty() || mode.empty()) {
    channelList->SetValue("");
    if (chCountCtrl)
      chCountCtrl->SetValue("");
    return;
  }
  auto channels = GetGdtfModeChannels(std::string(gdtfPath.ToUTF8()),
                                      std::string(mode.ToUTF8()));
  wxString msg;
  for (const auto &ch : channels) {
    wxString func = wxString::FromUTF8(ch.function);
    if (func.empty())
      func = "-";
    msg += wxString::Format("%d: %s\n", ch.channel, func);
  }
  channelList->SetValue(msg);
  int chCount = GetGdtfModeChannelCount(std::string(gdtfPath.ToUTF8()),
                                        std::string(mode.ToUTF8()));
  if (chCountCtrl)
    chCountCtrl->SetValue(chCount >= 0 ? wxString::Format("%d", chCount) : wxString());
}

void FixtureEditDialog::OnApply(wxCommandEvent &) { ApplyChanges(); }

void FixtureEditDialog::OnOk(wxCommandEvent &) {
  ApplyChanges();
  EndModal(wxID_OK);
}

void FixtureEditDialog::OnCancel(wxCommandEvent &) { EndModal(wxID_CANCEL); }

void FixtureEditDialog::ApplyChanges() {
  if (!panel)
    return;
  auto *table = panel->table;
  wxString gdtfPath = modelCtrl ? modelCtrl->GetValue() : wxString();

  std::vector<std::string> oldOrder = panel->rowUuids;
  std::vector<std::string> selectedUuids;
  if ((size_t)row < panel->rowUuids.size())
    selectedUuids.push_back(panel->rowUuids[row]);

  for (size_t i = 0; i < ctrls.size(); ++i) {
    if (i == 7 && modeChoice) {
      table->SetValue(wxVariant(modeChoice->GetStringSelection()), row, i);
    } else if (i == 8 && chCountCtrl) {
      table->SetValue(wxVariant(chCountCtrl->GetValue()), row, i);
    } else if (i == 9 && modelCtrl) {
      wxFileName fn(gdtfPath);
      table->SetValue(wxVariant(fn.GetFullName()), row, i);
      if ((size_t)row >= panel->gdtfPaths.size())
        panel->gdtfPaths.resize(row + 1);
      panel->gdtfPaths[row] = gdtfPath;
    } else if (i == 18) {
      auto *picker = wxDynamicCast(ctrls[i], wxColourPickerCtrl);
      if (picker) {
        wxString col = picker->GetColour().GetAsString(wxC2S_HTML_SYNTAX);
        table->SetValue(wxVariant(col), row, i);
      }
    } else {
      wxTextCtrl *tc = wxDynamicCast(ctrls[i], wxTextCtrl);
      if (tc) {
        wxString txt = tc->GetValue();
        if (i == 0 || i == 5 || i == 6) {
          long val = 0;
          txt.ToLong(&val);
          table->SetValue(wxVariant(val), row, i);
        } else {
          table->SetValue(wxVariant(txt), row, i);
        }
      }
    }
  }

  ApplyMotionToFixture();

  if (!gdtfPath.empty()) {
    std::string mode =
        modeChoice ? std::string(modeChoice->GetStringSelection().ToUTF8()) : std::string();
    GdtfDictionary::Update(std::string(originalType.ToUTF8()),
                           std::string(gdtfPath.ToUTF8()), mode);
    panel->ApplyModeForGdtf(gdtfPath, wxString::FromUTF8(mode));
  }

  panel->ResyncRows(oldOrder, selectedUuids);
  panel->UpdateSceneData();
  panel->HighlightDuplicateFixtureIds();
  applied = true;
  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }
}
