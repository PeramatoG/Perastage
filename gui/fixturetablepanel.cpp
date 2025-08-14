#include "fixturetablepanel.h"
#include "addressdialog.h"
#include "configmanager.h"
#include "fixtureeditdialog.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "layerpanel.h"
#include "matrixutils.h"
#include "patchmanager.h"
#include "projectutils.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <wx/choicdlg.h>
#include <wx/colordlg.h>
#include <wx/dcmemory.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/tokenzr.h>

namespace fs = std::filesystem;

FixtureTablePanel::FixtureTablePanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
  table->AssociateModel(&store);

  table->SetAlternateRowColour(wxColour(40, 40, 40));

  table->Bind(wxEVT_LEFT_DOWN, &FixtureTablePanel::OnLeftDown, this);
  table->Bind(wxEVT_LEFT_UP, &FixtureTablePanel::OnLeftUp, this);
  table->Bind(wxEVT_MOTION, &FixtureTablePanel::OnMouseMove, this);
  table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
              &FixtureTablePanel::OnSelectionChanged, this);

  table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
              &FixtureTablePanel::OnContextMenu, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
              &FixtureTablePanel::OnItemActivated, this);
  table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED, &FixtureTablePanel::OnColumnSorted,
              this);

  InitializeTable();
  ReloadData();

  sizer->Add(table, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
}

FixtureTablePanel::~FixtureTablePanel() {
  if (table)
    table->AssociateModel(nullptr);
}

void FixtureTablePanel::InitializeTable() {
  columnLabels = {"Fixture ID", "Name",        "Type",    "Layer",
                  "Hang Pos",   "Universe",    "Channel", "Mode",
                  "Ch Count",   "Model file",  "Pos X",   "Pos Y",
                  "Pos Z",      "Rot X",       "Rot Y",   "Rot Z",
                  "Power (W)",  "Weight (kg)", "Color"};

  std::vector<int> widths = {90, 150, 180, 100, 120, 80, 80,  120, 80, 180,
                             80, 80,  80,  80,  80,  80, 100, 100, 80};
  int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;

  // Column 0: Fixture ID (numeric for proper sorting)
  auto *idRenderer =
      new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  auto *idColumn = new wxDataViewColumn(columnLabels[0], idRenderer, 0,
                                        widths[0], wxALIGN_LEFT, flags);
  table->AppendColumn(idColumn);

  // Column 1: Name (string)
  table->AppendTextColumn(columnLabels[1], wxDATAVIEW_CELL_INERT, widths[1],
                          wxALIGN_LEFT, flags);

  // Column 2: Type (string)
  table->AppendTextColumn(columnLabels[2], wxDATAVIEW_CELL_INERT, widths[2],
                          wxALIGN_LEFT, flags);

  // Column 3: Layer (string)
  table->AppendTextColumn(columnLabels[3], wxDATAVIEW_CELL_INERT, widths[3],
                          wxALIGN_LEFT, flags);

  // Column 4: Hang Pos (string)
  table->AppendTextColumn(columnLabels[4], wxDATAVIEW_CELL_INERT, widths[4],
                          wxALIGN_LEFT, flags);

  // Column 5: Universe (numeric)
  auto *uniRenderer =
      new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  auto *uniColumn = new wxDataViewColumn(columnLabels[5], uniRenderer, 5,
                                         widths[5], wxALIGN_LEFT, flags);
  table->AppendColumn(uniColumn);

  // Column 6: Channel (numeric)
  auto *chRenderer =
      new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  auto *chColumn = new wxDataViewColumn(columnLabels[6], chRenderer, 6,
                                        widths[6], wxALIGN_LEFT, flags);
  table->AppendColumn(chColumn);

  // Columns 7 to second last as regular text
  for (size_t i = 7; i < columnLabels.size() - 1; ++i)
    table->AppendTextColumn(columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i],
                            wxALIGN_LEFT, flags);

  // Last column (Color) uses icon+text to show a colored square
  auto *colorRenderer = new wxDataViewIconTextRenderer();
  auto *colorColumn = new wxDataViewColumn(columnLabels.back(), colorRenderer,
                                           columnLabels.size() - 1,
                                           widths.back(), wxALIGN_LEFT, flags);
  table->AppendColumn(colorColumn);
}

