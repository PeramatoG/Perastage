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
#include "projectutils.h"
#include "gdtf_fixture_category.h"
#include "symbolcache.h"
#include "symbols/PerastageSvgSymbol.h"
#include "viewer3dpanel.h"
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/clrpicker.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <tinyxml2.h>
#include <unordered_map>
#include <set>
#include <cmath>
#include <memory>

namespace {

bool ParseFloatOrDefault(const wxString &text, float &out) {
  double parsed = 0.0;
  if (!text.ToDouble(&parsed))
    return false;
  out = static_cast<float>(parsed);
  return true;
}

bool LoadGdtfThumbnail(const std::string &gdtfPath, wxBitmap &outBitmap) {
  if (gdtfPath.empty())
    return false;

  wxFileInputStream input(wxString::FromUTF8(gdtfPath));
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::unordered_map<std::string, std::string> entries;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    wxString name = entry->GetName();
    std::string content;
    char buffer[4096];
    while (true) {
      zipInput.Read(buffer, sizeof(buffer));
      size_t count = zipInput.LastRead();
      if (count == 0)
        break;
      content.append(buffer, buffer + count);
    }
    entries.emplace(std::string(name.ToUTF8()), std::move(content));
  }

  auto descriptionIt = entries.find("description.xml");
  if (descriptionIt == entries.end())
    return false;

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionIt->second.c_str(), descriptionIt->second.size()) !=
      tinyxml2::XML_SUCCESS) {
    return false;
  }

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  else
    fixtureType = doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  const char *thumbnailAttr = fixtureType->Attribute("Thumbnail");
  std::string thumbnailBase = thumbnailAttr ? thumbnailAttr : "";

  std::vector<std::string> candidates;
  if (!thumbnailBase.empty()) {
    candidates.push_back(thumbnailBase);
    candidates.push_back(thumbnailBase + ".png");
    candidates.push_back(thumbnailBase + ".jpg");
    candidates.push_back(thumbnailBase + ".jpeg");
    candidates.push_back("thumbnails/" + thumbnailBase + ".png");
    candidates.push_back("thumbnails/" + thumbnailBase + ".jpg");
    candidates.push_back("thumbnails/" + thumbnailBase + ".jpeg");
  }
  candidates.push_back("thumbnail.png");
  candidates.push_back("thumbnail.jpg");
  candidates.push_back("thumbnail.jpeg");

  for (const auto &candidate : candidates) {
    auto it = entries.find(candidate);
    if (it == entries.end())
      continue;
    wxMemoryInputStream stream(it->second.data(), it->second.size());
    wxImage image;
    if (!image.LoadFile(stream, wxBITMAP_TYPE_ANY))
      continue;
    if (image.GetWidth() > 220 || image.GetHeight() > 150)
      image.Rescale(220, 150, wxIMAGE_QUALITY_HIGH);
    outBitmap = wxBitmap(image);
    return outBitmap.IsOk();
  }
  return false;
}

} // namespace

