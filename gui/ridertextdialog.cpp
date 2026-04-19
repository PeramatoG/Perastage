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
#include "ridertextdialog.h"

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/dataview.h>
#include <wx/msgdlg.h>
#include <wx/popupwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/strconv.h>
#include <wx/textctrl.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>

#include "projectutils.h"
#include "consolepanel.h"
#include "riderimporter.h"

enum {
  ID_RiderText_Load = wxID_HIGHEST + 4200,
  ID_RiderText_Example,
  ID_RiderText_ApplyFilter,
  ID_RiderText_Apply
};

wxBEGIN_EVENT_TABLE(RiderTextDialog, wxDialog)
EVT_BUTTON(ID_RiderText_Load, RiderTextDialog::OnLoadFromFile)
EVT_BUTTON(ID_RiderText_Example, RiderTextDialog::OnLoadExample)
EVT_BUTTON(ID_RiderText_ApplyFilter, RiderTextDialog::OnApplyFilter)
EVT_BUTTON(ID_RiderText_Apply, RiderTextDialog::OnApply)
wxEND_EVENT_TABLE()

RiderTextDialog::RiderTextDialog(wxWindow *parent,
                                 const wxString &initialText,
                                 const wxString &initialSource)
    : wxDialog(parent, wxID_ANY, "Create scene from text",
               wxDefaultPosition, wxSize(720, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      sourceLabel(initialSource) {
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer *headerSizer = new wxBoxSizer(wxHORIZONTAL);
  const wxString sourceTextLabel =
      sourceLabel.empty() ? wxString("No source loaded.")
                          : wxString("Loaded: ") + sourceLabel;
  sourceText = new wxStaticText(this, wxID_ANY, sourceTextLabel);
  headerSizer->Add(sourceText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  wxButton *loadButton = new wxButton(this, ID_RiderText_Load, "Load rider...");
  headerSizer->Add(loadButton, 0);
  wxButton *exampleButton =
      new wxButton(this, ID_RiderText_Example, "Use example");
  headerSizer->Add(exampleButton, 0, wxLEFT, 8);
  mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 8);

  textCtrl = new wxTextCtrl(this, wxID_ANY, initialText,
                            wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_RICH2);
  textCtrl->SetMinSize(wxSize(680, 360));
  mainSizer->Add(textCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  suggestionPopup = new wxPopupTransientWindow(this, wxBORDER_SIMPLE);
  wxBoxSizer *popupSizer = new wxBoxSizer(wxVERTICAL);
  suggestionList = new wxDataViewListCtrl(
      suggestionPopup, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxDV_ROW_LINES | wxDV_NO_HEADER);
  suggestionList->AppendIconTextColumn("Suggestion", wxDATAVIEW_CELL_INERT, 220,
                                       wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
  suggestionList->AppendTextColumn("Value", wxDATAVIEW_CELL_INERT, 120,
                                   wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
  popupSizer->Add(suggestionList, 1, wxEXPAND);
  suggestionPopup->SetSizerAndFit(popupSizer);
  suggestionPopup->Hide();

  if (textCtrl) {
    textCtrl->Bind(wxEVT_TEXT, &RiderTextDialog::OnTextChanged, this);
    textCtrl->Bind(wxEVT_KEY_DOWN, &RiderTextDialog::OnTextKeyDown, this);
  }
  if (suggestionList) {
    suggestionList->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                         &RiderTextDialog::OnSuggestionClick, this);
  }
  autocompleteTimer.SetOwner(this);
  Bind(wxEVT_TIMER, &RiderTextDialog::OnAutocompleteTimer, this);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *filterButton =
      new wxButton(this, ID_RiderText_ApplyFilter, "Apply filter");
  wxButton *applyButton = new wxButton(this, ID_RiderText_Apply, "Create");
  wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(filterButton, 0, wxRIGHT, 8);
  buttonSizer->Add(applyButton, 0, wxRIGHT, 8);
  buttonSizer->Add(cancelButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  Layout();
  Centre();
}

const std::string &RiderTextDialog::GetRiderTextUtf8() const {
  return selectedRiderTextUtf8;
}

wxString RiderTextDialog::GetLoadedFileTitle() const {
  if (!sourceLoadedFromFile)
    return wxEmptyString;

  const wxString fileTitle = wxFileName(sourceLabel).GetName();
  if (fileTitle.IsEmpty())
    return wxEmptyString;
  return fileTitle;
}

bool RiderTextDialog::TryGetCurrentText(std::string &outText) const {
  if (!textCtrl)
    return false;

  const wxString value = textCtrl->GetValue();
  const wxScopedCharBuffer textBuffer = value.ToUTF8();
  outText = textBuffer ? std::string(textBuffer.data(), textBuffer.length())
                       : value.ToStdString();
  return true;
}

void RiderTextDialog::OnLoadFromFile(wxCommandEvent &WXUNUSED(event)) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog dlg(this, "Import Rider", miscDir, "",
                   "Rider files (*.txt;*.pdf)|*.txt;*.pdf",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  const std::filesystem::path selectedPath(dlg.GetPath().ToStdWstring());
  const auto pathU8 = selectedPath.u8string();
  const std::string pathUtf8(pathU8.begin(), pathU8.end());
  std::string text;
  try {
    text = RiderImporter::LoadText(pathUtf8);
  } catch (const std::exception &ex) {
    if (ConsolePanel *console = ConsolePanel::Instance()) {
      console->AppendMessage("Rider import exception: " +
                             wxString::FromUTF8(ex.what()));
    }
    wxMessageBox("Unexpected error while loading rider file.", "Error",
                 wxICON_ERROR);
    return;
  } catch (...) {
    if (ConsolePanel *console = ConsolePanel::Instance())
      console->AppendMessage("Rider import exception: unknown error.");
    wxMessageBox("Unexpected unknown error while loading rider file.", "Error",
                 wxICON_ERROR);
    return;
  }
  if (text.empty()) {
    if (ConsolePanel *console = ConsolePanel::Instance()) {
      console->AppendMessage("Rider import: no visible text extracted from " +
                             dlg.GetFilename());
    }
    wxMessageBox("Failed to import rider.", "Error", wxICON_ERROR);
    return;
  }
  wxString loadedText = wxString::FromUTF8(text.data(), text.size());
  if (loadedText.empty() && !text.empty()) {
    loadedText = wxString::From8BitData(text.data(), text.size());
  }
  if (loadedText.empty()) {
    if (ConsolePanel *console = ConsolePanel::Instance()) {
      console->AppendMessage("Rider import: extracted text could not be "
                             "decoded for " + dlg.GetFilename());
    }
    wxMessageBox("Loaded rider text could not be decoded.", "Error",
                 wxICON_ERROR);
    return;
  }
  sourceLabel = dlg.GetFilename();
  sourceLoadedFromFile = true;
  if (sourceText)
    sourceText->SetLabel(wxString("Loaded: ") + sourceLabel);
  textCtrl->ChangeValue(loadedText);
  autocompleteProvider.RefreshDynamicTerms();
}

void RiderTextDialog::OnLoadExample(wxCommandEvent &WXUNUSED(event)) {
  const wxString exampleText =
      "LX1 \n"
      "8 Blinder 2\n"
      "8 Spiider\n"
      "6 Megapointe\n"
      "\n"
      "LX2 \n"
      "6 Megapointe\n"
      "6 Mac Viper Profile\n"
      "6 Spiider\n"
      "4 Q-7\n"
      "\n"
      "LX3\n"
      "6 Spiider\n"
      "6 Megapointe\n"
      "6 Mac Viper Profile\n"
      "4 Q-7\n"
      "\n"
      "LX4 \n"
      "3 Quantum Wash\n"
      "3 Quantum Spot\n"
      "3 Quantum Wash\n"
      "\n"
      "SIDES\n"
      "8 Quantum Wash\n"
      "\n"
      "FLOOR\n"
      "4 Tour Hazer 2\n"
      "\n"
      "SCREEN\n"
      "1 Screen 8x5m\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14 m LX1 (-2.0, 10.5) [0.6] *(Position and margin override)*\n"
      "1 TRUSS 40X40 12 m LX2 [0.8]\n"
      "1 TRUSS 40X40 12 m LX3 [1]\n"
      "1 TRUSS 40X40 12 m LX4\n"
      "1 TRUSS 40X40 6 m SIDES (1, 4) \n"
      "1 TRUSS 40X40 10 m SCREEN\n"
      "1 PIPE 12 m BACKDROP *(Pipe as scene object)*\n"
      "4 MOTOR 500Kg FOR LX1 *(Auto-distribute across LX1 target)*\n"
      "2 MOTOR 1000Kg FOR LX2\n"
      "2 MOTOR 1000Kg FOR LX3\n"
      "2 MOTOR 1000Kg FOR LX4\n"
      "4 MOTOR 1000Kg FOR SIDES\n"
      "4 MOTOR 1000Kg FOR SCREEN\n";
  textCtrl->ChangeValue(exampleText);
  sourceLabel = "Example text";
  sourceLoadedFromFile = false;
  if (sourceText)
    sourceText->SetLabel(wxString("Loaded: ") + sourceLabel);
  autocompleteProvider.RefreshDynamicTerms();
}

void RiderTextDialog::OnApplyFilter(wxCommandEvent &WXUNUSED(event)) {
  std::string text;
  if (!TryGetCurrentText(text) || text.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }

  const std::string filtered = RiderImporter::BuildFixtureFilterPreview(text);
  if (filtered.empty()) {
    wxMessageBox("No fixture lines were detected with the current parser "
                 "rules after filtering.",
                 "Apply filter", wxICON_INFORMATION);
    return;
  }

  wxString filteredText = wxString::FromUTF8(filtered.data(), filtered.size());
  if (filteredText.empty() && !filtered.empty())
    filteredText = wxString::From8BitData(filtered.data(), filtered.size());
  if (filteredText.empty()) {
    wxMessageBox("Filtered text could not be decoded.", "Error",
                 wxICON_ERROR);
    return;
  }
  textCtrl->ChangeValue(filteredText);
  autocompleteProvider.RefreshDynamicTerms();
}

void RiderTextDialog::OnApply(wxCommandEvent &WXUNUSED(event)) {
  std::string text;
  if (!TryGetCurrentText(text) || text.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }
  if (!ValidateAndNormalizeText(text)) {
    wxMessageBox(wxString::FromUTF8(lastValidationError), "Validation error",
                 wxICON_ERROR | wxOK, this);
    return;
  }
  selectedRiderTextUtf8 = std::move(text);
  EndModal(wxID_OK);
}

void RiderTextDialog::OnTextChanged(wxCommandEvent &event) {
  if (!suppressAutocompleteTextEvent)
    autocompleteTimer.StartOnce(35);
  event.Skip();
}

void RiderTextDialog::OnTextKeyDown(wxKeyEvent &event) {
  if (IsSuggestionPopupVisible()) {
    switch (event.GetKeyCode()) {
    case WXK_UP:
      if (suggestionList && suggestionList->GetItemCount() > 0) {
        const int nextSelection = std::max(
            0, static_cast<int>(suggestionList->GetSelectedRow()) - 1);
        suggestionList->SelectRow(nextSelection);
        return;
      }
      break;
    case WXK_DOWN:
      if (suggestionList && suggestionList->GetItemCount() > 0) {
        const int maxIndex =
            static_cast<int>(suggestionList->GetItemCount()) - 1;
        const int nextSelection =
            std::min(maxIndex, static_cast<int>(suggestionList->GetSelectedRow()) + 1);
        suggestionList->SelectRow(nextSelection);
        return;
      }
      break;
    case WXK_ESCAPE:
      HideSuggestionPopup();
      return;
    case WXK_TAB:
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
      if (AcceptCurrentSuggestion())
        return;
      break;
    default:
      break;
    }
  }

  event.Skip();
}

void RiderTextDialog::OnAutocompleteTimer(wxTimerEvent &WXUNUSED(event)) {
  RefreshAutocompleteSuggestions();
}

void RiderTextDialog::OnSuggestionClick(wxDataViewEvent &WXUNUSED(event)) {
  AcceptCurrentSuggestion();
}

void RiderTextDialog::RefreshAutocompleteSuggestions() {
  if (!textCtrl || !suggestionPopup || !suggestionList)
    return;

  autocompleteProvider.RefreshDynamicTerms();
  const wxString currentText = textCtrl->GetValue();
  const wxScopedCharBuffer utf8Text = currentText.ToUTF8();
  const std::string textUtf8 =
      utf8Text ? std::string(utf8Text.data(), utf8Text.length())
               : currentText.ToStdString();
  const long insertionPoint = textCtrl->GetInsertionPoint();
  const wxString leftText = currentText.Left(insertionPoint);
  const wxScopedCharBuffer leftUtf8 = leftText.ToUTF8();
  const std::string leftUtf8Text =
      leftUtf8 ? std::string(leftUtf8.data(), leftUtf8.length())
               : leftText.ToStdString();

  currentSuggestions =
      autocompleteProvider.Query(textUtf8, leftUtf8Text.size(), 10);

  if (currentSuggestions.empty()) {
    HideSuggestionPopup();
    return;
  }

  suggestionList->DeleteAllItems();
  for (const RiderTextAutocompleteProvider::Suggestion &suggestion :
       currentSuggestions) {
    wxVector<wxVariant> row;
    wxBitmap chip(14, 14);
    if (!suggestion.colorHex.empty()) {
      const wxString hex = wxString::FromUTF8(suggestion.colorHex);
      wxColour fill(hex);
      if (!fill.IsOk())
        fill = *wxLIGHT_GREY;
      const double luminance =
          0.299 * fill.Red() + 0.587 * fill.Green() + 0.114 * fill.Blue();
      const wxColour border =
          luminance < 85.0 ? wxColour(220, 220, 220) : wxColour(60, 60, 60);
      wxMemoryDC dc(chip);
      dc.SetBackground(*wxTRANSPARENT_BRUSH);
      dc.Clear();
      dc.SetBrush(wxBrush(fill));
      dc.SetPen(wxPen(border, 1));
      dc.DrawRectangle(1, 1, 12, 12);
      dc.SelectObject(wxNullBitmap);
    }

    row.push_back(wxVariant(wxDataViewIconText(
        wxString::FromUTF8(suggestion.displayText), chip)));
    row.push_back(wxVariant(wxString::FromUTF8(suggestion.insertText)));
    suggestionList->AppendItem(row);
  }
  suggestionList->SelectRow(0);

  wxPoint popupAnchor = textCtrl->ClientToScreen(wxPoint(8, 8));
  const wxPoint cursorPos = textCtrl->PositionToCoords(insertionPoint);
  if (cursorPos != wxDefaultPosition) {
    popupAnchor = textCtrl->ClientToScreen(
        wxPoint(cursorPos.x, cursorPos.y + textCtrl->GetCharHeight() + 6));
  }

  const int visibleRows = std::min<int>(6, static_cast<int>(suggestionList->GetItemCount()));
  const wxSize popupSize(std::max(280, textCtrl->GetSize().GetWidth() / 2),
                         std::max(120, visibleRows * (textCtrl->GetCharHeight() + 8)));
  suggestionPopup->SetSize(popupAnchor.x, popupAnchor.y, popupSize.GetWidth(),
                           popupSize.GetHeight());
  suggestionPopup->Popup(textCtrl);
}

void RiderTextDialog::HideSuggestionPopup() {
  if (suggestionPopup && suggestionPopup->IsShown())
    suggestionPopup->Dismiss();
}

bool RiderTextDialog::AcceptCurrentSuggestion() {
  if (!suggestionList || !IsSuggestionPopupVisible())
    return false;

  const int selection = static_cast<int>(suggestionList->GetSelectedRow());
  if (selection == wxNOT_FOUND || selection < 0 ||
      static_cast<size_t>(selection) >= currentSuggestions.size()) {
    HideSuggestionPopup();
    return false;
  }

  const RiderTextAutocompleteProvider::Suggestion &selected =
      currentSuggestions[static_cast<size_t>(selection)];
  ReplaceCurrentToken(selected.insertText);
  autocompleteProvider.RecordSuggestionAccepted(selected.insertText);
  HideSuggestionPopup();
  return true;
}

bool RiderTextDialog::IsSuggestionPopupVisible() const {
  return suggestionPopup && suggestionPopup->IsShown();
}

void RiderTextDialog::ReplaceCurrentToken(const std::string &replacement) {
  if (!textCtrl)
    return;

  const wxString text = textCtrl->GetValue();
  long insertionPoint = textCtrl->GetInsertionPoint();
  insertionPoint = std::max<long>(0, std::min<long>(insertionPoint, text.length()));

  long tokenStart = insertionPoint;
  while (tokenStart > 0 && !IsTokenDelimiter(text[tokenStart - 1]))
    --tokenStart;

  long tokenEnd = insertionPoint;
  while (tokenEnd < static_cast<long>(text.length()) &&
         !IsTokenDelimiter(text[tokenEnd])) {
    ++tokenEnd;
  }

  suppressAutocompleteTextEvent = true;
  const wxString replacementText = wxString::FromUTF8(replacement);
  textCtrl->Replace(tokenStart, tokenEnd, replacementText);
  textCtrl->SetInsertionPoint(tokenStart + replacementText.length());
  suppressAutocompleteTextEvent = false;
}

bool RiderTextDialog::IsTokenDelimiter(wxUniChar c) {
  switch (c.GetValue()) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
  case ',':
  case ';':
  case ':':
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
  case '"':
    return true;
  default:
    return false;
  }
}

bool RiderTextDialog::TryNormalizeColorToken(const std::string &token,
                                             std::string &normalizedHex) {
  const auto toLower = [](std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
  };
  const auto trim = [](const std::string &value) {
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
      return std::string();
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
  };

  const std::string trimmed = trim(token);
  if (trimmed.empty())
    return false;

  static const std::unordered_map<std::string, std::string> kColorAliases = {
      {"red", "#ff0000"},       {"green", "#00ff00"},    {"blue", "#0000ff"},
      {"amber", "#ffbf00"},     {"warm white", "#ffd7a3"},{"cool white", "#f3f8ff"},
      {"white", "#ffffff"},     {"cyan", "#00ffff"},     {"magenta", "#ff00ff"},
      {"yellow", "#ffff00"},    {"orange", "#ff7f00"},   {"pink", "#ff69b4"},
      {"purple", "#8f00ff"},    {"lavender", "#c7a3c7"}, {"lime", "#bfff00"},
      {"indigo", "#4b0082"},    {"teal", "#008080"},     {"gold", "#ffd700"},
      {"ctb", "#b8d8ff"},       {"cto", "#ffb347"}};

  if (trimmed.size() == 7 && trimmed[0] == '#') {
    if (std::all_of(trimmed.begin() + 1, trimmed.end(), [](unsigned char c) {
          return std::isxdigit(c) != 0;
        })) {
      normalizedHex = toLower(trimmed);
      return true;
    }
    return false;
  }

  std::smatch rgbMatch;
  if (std::regex_match(trimmed, rgbMatch,
                       std::regex("^rgb\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*\\)$",
                                  std::regex::icase))) {
    const int r = std::stoi(rgbMatch[1].str());
    const int g = std::stoi(rgbMatch[2].str());
    const int b = std::stoi(rgbMatch[3].str());
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
      return false;
    char hexBuffer[8];
    std::snprintf(hexBuffer, sizeof(hexBuffer), "#%02x%02x%02x", r, g, b);
    normalizedHex = hexBuffer;
    return true;
  }

  const auto aliasIt = kColorAliases.find(toLower(trimmed));
  if (aliasIt != kColorAliases.end()) {
    normalizedHex = aliasIt->second;
    return true;
  }

  return false;
}

bool RiderTextDialog::ValidateAndNormalizeText(std::string &text) {
  lastValidationError.clear();
  if (text.empty())
    return true;

  const std::regex colorDirectiveRe(
      "\\b(?:colou?r|col)\\s*[:=]?\\s*([^,;\\n]+)", std::regex::icase);

  std::stringstream input(text);
  std::string line;
  std::vector<std::string> normalizedLines;
  int lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    line = std::regex_replace(line, std::regex("\\s+"), " ");
    std::smatch match;
    if (std::regex_search(line, match, colorDirectiveRe) && match.size() > 1) {
      const std::string colorToken = match[1].str();
      std::string normalizedHex;
      if (!TryNormalizeColorToken(colorToken, normalizedHex)) {
        lastValidationError = "Invalid color format on line " +
                              std::to_string(lineNumber) + ": '" + colorToken +
                              "'. Use #RRGGBB, rgb(r,g,b), or a supported name.";
        return false;
      }
      const std::string replacement = "color " + normalizedHex;
      line.replace(static_cast<size_t>(match.position(0)),
                   static_cast<size_t>(match.length(0)), replacement);
    }
    normalizedLines.push_back(line);
  }

  text.clear();
  for (size_t i = 0; i < normalizedLines.size(); ++i) {
    text.append(normalizedLines[i]);
    if (i + 1 < normalizedLines.size())
      text.push_back('\n');
  }
  return true;
}