void FixtureTablePanel::ReloadData() {
  table->DeleteAllItems();
  gdtfPaths.clear();
  rowUuids.clear();

  const auto &fixtures = ConfigManager::Get().GetScene().fixtures;

  struct Address {
    long universe;
    long channel;
  };
  auto parseAddress = [](const std::string &addr) -> Address {
    Address res{0, 0};
    if (!addr.empty()) {
      size_t dot = addr.find('.');
      if (dot != std::string::npos) {
        try {
          res.universe = std::stol(addr.substr(0, dot));
        } catch (...) {
        }
        try {
          res.channel = std::stol(addr.substr(dot + 1));
        } catch (...) {
        }
      }
    }
    return res;
  };

  std::vector<std::pair<std::string, const Fixture *>> sorted;
  sorted.reserve(fixtures.size());
  for (const auto &[uuid, fixture] : fixtures)
    sorted.emplace_back(uuid, &fixture);

  std::sort(sorted.begin(), sorted.end(), [&](const auto &A, const auto &B) {
    const Fixture *a = A.second;
    const Fixture *b = B.second;
    if (a->fixtureId != b->fixtureId)
      return a->fixtureId < b->fixtureId;
    if (a->gdtfSpec != b->gdtfSpec)
      return StringUtils::NaturalLess(a->gdtfSpec, b->gdtfSpec);
    auto addrA = parseAddress(a->address);
    auto addrB = parseAddress(b->address);
    if (addrA.universe != addrB.universe)
      return addrA.universe < addrB.universe;
    return addrA.channel < addrB.channel;
  });

  for (const auto &pair : sorted) {
    const std::string &uuid = pair.first;
    const Fixture *fixture = pair.second;
    wxVector<wxVariant> row;

    wxString name = wxString::FromUTF8(fixture->instanceName);
    long fixtureID = static_cast<long>(fixture->fixtureId);
    wxString layer = fixture->layer == DEFAULT_LAYER_NAME
                         ? wxString()
                         : wxString::FromUTF8(fixture->layer);
    long universe = 0;
    long channel = 0;
    if (!fixture->address.empty()) {
      wxStringTokenizer tk(wxString::FromUTF8(fixture->address), ".");
      if (tk.HasMoreTokens())
        tk.GetNextToken().ToLong(&universe);
      if (tk.HasMoreTokens())
        tk.GetNextToken().ToLong(&channel);
    }
    std::string fullPath;
    if (!fixture->gdtfSpec.empty()) {
      const std::string &base = ConfigManager::Get().GetScene().basePath;
      fs::path p = base.empty() ? fs::path(fixture->gdtfSpec)
                                : fs::path(base) / fixture->gdtfSpec;
      fullPath = p.string();
    }
    wxString gdtfFull = wxString::FromUTF8(fullPath);
    gdtfPaths.push_back(gdtfFull);
    wxString gdtf = wxFileName(gdtfFull).GetFullName();
    wxString type = wxString::FromUTF8(fixture->typeName);
    if (type.empty())
      type = gdtf;
    wxString mode = wxString::FromUTF8(fixture->gdtfMode);

    int chCount = GetGdtfModeChannelCount(std::string(gdtfFull.ToUTF8()),
                                          fixture->gdtfMode);
    wxString chCountStr =
        chCount >= 0 ? wxString::Format("%d", chCount) : wxString();

    auto posArr = fixture->GetPosition();
    wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
    wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
    wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);
    wxString posName = wxString::FromUTF8(fixture->positionName);

    auto euler = MatrixUtils::MatrixToEuler(fixture->transform);
    wxString rotX = wxString::Format("%.1f\u00B0", euler[0]);
    wxString rotY = wxString::Format("%.1f\u00B0", euler[1]);
    wxString rotZ = wxString::Format("%.1f\u00B0", euler[2]);

    row.push_back(fixtureID);
    row.push_back(name);
    row.push_back(type);
    row.push_back(layer);
    row.push_back(posName);
    row.push_back(universe);
    row.push_back(channel);
    row.push_back(mode);
    row.push_back(chCountStr);
    row.push_back(gdtf);
    row.push_back(posX);
    row.push_back(posY);
    row.push_back(posZ);
    row.push_back(rotX);
    row.push_back(rotY);
    row.push_back(rotZ);
    wxString power = wxString::Format("%.1f", fixture->powerConsumptionW);
    wxString weight = wxString::Format("%.2f", fixture->weightKg);
    row.push_back(power);
    row.push_back(weight);
    wxString color = wxString::FromUTF8(fixture->color);
    if (!color.IsEmpty()) {
      wxColour col(color);
      wxBitmap bmp(16, 16);
      {
        wxMemoryDC dc(bmp);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(col));
        dc.DrawRectangle(0, 0, 16, 16);
        dc.SelectObject(wxNullBitmap);
      }
      wxDataViewIconText iconText(color, bmp);
      wxVariant var;
      var << iconText;
      row.push_back(var);
    } else {
      wxVariant var;
      var << wxDataViewIconText();
      row.push_back(var);
    }

    store.AppendItem(row, rowUuids.size());
    rowUuids.push_back(uuid);
  }

  if (Viewer3DPanel::Instance())
    Viewer3DPanel::Instance()->SetSelectedFixtures({});

  HighlightDuplicateFixtureIds();

  // Let wxDataViewListCtrl manage column headers and sorting
  if (LayerPanel::Instance())
    LayerPanel::Instance()->ReloadLayers();
  if (SummaryPanel::Instance() && IsActivePage())
    SummaryPanel::Instance()->ShowFixtureSummary();
}