FixtureEditDialog::FixtureEditDialog(FixtureTablePanel *p, int r)
    : wxDialog(p, wxID_ANY, "Edit Fixture", wxDefaultPosition,
               wxSize(700, 600)),
      panel(p), row(r) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *hSizer = new wxBoxSizer(wxHORIZONTAL);
  wxStaticBoxSizer *fixtureSpecificSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Fixture-specific");
  wxStaticBoxSizer *gdtfGeneralSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "GDTF (shared for this fixture type)");
  wxFlexGridSizer *fixtureGrid = new wxFlexGridSizer(2, 5, 5);
  fixtureGrid->AddGrowableCol(1, 1);
  wxFlexGridSizer *gdtfGrid = new wxFlexGridSizer(2, 5, 5);
  gdtfGrid->AddGrowableCol(1, 1);

  auto *table = panel->table; // friend access
  ctrls.resize(panel->columnLabels.size(), nullptr);

  wxVariant initType;
  table->GetValue(initType, row, 2);
  originalType = initType.GetString();

  const std::set<size_t> gdtfColumns = {2, 7, 8, 9, 16, 17, 18};
  auto addLabeledControl = [&](size_t index, wxWindow *controlWindow,
                               wxSizer *nestedSizer, bool isGdtfField) {
    wxFlexGridSizer *targetGrid = isGdtfField ? gdtfGrid : fixtureGrid;
    targetGrid->Add(new wxStaticText(this, wxID_ANY, panel->columnLabels[index]), 0,
                    wxALIGN_CENTER_VERTICAL);
    if (nestedSizer)
      targetGrid->Add(nestedSizer, 1, wxEXPAND);
    else
      targetGrid->Add(controlWindow, 1, wxEXPAND);
  };

  for (size_t i = 0; i < panel->columnLabels.size(); ++i) {
    wxVariant v;
    table->GetValue(v, row, i);
    wxWindow *controlWindow = nullptr;
    wxSizer *nestedSizer = nullptr;
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
      controlWindow = modeChoice;
    } else if (i == 8) {
      chCountCtrl =
          new wxTextCtrl(this, wxID_ANY, v.GetString(), wxDefaultPosition,
                         wxDefaultSize, wxTE_READONLY);
      ctrls[i] = chCountCtrl;
      controlWindow = chCountCtrl;
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
      nestedSizer = hs;
    } else if (i == 18) {
      auto *category = new wxChoice(this, wxID_ANY);
      const wxArrayString values = {
          "Beam",         "Blinder", "Conventional", "FX",    "Hoist",
          "Hybrid",       "Laser",   "LED",          "Smoke", "Spot",
          "Strobe",       "Unknown", "Video",        "Wash"};
      for (const auto &entry : values)
        category->Append(entry);
      int selection = category->FindString(v.GetString());
      if (selection != wxNOT_FOUND)
        category->SetSelection(selection);
      ctrls[i] = category;
      controlWindow = category;
    } else if (i == 19) {
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
      controlWindow = picker;
    } else {
      wxTextCtrl *tc = new wxTextCtrl(this, wxID_ANY, v.GetString());
      ctrls[i] = tc;
      controlWindow = tc;
    }
    addLabeledControl(i, controlWindow, nestedSizer, gdtfColumns.count(i) > 0);
  }

  if (ctrls.size() > 16) {
    if (auto *powerCtrl = wxDynamicCast(ctrls[16], wxTextCtrl))
      ParseFloatOrDefault(powerCtrl->GetValue(), originalPowerW);
  }
  if (ctrls.size() > 17) {
    if (auto *weightCtrl = wxDynamicCast(ctrls[17], wxTextCtrl))
      ParseFloatOrDefault(weightCtrl->GetValue(), originalWeightKg);
  }

  fixtureSpecificSizer->Add(fixtureGrid, 1, wxEXPAND | wxALL, 6);
  gdtfGeneralSizer->Add(gdtfGrid, 1, wxEXPAND | wxALL, 6);
  gdtfGeneralSizer->Add(
      new wxStaticText(this, wxID_ANY,
                       "Changes in this column update the GDTF file and append a GDTF revision entry."),
      0, wxLEFT | wxRIGHT | wxBOTTOM, 6);

  channelList = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                               wxSize(-1, 150), wxTE_MULTILINE | wxTE_READONLY);
  gdtfGeneralSizer->Add(new wxStaticText(this, wxID_ANY, "Mode channels"), 0,
                        wxLEFT | wxRIGHT | wxTOP, 6);
  gdtfGeneralSizer->Add(channelList, 1,
                        wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 6);

  wxBoxSizer *formSizer = new wxBoxSizer(wxHORIZONTAL);
  formSizer->Add(fixtureSpecificSizer, 1, wxRIGHT | wxEXPAND, 8);
  formSizer->Add(gdtfGeneralSizer, 1, wxLEFT | wxEXPAND, 8);
  hSizer->Add(formSizer, 3, wxALL | wxEXPAND, 10);

  wxBoxSizer *rightSizer = new wxBoxSizer(wxVERTICAL);
  preview = new FixturePreviewPanel(this);
  rightSizer->Add(preview, 1, wxEXPAND | wxBOTTOM, 5);

  wxStaticBoxSizer *symbolSizer =
      new wxStaticBoxSizer(wxHORIZONTAL, this, "Symbols");
  const std::array<wxString, 3> symbolLabels = {"Top", "Front", "Side"};
  for (size_t i = 0; i < symbolPanels.size(); ++i) {
    wxBoxSizer *symbolColumn = new wxBoxSizer(wxVERTICAL);
    symbolColumn->Add(new wxStaticText(this, wxID_ANY, symbolLabels[i]), 0,
                      wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 3);
    symbolPanels[i] = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                                  wxSize(90, 70), wxBORDER_SIMPLE);
    symbolPanels[i]->SetBackgroundStyle(wxBG_STYLE_PAINT);
    symbolPanels[i]->Bind(wxEVT_PAINT, &FixtureEditDialog::OnSymbolPreviewPaint,
                          this);
    symbolColumn->Add(symbolPanels[i], 1, wxEXPAND);
    symbolSizer->Add(symbolColumn, 1, wxEXPAND | wxRIGHT, i < 2 ? 6 : 0);
  }
  rightSizer->Add(symbolSizer, 0, wxEXPAND | wxBOTTOM, 5);

  wxStaticBoxSizer *imageSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Fixture image");
  fixtureImagePreview =
      new wxStaticBitmap(this, wxID_ANY, wxBitmap(220, 150));
  imageSizer->Add(fixtureImagePreview, 0, wxALIGN_CENTER | wxALL, 4);
  rightSizer->Add(imageSizer, 0, wxEXPAND | wxBOTTOM, 5);

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

  SetSizerAndFit(topSizer);
  UpdateChannels();
  UpdateVisualizers();
}