void FixtureTablePanel::OnContextMenu(wxDataViewEvent &event) {
  wxDataViewItem item = event.GetItem();
  int col = event.GetColumn();
  if (!item.IsOk() || col < 0)
    return;

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.empty())
    selections.push_back(item);

  std::vector<std::string> selectedUuids;
  for (const auto &itSel : selections) {
    int r = table->ItemToRow(itSel);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
      selectedUuids.push_back(rowUuids[r]);
  }
  std::vector<std::string> oldOrder = rowUuids;

  // Model file column opens file dialog
  if (col == 9) {
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() == wxID_OK) {
      wxString path = fdlg.GetPath();
      float w = 0.0f, p = 0.0f;
      std::string pathUtf8(path.ToUTF8());
      GetGdtfProperties(pathUtf8, w, p);
      wxString typeName = wxString::FromUTF8(GetGdtfFixtureName(pathUtf8));
      if (typeName.empty())
        typeName = fdlg.GetFilename();
      wxString fileName = fdlg.GetFilename();

      std::vector<std::string> prevTypes;

      for (const auto &itSel : selections) {
        int r = table->ItemToRow(itSel);
        if (r == wxNOT_FOUND)
          continue;

        wxVariant prevType;
        table->GetValue(prevType, r, 2);
        prevTypes.push_back(std::string(prevType.GetString().ToUTF8()));

        if ((size_t)r >= gdtfPaths.size())
          gdtfPaths.resize(table->GetItemCount());

        gdtfPaths[r] = path;
        table->SetValue(wxVariant(fileName), r, 9);
        table->SetValue(wxVariant(typeName), r, 2);

        wxString pstr = wxString::Format("%.1f", p);
        wxString wstr = wxString::Format("%.2f", w);
        table->SetValue(wxVariant(pstr), r, 16);
        table->SetValue(wxVariant(wstr), r, 17);
      }

      PropagateTypeValues(selections, 16);
      PropagateTypeValues(selections, 17);

      wxString dictMode;
      if (!prevTypes.empty()) {
        if (auto entry = GdtfDictionary::Get(prevTypes[0]))
          dictMode = wxString::FromUTF8(entry->mode);
      }
      ApplyModeForGdtf(path, dictMode);

      // Update dictionary with final mode for each previous type
      for (size_t idx = 0; idx < selections.size(); ++idx) {
        int r = table->ItemToRow(selections[idx]);
        if (r == wxNOT_FOUND)
          continue;
        wxVariant modeVar;
        table->GetValue(modeVar, r, 7);
        GdtfDictionary::Update(prevTypes[idx], pathUtf8,
                               std::string(modeVar.GetString().ToUTF8()));
      }
    }
    ResyncRows(oldOrder, selectedUuids);
    UpdateSceneData();
    HighlightDuplicateFixtureIds();
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    }
    return;
  }

  // Mode column shows available modes of the selected GDTF
  if (col == 7) {
    int r = table->ItemToRow(item);
    if (r == wxNOT_FOUND)
      return;

    wxString gdtfPath;
    if ((size_t)r < gdtfPaths.size())
      gdtfPath = gdtfPaths[r];

    std::vector<std::string> modes =
        GetGdtfModes(std::string(gdtfPath.ToUTF8()));
    if (modes.size() <= 1)
      return;

    wxArrayString choices;
    for (const auto &m : modes)
      choices.push_back(wxString::FromUTF8(m));

    wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
    if (dlg.ShowModal() != wxID_OK)
      return;

    wxString sel = dlg.GetStringSelection();

    wxDataViewItemArray modeSelections;
    table->GetSelections(modeSelections);
    if (modeSelections.empty())
      modeSelections.push_back(item);

    for (const auto &itSel : modeSelections) {
      int sr = table->ItemToRow(itSel);
      if (sr == wxNOT_FOUND)
        continue;
      if ((size_t)sr >= gdtfPaths.size())
        continue;
      if (gdtfPaths[sr] != gdtfPath)
        continue;

      table->SetValue(wxVariant(sel), sr, col);

      int chCount = GetGdtfModeChannelCount(std::string(gdtfPath.ToUTF8()),
                                            std::string(sel.ToUTF8()));
      wxString chStr =
          chCount >= 0 ? wxString::Format("%d", chCount) : wxString();
      table->SetValue(wxVariant(chStr), sr, 8);

      wxVariant typeVar;
      table->GetValue(typeVar, sr, 2);
      GdtfDictionary::Update(std::string(typeVar.GetString().ToUTF8()),
                             std::string(gdtfPath.ToUTF8()),
                             std::string(sel.ToUTF8()));
    }
    ApplyModeForGdtf(gdtfPath, sel);

    ResyncRows(oldOrder, selectedUuids);
    UpdateSceneData();
    HighlightDuplicateFixtureIds();
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    }
    return;
  }

  // Layer column uses existing layer list
  if (col == 3) {
    auto layers = ConfigManager::Get().GetLayerNames();
    wxArrayString choices;
    for (const auto &n : layers)
      choices.push_back(wxString::FromUTF8(n));
    wxSingleChoiceDialog dlg(this, "Select layer", "Layer", choices);
    if (dlg.ShowModal() != wxID_OK)
      return;
    wxString sel = dlg.GetStringSelection();
    wxString val =
        sel == wxString::FromUTF8(DEFAULT_LAYER_NAME) ? wxString() : sel;
    for (const auto &itSel : selections) {
      int r = table->ItemToRow(itSel);
      if (r != wxNOT_FOUND)
        table->SetValue(wxVariant(val), r, col);
    }
    PropagateTypeValues(selections, col);
    ResyncRows(oldOrder, selectedUuids);
    UpdateSceneData();
    HighlightDuplicateFixtureIds();
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    }
    return;
  }

  // Channel column edits both universe and channel
  if (col == 6) {
    int r = table->ItemToRow(item);
    if (r == wxNOT_FOUND)
      return;

    wxVariant vUni, vCh;
    table->GetValue(vUni, r, 5);
    table->GetValue(vCh, r, 6);
    AddressDialog dlg(this, vUni.GetLong(), vCh.GetLong());
    if (dlg.ShowModal() != wxID_OK)
      return;

    int newUni = dlg.GetUniverse();
    int newCh = dlg.GetChannel();
    if (newCh < 1)
      newCh = 1;
    if (newCh > 512) {
      newUni += (newCh - 1) / 512;
      newCh = 1;
    }

    wxDataViewItemArray adrSelections;
    table->GetSelections(adrSelections);
    if (adrSelections.empty())
      adrSelections.push_back(item);

    std::vector<int> selectedRows;
    for (const auto &itSel : adrSelections) {
      int row = table->ItemToRow(itSel);
      if (row != wxNOT_FOUND)
        selectedRows.push_back(row);
    }

    std::vector<int> orderedRows;
    for (int idx : selectionOrder)
      if (std::find(selectedRows.begin(), selectedRows.end(), idx) !=
          selectedRows.end())
        orderedRows.push_back(idx);
    for (int idx : selectedRows)
      if (std::find(orderedRows.begin(), orderedRows.end(), idx) ==
          orderedRows.end())
        orderedRows.push_back(idx);

    std::vector<int> counts;
    counts.reserve(orderedRows.size());
    for (int row : orderedRows) {
      wxVariant vCount;
      table->GetValue(vCount, row, 8);
      long c = 1;
      if (!vCount.GetString().ToLong(&c))
        c = 1;
      if (c < 1)
        c = 1;
      counts.push_back(static_cast<int>(c));
    }

    auto addrs = PatchManager::SequentialPatch(counts, newUni, newCh);
    for (size_t i = 0; i < orderedRows.size() && i < addrs.size(); ++i) {
      table->SetValue(wxVariant(addrs[i].universe), orderedRows[i], 5);
      table->SetValue(wxVariant(addrs[i].channel), orderedRows[i], 6);
    }

    ResyncRows(oldOrder, selectedUuids);
    UpdateSceneData();
    HighlightDuplicateFixtureIds();
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    }
    return;
  }

  int baseRow = table->ItemToRow(item);
  if (baseRow == wxNOT_FOUND)
    return;

  wxVariant current;
  table->GetValue(current, baseRow, col);

  if (col == 18) {
    wxColourData data;
    data.SetChooseFull(true);
    wxColour initial(current.GetString());
    data.SetColour(initial);
    wxColourDialog cdlg(this, &data);
    if (cdlg.ShowModal() != wxID_OK)
      return;
    wxColour selCol = cdlg.GetColourData().GetColour();
    wxString value = selCol.GetAsString(wxC2S_HTML_SYNTAX);
    wxBitmap bmp(16, 16);
    {
      wxMemoryDC dc(bmp);
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(selCol));
      dc.DrawRectangle(0, 0, 16, 16);
      dc.SelectObject(wxNullBitmap);
    }
    wxDataViewIconText iconText(value, bmp);
    wxVariant var;
    var << iconText;
    for (const auto &it : selections) {
      int r = table->ItemToRow(it);
      if (r != wxNOT_FOUND)
        table->SetValue(var, r, col);
    }
    PropagateTypeValues(selections, col);
    ResyncRows(oldOrder, selectedUuids);
    UpdateSceneData();
    HighlightDuplicateFixtureIds();
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    }
    return;
  }

  wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col],
                        current.GetString());
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString raw = dlg.GetValue();
  bool trailingSpace = raw.EndsWith(" ");
  wxString value = raw.Trim(true).Trim(false);

  bool intCol = (col == 0 || col == 5 || col == 6);
  bool numericCol =
      intCol || (col >= 10 && col <= 12) || (col >= 13 && col <= 17);
  bool relative = false;
  double delta = 0.0;
  if (!intCol && col >= 10 && col <= 15 && !value.empty() &&
      (value[0] == '+' || value[0] == '-')) {
    wxString numStr = value.Mid(1);
    if (numStr.ToDouble(&delta)) {
      if (value[0] == '-')
        delta = -delta;
      relative = true;
    }
  }

  wxArrayString parts = wxSplit(value, ' ');

  if (numericCol) {
    if (relative) {
      for (const auto &it : selections) {
        int r = table->ItemToRow(it);
        if (r == wxNOT_FOUND)
          continue;
        wxVariant cv;
        table->GetValue(cv, r, col);
        wxString cur = cv.GetString();
        if (col >= 13 && col <= 15)
          cur.Replace("\u00B0", "");
        double curVal = 0.0;
        cur.ToDouble(&curVal);
        double newVal = curVal + delta;
        wxString out;
        if (col >= 13 && col <= 15)
          out = wxString::Format("%.1f\u00B0", newVal);
        else
          out = wxString::Format("%.3f", newVal);
        table->SetValue(wxVariant(out), r, col);
      }
    } else {
      if (parts.size() == 0 || parts.size() > 2) {
        wxMessageBox("Invalid numeric value", "Error", wxOK | wxICON_ERROR);
        return;
      }

      if (intCol) {
        long v1, v2 = 0;
        if (!parts[0].ToLong(&v1)) {
          wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
          return;
        }
        if (col == 6 && (v1 < 1 || v1 > 512)) {
          wxMessageBox("Channel out of range (1-512)", "Error",
                       wxOK | wxICON_ERROR);
          return;
        }
        bool interp = false;
        bool sequential = false;
        if (parts.size() == 2) {
          if (!parts[1].ToLong(&v2)) {
            wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
            return;
          }
          if (col == 6 && (v2 < 1 || v2 > 512)) {
            wxMessageBox("Channel out of range (1-512)", "Error",
                         wxOK | wxICON_ERROR);
            return;
          }
          interp = selections.size() > 1;
        } else if (trailingSpace) {
          sequential = selections.size() > 1;
        }

        std::vector<int> selectedRows;
        selectedRows.reserve(selections.size());
        for (const auto &it : selections) {
          int r = table->ItemToRow(it);
          if (r != wxNOT_FOUND)
            selectedRows.push_back(r);
        }

        std::vector<int> orderedRows;
        for (int idx : selectionOrder)
          if (std::find(selectedRows.begin(), selectedRows.end(), idx) !=
              selectedRows.end())
            orderedRows.push_back(idx);
        for (int idx : selectedRows)
          if (std::find(orderedRows.begin(), orderedRows.end(), idx) ==
              orderedRows.end())
            orderedRows.push_back(idx);

        for (size_t i = 0; i < orderedRows.size(); ++i) {
          long val = v1;
          if (interp)
            val = static_cast<long>(v1 + (double)(v2 - v1) * i /
                                             (orderedRows.size() - 1));
          else if (sequential)
            val = v1 + static_cast<long>(i);

          table->SetValue(wxVariant(val), orderedRows[i], col);
        }
      } else // floating point stored as string
      {
        double v1, v2 = 0.0;
        if (!parts[0].ToDouble(&v1)) {
          wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
          return;
        }
        bool interp = false;
        if (parts.size() == 2) {
          if (!parts[1].ToDouble(&v2)) {
            wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
            return;
          }
          interp = selections.size() > 1;
        }

        for (size_t i = 0; i < selections.size(); ++i) {
          double val = v1;
          if (interp)
            val = v1 + (v2 - v1) * i / (selections.size() - 1);

          wxString out;
          if (col >= 13 && col <= 15)
            out = wxString::Format("%.1f\u00B0", val);
          else
            out = wxString::Format("%.3f", val);

          int r = table->ItemToRow(selections[i]);
          if (r != wxNOT_FOUND)
            table->SetValue(wxVariant(out), r, col);
        }
      }
    }
  } else {
    if (col == 1 && selections.size() > 1) {
      int spacePos = value.find_last_of(' ');
      long baseNum = 0;
      if (spacePos != wxNOT_FOUND && value.Mid(spacePos + 1).ToLong(&baseNum)) {
        wxString prefix = value.Left(spacePos);

        std::vector<int> selectedRows;
        selectedRows.reserve(selections.size());
        for (const auto &it : selections) {
          int r = table->ItemToRow(it);
          if (r != wxNOT_FOUND)
            selectedRows.push_back(r);
        }

        std::vector<int> orderedRows;
        for (int idx : selectionOrder)
          if (std::find(selectedRows.begin(), selectedRows.end(), idx) !=
              selectedRows.end())
            orderedRows.push_back(idx);
        for (int idx : selectedRows)
          if (std::find(orderedRows.begin(), orderedRows.end(), idx) ==
              orderedRows.end())
            orderedRows.push_back(idx);

        for (size_t i = 0; i < orderedRows.size(); ++i) {
          wxString newName =
              wxString::Format("%s %ld", prefix, baseNum + (long)i);
          table->SetValue(wxVariant(newName), orderedRows[i], col);
        }
      } else {
        for (const auto &it : selections) {
          int r = table->ItemToRow(it);
          if (r != wxNOT_FOUND)
            table->SetValue(wxVariant(value), r, col);
        }
      }
    } else {
      for (const auto &it : selections) {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND)
          table->SetValue(wxVariant(value), r, col);
      }
    }
  }
  PropagateTypeValues(selections, col);
  ResyncRows(oldOrder, selectedUuids);
  UpdateSceneData();
  HighlightDuplicateFixtureIds();
  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }
}

static FixtureTablePanel *s_instance = nullptr;

FixtureTablePanel *FixtureTablePanel::Instance() { return s_instance; }

void FixtureTablePanel::SetInstance(FixtureTablePanel *panel) {
  s_instance = panel;
}

bool FixtureTablePanel::IsActivePage() const {
  auto *nb = dynamic_cast<wxNotebook *>(GetParent());
  return nb && nb->GetPage(nb->GetSelection()) == this;
}

void FixtureTablePanel::HighlightFixture(const std::string &uuid) {
  for (size_t i = 0; i < rowUuids.size(); ++i) {
    if (!uuid.empty() && rowUuids[i] == uuid)
      store.SetRowBackgroundColour(i, wxColour(0, 200, 0));
    else
      store.ClearRowBackground(i);
  }
  table->Refresh();
}

void FixtureTablePanel::HighlightPatchConflicts() {
  // Clear previous highlighting on Universe and Channel columns
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    store.ClearCellTextColour(i, 5);
    store.ClearCellTextColour(i, 6);
  }

  struct PatchInfo {
    int start;
    int end;
    unsigned row;
  };
  std::unordered_map<int, std::vector<PatchInfo>> uniMap;

  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    wxVariant v;
    table->GetValue(v, i, 5);
    long uni = v.GetLong();
    table->GetValue(v, i, 6);
    long ch = v.GetLong();
    table->GetValue(v, i, 8);
    long count = 1;
    if (!v.GetString().ToLong(&count))
      count = 1;

    if (uni <= 0 || ch <= 0 || count <= 0)
      continue;

    PatchInfo info{static_cast<int>(ch), static_cast<int>(ch + count - 1), i};
    uniMap[static_cast<int>(uni)].push_back(info);
  }

  for (auto &[uni, vec] : uniMap) {
    std::sort(vec.begin(), vec.end(),
              [](const PatchInfo &a, const PatchInfo &b) {
                return a.start < b.start;
              });

    for (size_t i = 0; i < vec.size(); ++i) {
      for (size_t j = i + 1; j < vec.size(); ++j) {
        if (vec[j].start <= vec[i].end) {
          store.SetCellTextColour(vec[i].row, 5, *wxRED);
          store.SetCellTextColour(vec[i].row, 6, *wxRED);
          store.SetCellTextColour(vec[j].row, 5, *wxRED);
          store.SetCellTextColour(vec[j].row, 6, *wxRED);
        } else {
          break;
        }
      }
    }
  }
}