void FixtureEditDialog::OnBrowse(wxCommandEvent &) {
  wxString fixDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                    wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fdlg.ShowModal() != wxID_OK)
    return;
  wxString path = fdlg.GetPath();
  modelCtrl->SetValue(path);
  if (preview)
    preview->LoadFixture(std::string(path.ToUTF8()));
  // update type/power/weight fields
  if (ctrls.size() > 2) {
    wxString typeName =
        wxString::FromUTF8(GetGdtfFixtureName(std::string(path.ToUTF8())));
    if (typeName.empty())
      typeName = fdlg.GetFilename();
    static_cast<wxTextCtrl *>(ctrls[2])->SetValue(typeName);
    float w = 0.f, p = 0.f;
    GetGdtfProperties(std::string(path.ToUTF8()), w, p);
    if (ctrls.size() > 16)
      static_cast<wxTextCtrl *>(ctrls[16])->SetValue(
          wxString::Format("%.1f", p));
    if (ctrls.size() > 17)
      static_cast<wxTextCtrl *>(ctrls[17])->SetValue(
          wxString::Format("%.2f", w));
  }
  // repopulate modes
  if (modeChoice) {
    modeChoice->Clear();
    auto modes = GetGdtfModes(std::string(path.ToUTF8()));
    for (const auto &m : modes)
      modeChoice->Append(wxString::FromUTF8(m));
    if (!modeChoice->IsEmpty())
      modeChoice->SetSelection(0);
  }
  UpdateChannels();
  UpdateVisualizers();
}

void FixtureEditDialog::OnModeChanged(wxCommandEvent &) { UpdateChannels(); }