void FixtureTablePanel::ClearSelection() {
  table->UnselectAll();
  selectionOrder.clear();
}

std::vector<std::string> FixtureTablePanel::GetSelectedUuids() const {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> uuids;
  uuids.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
      uuids.push_back(rowUuids[r]);
  }
  return uuids;
}

void FixtureTablePanel::SelectByUuid(const std::vector<std::string> &uuids) {
  table->UnselectAll();
  selectionOrder.clear();
  for (const auto &u : uuids) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), u);
    if (pos != rowUuids.end()) {
      int row = static_cast<int>(pos - rowUuids.begin());
      table->SelectRow(row);
      selectionOrder.push_back(row);
    }
  }
}

void FixtureTablePanel::DeleteSelected() {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.empty())
    return;

  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("delete fixture");

  std::vector<int> rows;
  rows.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND)
      rows.push_back(r);
  }
  std::sort(rows.begin(), rows.end(), std::greater<int>());

  auto &scene = ConfigManager::Get().GetScene();
  for (int r : rows) {
    if ((size_t)r < rowUuids.size()) {
      scene.fixtures.erase(rowUuids[r]);
      rowUuids.erase(rowUuids.begin() + r);
      if ((size_t)r < gdtfPaths.size())
        gdtfPaths.erase(gdtfPaths.begin() + r);
      table->DeleteItem(r);
      for (auto itSel = selectionOrder.begin();
           itSel != selectionOrder.end();) {
        if (*itSel == r)
          itSel = selectionOrder.erase(itSel);
        else {
          if (*itSel > r)
            --(*itSel);
          ++itSel;
        }
      }
    }
  }

  HighlightDuplicateFixtureIds();

  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }

  std::vector<std::string> order = rowUuids;
  ResyncRows(order, {});
}

void FixtureTablePanel::OnItemActivated(wxDataViewEvent &event) {
  wxDataViewItem item = event.GetItem();
  if (!item.IsOk()) {
    event.Skip();
    return;
  }
  int r = table->ItemToRow(item);
  if (r == wxNOT_FOUND)
    return;

  FixtureEditDialog dlg(this, r);
  dlg.ShowModal();
}

void FixtureTablePanel::OnLeftDown(wxMouseEvent &evt) {
  wxDataViewItem item;
  wxDataViewColumn *col;
  table->HitTest(evt.GetPosition(), item, col);
  startRow = table->ItemToRow(item);
  if (startRow != wxNOT_FOUND) {
    dragSelecting = true;
    table->UnselectAll();
    table->SelectRow(startRow);
    CaptureMouse();
  }
  evt.Skip();
}

void FixtureTablePanel::OnLeftUp(wxMouseEvent &evt) {
  if (dragSelecting) {
    dragSelecting = false;
    ReleaseMouse();
  }
  evt.Skip();
}

void FixtureTablePanel::OnMouseMove(wxMouseEvent &evt) {
  if (!dragSelecting || !evt.Dragging()) {
    evt.Skip();
    return;
  }

  wxDataViewItem item;
  wxDataViewColumn *col;
  table->HitTest(evt.GetPosition(), item, col);
  int row = table->ItemToRow(item);
  if (row != wxNOT_FOUND) {
    int minRow = std::min(startRow, row);
    int maxRow = std::max(startRow, row);
    table->UnselectAll();
    for (int r = minRow; r <= maxRow; ++r)
      table->SelectRow(r);
  }
  evt.Skip();
}