void FixtureEditDialog::OnSymbolPreviewPaint(wxPaintEvent &evt) {
  wxPanel *panelWindow = wxDynamicCast(evt.GetEventObject(), wxPanel);
  if (!panelWindow)
    return;

  int panelIndex = -1;
  for (size_t i = 0; i < symbolPanels.size(); ++i) {
    if (symbolPanels[i] == panelWindow) {
      panelIndex = static_cast<int>(i);
      break;
    }
  }
  if (panelIndex < 0)
    return;

  wxAutoBufferedPaintDC dc(panelWindow);
  dc.SetBackground(*wxWHITE_BRUSH);
  dc.Clear();

  if (!symbolAvailability[panelIndex]) {
    dc.SetTextForeground(*wxLIGHT_GREY);
    dc.DrawLabel("N/A", panelWindow->GetClientRect(), wxALIGN_CENTER);
    return;
  }

  wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
  if (!gc)
    return;

  const PerastageSvgSymbolData &svg = symbolData[panelIndex];
  wxRect rect = panelWindow->GetClientRect();
  const double scale = std::min(
      (rect.GetWidth() - 8.0) / std::max(1.0, svg.viewBoxWidth),
      (rect.GetHeight() - 8.0) / std::max(1.0, svg.viewBoxHeight));
  const double originX = rect.GetX() + (rect.GetWidth() - svg.viewBoxWidth * scale) * 0.5;
  const double originY = rect.GetY() + (rect.GetHeight() - svg.viewBoxHeight * scale) * 0.5;

  gc->SetPen(wxPen(*wxBLACK, 1));
  gc->SetBrush(*wxWHITE_BRUSH);
  gc->DrawRectangle(rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
  gc->SetBrush(*wxBLACK_BRUSH);
  for (const auto &poly : svg.fills) {
    if (poly.points.size() < 3)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(originX + poly.points[0].x * scale,
                     originY + poly.points[0].y * scale);
    for (size_t i = 1; i < poly.points.size(); ++i)
      path.AddLineToPoint(originX + poly.points[i].x * scale,
                          originY + poly.points[i].y * scale);
    path.CloseSubpath();
    gc->FillPath(path);
    gc->SetBrush(*wxWHITE_BRUSH);
    for (const auto &hole : poly.holes) {
      if (hole.size() < 3)
        continue;
      wxGraphicsPath holePath = gc->CreatePath();
      holePath.MoveToPoint(originX + hole[0].x * scale,
                           originY + hole[0].y * scale);
      for (size_t i = 1; i < hole.size(); ++i)
        holePath.AddLineToPoint(originX + hole[i].x * scale,
                                originY + hole[i].y * scale);
      holePath.CloseSubpath();
      gc->FillPath(holePath);
    }
    gc->SetBrush(*wxBLACK_BRUSH);
  }
  gc->SetPen(wxPen(*wxBLACK, 1));
  for (const auto &line : svg.strokes) {
    if (line.points.size() < 2)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(originX + line.points[0].x * scale,
                     originY + line.points[0].y * scale);
    for (size_t i = 1; i < line.points.size(); ++i)
      path.AddLineToPoint(originX + line.points[i].x * scale,
                          originY + line.points[i].y * scale);
    gc->StrokePath(path);
  }
  delete gc;
}

void FixtureEditDialog::UpdateVisualizers() {
  const wxString gdtfPath = modelCtrl ? modelCtrl->GetValue() : wxString();
  const std::string path = std::string(gdtfPath.ToUTF8());
  const std::array<SymbolViewKind, 3> views = {SymbolViewKind::Bottom,
                                                SymbolViewKind::Front,
                                                SymbolViewKind::Left};
  for (size_t i = 0; i < views.size(); ++i) {
    PerastageSvgSymbolData loaded;
    symbolAvailability[i] =
        LoadPerastageSvgSymbolFromGdtf(path, views[i], loaded);
    if (symbolAvailability[i])
      symbolData[i] = std::move(loaded);
    if (symbolPanels[i])
      symbolPanels[i]->Refresh();
  }

  if (fixtureImagePreview) {
    wxBitmap image;
    if (LoadGdtfThumbnail(path, image)) {
      fixtureImagePreview->SetBitmap(image);
      fixtureImagePreview->SetToolTip("");
    } else {
      wxBitmap fallback(220, 150);
      wxMemoryDC dc(fallback);
      dc.SetBackground(*wxLIGHT_GREY_BRUSH);
      dc.Clear();
      dc.SetTextForeground(*wxBLACK);
      dc.DrawLabel("No image", wxRect(0, 0, 220, 150), wxALIGN_CENTER);
      dc.SelectObject(wxNullBitmap);
      fixtureImagePreview->SetBitmap(fallback);
      fixtureImagePreview->SetToolTip("No thumbnail image found in this GDTF.");
    }
    Layout();
  }
}

void FixtureEditDialog::UpdateChannels() {
  wxString gdtfPath = modelCtrl ? modelCtrl->GetValue() : wxString();
  wxString mode = modeChoice ? modeChoice->GetStringSelection() : wxString();
  if (preview)
    preview->LoadFixture(std::string(gdtfPath.ToUTF8()));
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
    while (true) {
      const int sectionStart = func.Find('[');
      if (sectionStart == wxNOT_FOUND)
        break;
      const wxString remainder = func.Mid(static_cast<size_t>(sectionStart));
      const int sectionEndRelative = remainder.Find(']');
      if (sectionEndRelative == wxNOT_FOUND) {
        func = func.Left(static_cast<size_t>(sectionStart));
        break;
      }
      const size_t sectionEnd =
          static_cast<size_t>(sectionStart + sectionEndRelative);
      func = func.Left(static_cast<size_t>(sectionStart)) +
             func.Mid(sectionEnd + 1);
    }
    func.Trim(true).Trim(false);
    if (func.empty())
      func = "-";
    msg += wxString::Format("%d: ", ch.channel) + func + "\n";
  }
  channelList->SetValue(msg);
  int chCount = GetGdtfModeChannelCount(std::string(gdtfPath.ToUTF8()),
                                        std::string(mode.ToUTF8()));
  if (chCountCtrl)
    chCountCtrl->SetValue(chCount >= 0 ? wxString::Format("%d", chCount)
                                       : wxString());
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
      auto *category = wxDynamicCast(ctrls[i], wxChoice);
      if (category)
        table->SetValue(wxVariant(category->GetStringSelection()), row, i);
    } else if (i == 19) {
      auto *picker = wxDynamicCast(ctrls[i], wxColourPickerCtrl);
      if (picker) {
        wxColour selectedColor = picker->GetColour();
        wxString colorText = selectedColor.GetAsString(wxC2S_HTML_SYNTAX);
        wxBitmap colorSwatch(16, 16);
        {
          wxMemoryDC dc(colorSwatch);
          dc.SetPen(*wxTRANSPARENT_PEN);
          dc.SetBrush(wxBrush(selectedColor));
          dc.DrawRectangle(0, 0, 16, 16);
          dc.SelectObject(wxNullBitmap);
        }
        wxVariant colorValue;
        colorValue << wxDataViewIconText(colorText, colorSwatch);
        table->SetValue(colorValue, row, i);
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
  if (!gdtfPath.empty()) {
    if (ctrls.size() > 17) {
      float newPowerW = originalPowerW;
      float newWeightKg = originalWeightKg;
      if (auto *powerCtrl = wxDynamicCast(ctrls[16], wxTextCtrl))
        ParseFloatOrDefault(powerCtrl->GetValue(), newPowerW);
      if (auto *weightCtrl = wxDynamicCast(ctrls[17], wxTextCtrl))
        ParseFloatOrDefault(weightCtrl->GetValue(), newWeightKg);
      const bool gdtfPhysicalChanged =
          std::fabs(newPowerW - originalPowerW) > 0.01f ||
          std::fabs(newWeightKg - originalWeightKg) > 0.01f;
      if (gdtfPhysicalChanged &&
          !SetGdtfProperties(std::string(gdtfPath.ToUTF8()), newWeightKg,
                             newPowerW, "Perastage")) {
        wxMessageBox(
            "Could not update GDTF physical properties (Weight/PowerConsumption).",
            "GDTF update", wxOK | wxICON_WARNING, this);
      } else if (gdtfPhysicalChanged) {
        originalPowerW = newPowerW;
        originalWeightKg = newWeightKg;
      }
    }

    std::string mode =
        modeChoice ? std::string(modeChoice->GetStringSelection().ToUTF8()) :
                     std::string();
    wxVariant categoryVar;
    table->GetValue(categoryVar, row, 18);
    const std::string category =
        GdtfFixtureCategory::NormalizeCategory(std::string(categoryVar.GetString().ToUTF8()));
    GdtfDictionary::Update(std::string(originalType.ToUTF8()),
                           std::string(gdtfPath.ToUTF8()), mode, category);
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