void FixtureTablePanel::OnSelectionChanged(wxDataViewEvent &evt) {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<int> currentRows;
  currentRows.reserve(selections.size());
  std::vector<std::string> uuids;
  uuids.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size()) {
      currentRows.push_back(r);
      uuids.push_back(rowUuids[r]);
    }
  }
  // Preserve existing order but drop unselected rows
  std::vector<int> newOrder;
  for (int r : selectionOrder)
    if (std::find(currentRows.begin(), currentRows.end(), r) !=
        currentRows.end())
      newOrder.push_back(r);
  // Append newly selected rows in the order reported
  for (int r : currentRows)
    if (std::find(newOrder.begin(), newOrder.end(), r) == newOrder.end())
      newOrder.push_back(r);
  selectionOrder.swap(newOrder);
  ConfigManager &cfg = ConfigManager::Get();
  if (uuids != cfg.GetSelectedFixtures()) {
    cfg.PushUndoState("fixture selection");
    cfg.SetSelectedFixtures(uuids);
  }
  if (Viewer3DPanel::Instance())
    Viewer3DPanel::Instance()->SetSelectedFixtures(uuids);
  evt.Skip();
}

void FixtureTablePanel::PropagateTypeValues(
    const wxDataViewItemArray &selections, int col) {
  if (col != 16 && col != 17 && col != 18)
    return;

  if (col == 18) {
    std::unordered_map<std::string, wxVariant> typeValues;
    for (const auto &it : selections) {
      int r = table->ItemToRow(it);
      if (r == wxNOT_FOUND)
        continue;
      wxVariant vType, vVal;
      table->GetValue(vType, r, 2);
      table->GetValue(vVal, r, col);
      typeValues[std::string(vType.GetString().ToUTF8())] = vVal;
    }

    unsigned int rowCount = table->GetItemCount();
    for (unsigned int i = 0; i < rowCount; ++i) {
      wxVariant vType;
      table->GetValue(vType, i, 2);
      auto it = typeValues.find(std::string(vType.GetString().ToUTF8()));
      if (it != typeValues.end()) {
        table->SetValue(it->second, i, col);
      }
    }
    return;
  }

  std::unordered_map<std::string, wxString> typeValues;
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r == wxNOT_FOUND)
      continue;
    wxVariant vType, vVal;
    table->GetValue(vType, r, 2);
    table->GetValue(vVal, r, col);
    typeValues[std::string(vType.GetString().ToUTF8())] = vVal.GetString();
  }

  unsigned int rowCount = table->GetItemCount();
  for (unsigned int i = 0; i < rowCount; ++i) {
    wxVariant vType;
    table->GetValue(vType, i, 2);
    auto it = typeValues.find(std::string(vType.GetString().ToUTF8()));
    if (it != typeValues.end())
      table->SetValue(wxVariant(it->second), i, col);
  }
}

void FixtureTablePanel::UpdateSceneData() {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("edit fixture");
  auto &scene = cfg.GetScene();
  size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
  for (size_t i = 0; i < count; ++i) {
    auto it = scene.fixtures.find(rowUuids[i]);
    if (it == scene.fixtures.end())
      continue;

    if (i < gdtfPaths.size())
      it->second.gdtfSpec = std::string(gdtfPaths[i].ToUTF8());

    wxVariant v;
    table->GetValue(v, i, 1);
    it->second.instanceName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 0);
    long fid = v.GetLong();
    it->second.fixtureId = static_cast<int>(fid);

    table->GetValue(v, i, 3);
    std::string layerStr = std::string(v.GetString().ToUTF8());
    if (layerStr.empty())
      it->second.layer.clear();
    else
      it->second.layer = layerStr;

    table->GetValue(v, i, 5);
    long uni = v.GetLong();

    table->GetValue(v, i, 6);
    long ch = v.GetLong();

    table->GetValue(v, i, 2);
    it->second.typeName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 7);
    it->second.gdtfMode = std::string(v.GetString().ToUTF8());

    if (uni > 0 && ch > 0)
      it->second.address = wxString::Format("%ld.%ld", uni, ch).ToStdString();
    else
      it->second.address.clear();

    double x = 0, y = 0, z = 0;
    table->GetValue(v, i, 10);
    v.GetString().ToDouble(&x);
    table->GetValue(v, i, 11);
    v.GetString().ToDouble(&y);
    table->GetValue(v, i, 12);
    v.GetString().ToDouble(&z);

    double rx = 0, ry = 0, rz = 0;
    table->GetValue(v, i, 13);
    {
      wxString s = v.GetString();
      s.Replace("\u00B0", "");
      s.ToDouble(&rx);
    }
    table->GetValue(v, i, 14);
    {
      wxString s = v.GetString();
      s.Replace("\u00B0", "");
      s.ToDouble(&ry);
    }
    table->GetValue(v, i, 15);
    {
      wxString s = v.GetString();
      s.Replace("\u00B0", "");
      s.ToDouble(&rz);
    }

    Matrix rot = MatrixUtils::EulerToMatrix(
        static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rz));
    rot.o = {static_cast<float>(x * 1000.0), static_cast<float>(y * 1000.0),
             static_cast<float>(z * 1000.0)};
    it->second.transform = rot;

    table->GetValue(v, i, 16);
    double pw = 0.0;
    v.GetString().ToDouble(&pw);
    it->second.powerConsumptionW = static_cast<float>(pw);

    table->GetValue(v, i, 17);
    double wt = 0.0;
    v.GetString().ToDouble(&wt);
    it->second.weightKg = static_cast<float>(wt);

    table->GetValue(v, i, 18);
    it->second.color = std::string(v.GetString().ToUTF8());
  }

  HighlightDuplicateFixtureIds();

  if (SummaryPanel::Instance() && IsActivePage())
    SummaryPanel::Instance()->ShowFixtureSummary();
}

void FixtureTablePanel::ApplyModeForGdtf(const wxString &path,
                                         const wxString &preferredMode) {
  if (path.empty())
    return;

  std::vector<std::string> modes = GetGdtfModes(std::string(path.ToUTF8()));
  if (modes.empty())
    return;

  auto toLower = [](const std::string &s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
  };

  for (size_t i = 0; i < gdtfPaths.size() && i < (size_t)table->GetItemCount();
       ++i) {
    if (gdtfPaths[i] != path)
      continue;

    wxVariant v;
    table->GetValue(v, i, 7);
    wxString currWx = v.GetString();
    std::string curr = std::string(currWx.ToUTF8());

    std::string chosen;
    if (!preferredMode.empty()) {
      std::string pref = std::string(preferredMode.ToUTF8());
      if (std::find(modes.begin(), modes.end(), pref) != modes.end())
        chosen = pref;
    }

    if (chosen.empty()) {
      chosen = curr;
      bool found = std::find(modes.begin(), modes.end(), curr) != modes.end();
      if (!found) {
        for (const std::string &m : modes) {
          std::string low = toLower(m);
          if (low == "default" || low == "standard") {
            chosen = m;
            found = true;
            break;
          }
        }
        if (!found)
          chosen = modes.front();
      }
    }

    if (chosen != curr)
      table->SetValue(wxVariant(wxString::FromUTF8(chosen)), i, 7);

    int chCount = GetGdtfModeChannelCount(std::string(path.ToUTF8()), chosen);
    wxString chStr =
        chCount >= 0 ? wxString::Format("%d", chCount) : wxString();
    table->SetValue(wxVariant(chStr), i, 8);
  }
}

void FixtureTablePanel::HighlightDuplicateFixtureIds() {
  // Clear existing text colour highlights
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    store.ClearRowTextColour(i);
    store.ClearCellTextColour(i, 0); // Fixture ID column
  }

  std::unordered_map<long, std::vector<unsigned>> idRows;
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    wxVariant v;
    table->GetValue(v, i, 0); // Fixture ID column
    long id = v.GetLong();
    idRows[id].push_back(i);
  }

  for (const auto &it : idRows) {
    if (it.second.size() > 1) {
      for (unsigned r : it.second)
        store.SetCellTextColour(r, 0, *wxRED);
    }
  }

  HighlightPatchConflicts();
  table->Refresh();
}

void FixtureTablePanel::ResyncRows(
    const std::vector<std::string> &oldOrder,
    const std::vector<std::string> &selectedUuids) {
  unsigned int count = table->GetItemCount();
  std::vector<std::string> newOrder(count);
  std::vector<wxString> newPaths(count);
  for (unsigned int i = 0; i < count; ++i) {
    wxDataViewItem it = table->RowToItem(i);
    unsigned long idx = store.GetItemData(it);
    if (idx < oldOrder.size()) {
      newOrder[i] = oldOrder[idx];
      if (idx < gdtfPaths.size())
        newPaths[i] = gdtfPaths[idx];
    }
    store.SetItemData(it, i);
  }
  rowUuids.swap(newOrder);
  gdtfPaths.swap(newPaths);

  table->UnselectAll();
  for (const auto &uuid : selectedUuids) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
    if (pos != rowUuids.end())
      table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
  }
}

void FixtureTablePanel::OnColumnSorted(wxDataViewEvent &event) {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> selectedUuids;
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
      selectedUuids.push_back(rowUuids[r]);
  }
  std::vector<std::string> oldOrder = rowUuids;
  ResyncRows(oldOrder, selectedUuids);
  event.Skip();
}
