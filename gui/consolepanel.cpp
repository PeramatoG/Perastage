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
#include "consolepanel.h"
#include "console_command_parser.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "mainwindow.h"
#include "matrixutils.h"
#include "scene_grouping.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <fstream>
#include <charconv>
#include <cctype>
#include <exception>
#include <optional>
#include <sstream>
#include <vector>
#include <wx/intl.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {

enum class ConsoleMessageKind {
  Default,
  Error,
  Warning,
  Command,
  Info,
};

ConsoleMessageKind DetectMessageKind(const wxString &message) {
  if (!message.StartsWith("["))
    return ConsoleMessageKind::Default;

  const int closing = message.Find(']');
  if (closing == wxNOT_FOUND || closing <= 1)
    return ConsoleMessageKind::Default;

  const wxString tag = message.Mid(1, closing - 1).Upper();
  if (tag == "ERROR")
    return ConsoleMessageKind::Error;
  if (tag == "WARNING")
    return ConsoleMessageKind::Warning;
  if (tag == "CMD")
    return ConsoleMessageKind::Command;
  if (tag == "INFO")
    return ConsoleMessageKind::Info;

  return ConsoleMessageKind::Default;
}

wxColour ColorForMessageKind(ConsoleMessageKind kind) {
  switch (kind) {
  case ConsoleMessageKind::Error:
    return wxColour(255, 80, 80);
  case ConsoleMessageKind::Warning:
    return wxColour(255, 180, 60);
  case ConsoleMessageKind::Command:
    return wxColour(235, 235, 235);
  case ConsoleMessageKind::Info:
  case ConsoleMessageKind::Default:
  default:
    return wxColour(0, 255, 0);
  }
}

void AppendStyledConsoleLine(wxTextCtrl *textCtrl, const wxString &line) {
  if (!textCtrl)
    return;

  const wxColour messageColour = ColorForMessageKind(DetectMessageKind(line));
  textCtrl->SetDefaultStyle(wxTextAttr(messageColour));
  textCtrl->AppendText(line + "\n");
}

wxString ReadUtf8File(const wxString &path) {
  std::ifstream in(path.ToStdString());
  if (!in)
    return {};
  std::stringstream buffer;
  buffer << in.rdbuf();
  return wxString::FromUTF8(buffer.str());
}

wxString ExtractConsoleSection(const wxString &markdown,
                              const wxString &header) {
  const wxString startToken = "## " + header;
  const int start = markdown.Find(startToken);
  if (start == wxNOT_FOUND)
    return {};

  const int sectionStart = start + static_cast<int>(startToken.length());
  wxString rest = markdown.Mid(sectionStart);
  const int nextHeader = rest.Find("\n## ");
  if (nextHeader != wxNOT_FOUND)
    rest = rest.Left(nextHeader);

  wxArrayString lines = wxSplit(rest, '\n', '\0');
  wxString result;
  for (const wxString &line : lines) {
    wxString clean = line;
    clean.Trim(true).Trim(false);
    if (clean.IsEmpty()) {
      result += "\n";
      continue;
    }

    if (clean.StartsWith("| ---"))
      continue;
    if (clean.StartsWith("### ")) {
      result += clean.Mid(4) + "\n";
      continue;
    }
    if (clean.StartsWith("## ")) {
      result += clean.Mid(3) + "\n";
      continue;
    }
    if (clean.StartsWith("|")) {
      wxString tableLine = clean;
      tableLine.Replace("|", " ");
      tableLine.Trim(true).Trim(false);
      result += tableLine + "\n";
      continue;
    }
    result += clean + "\n";
  }

  return result.Trim();
}


// Builds localized console help while preserving executable command syntax.
wxString BuildLocalizedConsoleHelpContent() {
  wxString help;
  help += _("The console transforms the current mixed selection. Grouped trusses move through their root group; fixtures, supports, and scene objects remain exact targets.");
  help += "\n\n";
  help += _("Selection");
  help += "\n\n";
  help += _("Command    Description");
  help += "\n";
  help += "`clear`    " + _("Clears all selections (fixtures, trusses, scene objects).") + "\n";
  help += "`f ...`    " + _("Select fixtures by ID.") + "\n";
  help += "`t ...`    " + _("Select trusses by unit number (clears current truss selection first).") + "\n\n";
  help += _("Selection syntax supports:");
  help += "\n\n";
  help += "- " + _("Single IDs: `f 12`") + "\n";
  help += "- " + _("Ranges: `f 1-5`, `f 1 thru 5`, `f 1 t 5`") + "\n";
  help += "- " + _("Add/remove: `f + 10 - 3`") + "\n";
  help += "- " + _("Mixed tokens: `f 1 3 5 7-9`") + "\n\n";
  help += _("Position and rotation");
  help += "\n\n";
  help += _("Command          Description");
  help += "\n";
  help += "`pos x <values>`  " + _("Set X positions for the selection.") + "\n";
  help += "`pos y <values>`  " + _("Set Y positions for the selection.") + "\n";
  help += "`pos z <values>`  " + _("Set Z positions for the selection.") + "\n";
  help += "`pos <x>,<y>,<z>` " + _("Set X/Y/Z in one command.") + "\n";
  help += "`x <values>`      " + _("Shortcut for `pos x`.") + "\n";
  help += "`y <values>`      " + _("Shortcut for `pos y`.") + "\n";
  help += "`z <values>`      " + _("Shortcut for `pos z`.") + "\n";
  help += "`rot x <values>`  " + _("Set rotation around X (roll).") + "\n";
  help += "`rot y <values>`  " + _("Set rotation around Y (pitch).") + "\n";
  help += "`rot z <values>`  " + _("Set rotation around Z (yaw).") + "\n";
  help += "`rot x y z <values> --group`  " + _("Rotate the full selection as one group around a pivot.") + "\n";
  help += "`rot x y z <values> --g`      " + _("Alias of `--group`.") + "\n";
  help += "`pos/rot ... --local|-l` " + _("Use local axes for relative transforms.") + "\n\n";
  help += _("Notes:");
  help += "\n\n";
  help += "- " + _("Provide one value to apply it to all selected items.") + "\n";
  help += "- " + _("Provide two values to linearly distribute from start to end across the selection.") + "\n";
  help += "- " + _("Use compact or spaced `++` / `--` relative offsets, such as `++1`, `++ 1`, `--1`, or `-- 1`.") + "\n";
  help += "- " + _("You can also type a comma-separated triplet like `1, 2, 3` as a shortcut for `pos`.") + "\n";
  return help;
}

// Builds localized fallback console help when bundled help text is unavailable.
wxString BuildConsoleHelpContent() {
  if (_("Console commands") != wxString("Console commands"))
    return BuildLocalizedConsoleHelpContent();

  wxFileName helpPath(wxStandardPaths::Get().GetExecutablePath());
  helpPath.SetFullName("help.md");
  const wxString markdown = ReadUtf8File(helpPath.GetFullPath());

  const wxString preferredHeader = "Console Commands (complete)";
  const wxString fallbackHeader = "Comandos de consola (completo)";

  wxString section = ExtractConsoleSection(markdown, preferredHeader);
  if (section.IsEmpty())
    section = ExtractConsoleSection(markdown, fallbackHeader);
  if (!section.IsEmpty())
    return section;

  wxString help = _("Console commands:");
  help += "\n- clear\n- f ...\n- t ...\n";
  help += "- pos x|y|z <values>\n- pos <x>,<y>,<z>\n";
  help += "- x|y|z <values>\n";
  help += "- rot x|y|z <values> [--group|--g] [pivotX,pivotY,pivotZ]\n";
  help += _("Examples:");
  help += "\n- f 1-5\n- pos x 1 4\n- pos x ++1 --local\n- rot z --10\n";
  help += "- rot y ++45 --g --local -2.5,0,0";
  return help;
}

} // namespace

ConsolePanel::ConsolePanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  m_textCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                              wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  m_textCtrl->SetBackgroundColour(*wxBLACK);
  m_textCtrl->SetForegroundColour(wxColour(0, 255, 0));
  wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
              wxFONTWEIGHT_NORMAL);
  m_textCtrl->SetFont(font);
  m_inputCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                               wxDefaultSize, wxTE_PROCESS_ENTER);
  m_inputCtrl->SetFont(font);
  m_inputCtrl->Bind(wxEVT_TEXT_ENTER, &ConsolePanel::OnCommandEnter, this);
  m_inputCtrl->Bind(wxEVT_SET_FOCUS, &ConsolePanel::OnInputFocus, this);
  m_inputCtrl->Bind(wxEVT_KILL_FOCUS, &ConsolePanel::OnInputKillFocus, this);
  m_inputCtrl->Bind(wxEVT_KEY_DOWN, &ConsolePanel::OnInputKeyDown, this);
  m_inputCtrl->SetValue(">>> ");
  m_inputCtrl->SetInsertionPointEnd();
  m_helpButton = new wxButton(this, wxID_ANY, "?", wxDefaultPosition,
                              wxSize(24, 24), wxBU_EXACTFIT);
  m_helpButton->SetToolTip(
      _("Show available console commands and examples."));
  m_helpButton->Bind(wxEVT_BUTTON, &ConsolePanel::OnHelpButton, this);

  wxBoxSizer *inputSizer = new wxBoxSizer(wxHORIZONTAL);
  inputSizer->Add(m_inputCtrl, 1, wxEXPAND);
  inputSizer->Add(m_helpButton, 0, wxLEFT, 4);

  const wxEventTypeTag<wxScrollWinEvent> scrollEvents[] = {
      wxEVT_SCROLLWIN_TOP,        wxEVT_SCROLLWIN_BOTTOM,
      wxEVT_SCROLLWIN_LINEUP,     wxEVT_SCROLLWIN_LINEDOWN,
      wxEVT_SCROLLWIN_PAGEUP,     wxEVT_SCROLLWIN_PAGEDOWN,
      wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE};
  for (const auto &evt : scrollEvents)
    m_textCtrl->Bind(evt, &ConsolePanel::OnScroll, this);
  sizer->Add(m_textCtrl, 1, wxEXPAND | wxALL, 5);
  sizer->Add(inputSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  SetSizer(sizer);
}

void ConsolePanel::OnHelpButton(wxCommandEvent &) {
  wxDialog helpDialog(this, wxID_ANY, _("Console commands"), wxDefaultPosition,
                      wxSize(620, 420),
                      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto *dialogSizer = new wxBoxSizer(wxVERTICAL);
  auto *helpText = new wxTextCtrl(&helpDialog, wxID_ANY,
                                  BuildConsoleHelpContent(), wxDefaultPosition,
                                  wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY);
  helpText->SetBackgroundColour(*wxBLACK);
  helpText->SetForegroundColour(wxColour(230, 230, 230));
  helpText->SetFont(
      wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
  dialogSizer->Add(helpText, 1, wxEXPAND | wxALL, 8);
  dialogSizer->Add(helpDialog.CreateButtonSizer(wxOK), 0,
                  wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  helpDialog.SetSizerAndFit(dialogSizer);
  helpDialog.SetSize(620, 420);
  helpDialog.ShowModal();
}

void ConsolePanel::AppendMessage(const wxString &msg) {
  if (!m_textCtrl)
    return;

  constexpr size_t kMaxConsoleMessageLength = 8 * 1024;
  const wxString suffix = "... (truncated)";
  wxString safeMsg = msg;
  if (safeMsg.length() > kMaxConsoleMessageLength) {
    size_t keepLength =
        kMaxConsoleMessageLength > suffix.length()
            ? kMaxConsoleMessageLength - suffix.length()
            : 0;
    safeMsg = safeMsg.Left(keepLength) + suffix;
  }

  if (safeMsg == m_lastMessage) {
    m_repeatCount++;
    wxString combined = safeMsg + " (repeated " +
                        wxString::Format("%zu", m_repeatCount) + " times)";
    long endPos = m_textCtrl->GetLastPosition();
    if (m_lastLineStart < endPos)
      m_textCtrl->Remove(m_lastLineStart, endPos);
    AppendStyledConsoleLine(m_textCtrl, combined);
  } else {
    m_lastMessage = safeMsg;
    m_repeatCount = 1;
    m_lastLineStart = m_textCtrl->GetLastPosition();
    AppendStyledConsoleLine(m_textCtrl, safeMsg);
  }
  if (m_autoScroll)
    m_textCtrl->ShowPosition(m_textCtrl->GetLastPosition());
}

static ConsolePanel *s_instance = nullptr;

ConsolePanel *ConsolePanel::Instance() { return s_instance; }

void ConsolePanel::SetInstance(ConsolePanel *panel) { s_instance = panel; }

bool ConsolePanel::IsInputWidgetOrChild(const wxWindow *window) const {
  if (!window || !m_inputCtrl)
    return false;

  const wxWindow *current = window;
  while (current) {
    if (current == m_inputCtrl)
      return true;
    current = current->GetParent();
  }
  return false;
}

bool ConsolePanel::InputHasTypedContent() const {
  if (!m_inputCtrl)
    return false;

  wxString value = m_inputCtrl->GetValue();
  if (value.StartsWith(">>> "))
    value = value.Mid(4);
  value.Trim(true).Trim(false);
  return !value.IsEmpty();
}

void ConsolePanel::FocusInputWithOptionalPrefill(const wxString &text) {
  if (!m_inputCtrl)
    return;

  if (!text.IsEmpty() && !InputHasTypedContent())
    m_inputCtrl->SetValue(">>> " + text);

  m_inputCtrl->SetFocus();
  m_inputCtrl->SetInsertionPointEnd();
}

void ConsolePanel::OnScroll(wxScrollWinEvent &event) {
  if (!m_textCtrl) {
    event.Skip();
    return;
  }
  int maxPos = m_textCtrl->GetScrollRange(wxVERTICAL) -
               m_textCtrl->GetScrollThumb(wxVERTICAL);
  int pos = event.GetPosition();
  m_autoScroll = (pos >= maxPos);
  event.Skip();
}

// --- Input handling ---

void ConsolePanel::OnCommandEnter(wxCommandEvent &event) {
  wxString cmd = m_inputCtrl ? m_inputCtrl->GetValue() : wxString();
  if (cmd.StartsWith(">>> "))
    cmd = cmd.Mid(4);
  if (!cmd.IsEmpty()) {
    m_history.push_back(cmd);
    m_historyIndex = m_history.size();
  }
  if (m_inputCtrl) {
    m_inputCtrl->SetValue(">>> ");
    m_inputCtrl->SetInsertionPointEnd();
  }
  ProcessCommand(cmd);
}

void ConsolePanel::OnInputFocus(wxFocusEvent &event) {
  if (MainWindow::Instance())
    MainWindow::Instance()->EnableShortcuts(false);
  if (m_inputCtrl)
    m_inputCtrl->SetInsertionPointEnd();
  event.Skip();
}

void ConsolePanel::OnInputKillFocus(wxFocusEvent &event) {
  if (MainWindow::Instance())
    MainWindow::Instance()->EnableShortcuts(true);
  event.Skip();
}

void ConsolePanel::OnInputKeyDown(wxKeyEvent &event) {
  int code = event.GetKeyCode();
  long pos = m_inputCtrl ? m_inputCtrl->GetInsertionPoint() : 0;
  if (code == WXK_ESCAPE) {
    if (MainWindow::Instance())
      MainWindow::Instance()->EnableShortcuts(true);
    m_inputCtrl->SetValue(">>> ");
    m_inputCtrl->SetInsertionPointEnd();
    if (m_textCtrl)
      m_textCtrl->SetFocus();
    return;
  }
  if ((code == WXK_BACK || code == WXK_LEFT) && pos <= 4) {
    m_inputCtrl->SetInsertionPoint(4);
    return;
  }
  if (code == WXK_HOME) {
    m_inputCtrl->SetInsertionPoint(4);
    return;
  }
  if (code == WXK_UP) {
    if (!m_history.empty() && m_historyIndex > 0) {
      m_historyIndex--;
      m_inputCtrl->SetValue(">>> " + m_history[m_historyIndex]);
      m_inputCtrl->SetInsertionPointEnd();
    }
    return;
  }
  if (code == WXK_DOWN) {
    if (m_historyIndex + 1 < m_history.size()) {
      m_historyIndex++;
      m_inputCtrl->SetValue(">>> " + m_history[m_historyIndex]);
    } else {
      m_historyIndex = m_history.size();
      m_inputCtrl->SetValue(">>> ");
    }
    m_inputCtrl->SetInsertionPointEnd();
    return;
  }
  event.Skip();
}

static std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  size_t end = s.find_last_not_of(" \t\n\r");
  if (start == std::string::npos)
    return std::string();
  return s.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> parts;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim))
    parts.push_back(trim(item));
  return parts;
}

static bool isNumberToken(const std::string &token) {
  if (token.empty())
    return false;
  int value = 0;
  auto begin = token.data();
  auto end = token.data() + token.size();
  auto result = std::from_chars(begin, end, value);
  return result.ec == std::errc{} && result.ptr == end;
}

static std::vector<std::string>
NormalizeRangeTokens(const std::vector<std::string> &tokens) {
  std::vector<std::string> out;
  out.reserve(tokens.size());
  for (const auto &token : tokens) {
    if (token == "+" || token == "-") {
      out.push_back(token);
      continue;
    }
    std::string lower = token;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "t" || lower == "thru")
      continue;
    if (lower.size() > 4 && lower.rfind("thru", 0) == 0) {
      std::string after = token.substr(4);
      if (isNumberToken(after)) {
        out.push_back(after);
        continue;
      }
    }
    if (lower.size() > 1 && lower.rfind("t", 0) == 0) {
      std::string after = token.substr(1);
      if (isNumberToken(after)) {
        out.push_back(after);
        continue;
      }
    }
    if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, "thru") == 0) {
      std::string before = token.substr(0, token.size() - 4);
      if (isNumberToken(before)) {
        out.push_back(before);
        continue;
      }
    }
    if (lower.size() > 1 && lower.back() == 't') {
      std::string before = token.substr(0, token.size() - 1);
      if (isNumberToken(before)) {
        out.push_back(before);
        continue;
      }
    }
    size_t thruPos = lower.find("thru");
    if (thruPos != std::string::npos && thruPos > 0 &&
        thruPos + 4 < token.size()) {
      std::string before = token.substr(0, thruPos);
      std::string after = token.substr(thruPos + 4);
      if (isNumberToken(before) && isNumberToken(after)) {
        out.push_back(before);
        out.push_back(after);
        continue;
      }
    }
    size_t tPos = lower.find('t');
    if (tPos != std::string::npos && tPos > 0 && tPos + 1 < token.size()) {
      std::string before = token.substr(0, tPos);
      std::string after = token.substr(tPos + 1);
      if (isNumberToken(before) && isNumberToken(after)) {
        out.push_back(before);
        out.push_back(after);
        continue;
      }
    }
    size_t dashPos = token.find('-');
    if (dashPos != std::string::npos && dashPos > 0 &&
        dashPos + 1 < token.size() && token.find('-', dashPos + 1) == std::string::npos) {
      std::string before = token.substr(0, dashPos);
      std::string after = token.substr(dashPos + 1);
      if (isNumberToken(before) && isNumberToken(after)) {
        out.push_back(before);
        out.push_back(after);
        continue;
      }
    }
    out.push_back(token);
  }
  return out;
}

// Parses and applies command-bar actions to the current scene selection.
void ConsolePanel::ProcessCommand(const wxString &cmdWx) {
  std::string cmd = std::string(cmdWx.ToUTF8());
  cmd = trim(cmd);
  if (cmd.empty())
    return;

  AppendMessage("[CMD] " + cmdWx);

  try {
    std::string lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();

    auto handleSelection = [&](bool fixtures, bool clearSel,
                               const std::vector<std::string> &tokens) {
      auto &scene = cfg.GetScene();
      std::vector<std::string> current =
          clearSel ? std::vector<std::string>()
                   : (fixtures ? cfg.GetSelectedFixtures()
                               : cfg.GetSelectedTrusses());
      auto parseId = [&](const std::string &token, int &value) {
        if (token.empty()) {
          AppendMessage("[ERROR] Invalid selection id: empty token");
          return false;
        }
        auto begin = token.data();
        auto end = token.data() + token.size();
        auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end) {
          AppendMessage("[ERROR] Invalid selection id: " +
                        wxString::FromUTF8(token));
          return false;
        }
        return true;
      };
      auto addId = [&](int id) {
        std::string uid;
        if (fixtures) {
          for (const auto &[u, f] : scene.fixtures)
            if (f.fixtureId == id) {
              uid = u;
              break;
            }
        } else {
          for (const auto &[u, t] : scene.trusses)
            if (t.unitNumber == id) {
              uid = u;
              break;
            }
        }
        if (!uid.empty() &&
            std::find(current.begin(), current.end(), uid) == current.end())
          current.push_back(uid);
      };
      auto removeId = [&](int id) {
        auto it = current.begin();
        while (it != current.end()) {
          int fid = -1;
          if (fixtures) {
            auto fit = scene.fixtures.find(*it);
            if (fit != scene.fixtures.end())
              fid = fit->second.fixtureId;
          } else {
            auto fit = scene.trusses.find(*it);
            if (fit != scene.trusses.end())
              fid = fit->second.unitNumber;
          }
          if (fid == id)
            it = current.erase(it);
          else
            ++it;
        }
      };
      std::vector<std::string> normalized = NormalizeRangeTokens(tokens);
      char mode = '+';
      for (size_t i = 0; i < normalized.size();) {
        const std::string &tok = normalized[i];
        if (tok == "+" || tok == "-") {
          mode = tok[0];
          ++i;
          continue;
        }
        int a = 0;
        if (!parseId(tok, a))
          return;
        if (i + 1 < normalized.size() && normalized[i + 1] != "+" &&
            normalized[i + 1] != "-") {
          int b = 0;
          if (!parseId(normalized[i + 1], b))
            return;
          if (a > b)
            std::swap(a, b);
          for (int n = a; n <= b; ++n) {
            if (mode == '+')
              addId(n);
            else
              removeId(n);
          }
          i += 2;
        } else {
          if (mode == '+')
            addId(a);
          else
            removeId(a);
          ++i;
        }
      }
      if (fixtures) {
        cfg.SetSelectedFixtures(current);
        if (FixtureTablePanel::Instance())
          FixtureTablePanel::Instance()->SelectByUuid(current);
      } else {
        cfg.SetSelectedTrusses(current);
        if (TrussTablePanel::Instance())
          TrussTablePanel::Instance()->SelectByUuid(current);
      }
      if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->SetSelectedUuids(current);
      if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetSelectedFixtures(current);
        Viewer3DPanel::Instance()->Refresh();
      }
    };

    auto parsePivotToken = [&](const std::string &token,
                               std::array<float, 3> &pivotMm) {
      auto parts = split(token, ',');
      if (parts.size() != 3)
        return false;
      for (size_t idx = 0; idx < 3; ++idx) {
        std::string part = trim(parts[idx]);
        if (part.empty())
          return false;
        std::stringstream parser(part);
        float valueMeters = 0.0f;
        parser >> valueMeters;
        if (!parser || !parser.eof() || !std::isfinite(valueMeters))
          return false;
        pivotMm[idx] = valueMeters * 1000.0f;
      }
      return true;
    };

    auto computeSelectionBoundsCenterMm = [&]() -> std::optional<std::array<float, 3>> {
      auto &scene = cfg.GetScene();
      bool hasAny = false;
      std::array<float, 3> minCorner{};
      std::array<float, 3> maxCorner{};
      auto expandBounds = [&](const std::array<float, 3> &positionMm) {
        if (!hasAny) {
          minCorner = positionMm;
          maxCorner = positionMm;
          hasAny = true;
          return;
        }
        for (size_t axis = 0; axis < 3; ++axis) {
          minCorner[axis] = std::min(minCorner[axis], positionMm[axis]);
          maxCorner[axis] = std::max(maxCorner[axis], positionMm[axis]);
        }
      };
      const scene_grouping::ObjectSelection selection{
          .fixtures = cfg.GetSelectedFixtures(),
          .trusses = cfg.GetSelectedTrusses(),
          .supports = cfg.GetSelectedSupports(),
          .sceneObjects = cfg.GetSelectedSceneObjects()};
      for (const auto &target : scene_grouping::BuildInteractiveTransformTargets(scene, selection))
        expandBounds(scene_grouping::GetTargetWorldTransform(scene, target).o);
      if (!hasAny)
        return std::nullopt;
      return std::array<float, 3>{
          (minCorner[0] + maxCorner[0]) * 0.5f,
          (minCorner[1] + maxCorner[1]) * 0.5f,
          (minCorner[2] + maxCorner[2]) * 0.5f,
      };
    };

    auto buildEffectiveSelection = [&]() {
      return scene_grouping::ObjectSelection{.fixtures = cfg.GetSelectedFixtures(),
                                             .trusses = cfg.GetSelectedTrusses(),
                                             .supports = cfg.GetSelectedSupports(),
                                             .sceneObjects = cfg.GetSelectedSceneObjects()};
    };

    auto hasEffectiveTargets = [&]() {
      return !scene_grouping::BuildInteractiveTransformTargets(
                  cfg.GetScene(), buildEffectiveSelection())
                  .empty();
    };

    auto applyPosEffective = [&](MvrScene &scene, int axis,
                                 const std::vector<float> &vals,
                                 bool relative,
                                 transform_space::TransformSpace space =
                                     transform_space::TransformSpace::World) {
      if (vals.empty())
        return;
      const auto targets = scene_grouping::BuildInteractiveTransformTargets(
          scene, buildEffectiveSelection());
      const size_t n = targets.size();
      if (n == 0)
        return;
      const float start = vals[0] * 1000.0f;
      const float end = (vals.size() > 1 ? vals[1] : vals[0]) * 1000.0f;
      for (size_t i = 0; i < n; ++i) {
        const float value = (vals.size() > 1 && n > 1)
                                ? start + (end - start) * static_cast<float>(i) /
                                              static_cast<float>(n - 1)
                                : start;
        Matrix transform = scene_grouping::GetTargetWorldTransform(scene, targets[i]);
        if (relative) {
          std::array<float, 3> delta{0.0f, 0.0f, 0.0f};
          delta[axis] = value;
          transform = transform_space::ApplyIncrementalTranslation(transform, delta, space);
        } else {
          transform.o[axis] = value;
        }
        scene_grouping::SetTargetWorldTransform(scene, targets[i], transform);
      }
    };

    auto applyRotEffective = [&](MvrScene &scene, int axis,
                                 const std::vector<float> &vals,
                                 bool relative,
                                 transform_space::TransformSpace space =
                                     transform_space::TransformSpace::World) {
      if (vals.empty())
        return;
      const auto targets = scene_grouping::BuildInteractiveTransformTargets(
          scene, buildEffectiveSelection());
      const size_t n = targets.size();
      if (n == 0)
        return;
      const float start = vals[0];
      const float end = vals.size() > 1 ? vals[1] : vals[0];
      int eAxis = 0;
      switch (axis) {
      case 0: eAxis = 2; break;
      case 1: eAxis = 1; break;
      default: eAxis = 0; break;
      }
      for (size_t i = 0; i < n; ++i) {
        const float angle = (vals.size() > 1 && n > 1)
                                ? start + (end - start) * static_cast<float>(i) /
                                              static_cast<float>(n - 1)
                                : start;
        Matrix transform = scene_grouping::GetTargetWorldTransform(scene, targets[i]);
        Matrix rotated;
        if (relative) {
          Matrix delta = MatrixUtils::Identity();
          if (axis == 0)
            delta = MatrixUtils::EulerToMatrix(0.0f, 0.0f, angle);
          else if (axis == 1)
            delta = MatrixUtils::EulerToMatrix(0.0f, angle, 0.0f);
          else
            delta = MatrixUtils::EulerToMatrix(angle, 0.0f, 0.0f);
          rotated = transform_space::ApplyIncrementalRotation(transform, delta, space);
        } else {
          auto e = MatrixUtils::MatrixToEuler(transform);
          e[eAxis] = angle;
          rotated = MatrixUtils::ApplyRotationPreservingScale(
              transform, MatrixUtils::EulerToMatrix(e[0], e[1], e[2]), transform.o);
        }
        scene_grouping::SetTargetWorldTransform(scene, targets[i], rotated);
      }
    };

    auto rotateEffectiveAroundPivot = [&](MvrScene &scene, int axis,
                                          float angleDeg,
                                          const std::array<float, 3> &pivotMm,
                                          transform_space::TransformSpace space) {
      scene_grouping::RotateSelectionAroundPivot(scene,
                                                 buildEffectiveSelection(), axis,
                                                 angleDeg, pivotMm, space);
    };

    auto executeTransform = [&](const std::string &undoLabel,
                                const auto &operation) {
      MvrScene preview = cfg.GetScene();
      operation(preview);
      const auto targets = scene_grouping::BuildInteractiveTransformTargets(
          cfg.GetScene(), buildEffectiveSelection());
      bool changed = false;
      for (const auto &target : targets) {
        const Matrix before =
            scene_grouping::GetTargetWorldTransform(cfg.GetScene(), target);
        const Matrix after =
            scene_grouping::GetTargetWorldTransform(preview, target);
        for (const auto &pair : {std::pair{&before.u, &after.u},
                                 std::pair{&before.v, &after.v},
                                 std::pair{&before.w, &after.w},
                                 std::pair{&before.o, &after.o}}) {
          for (size_t component = 0; component < 3; ++component)
            changed = changed ||
                      std::fabs((*pair.first)[component] -
                                (*pair.second)[component]) > 0.0001f;
        }
      }
      if (!changed) {
        AppendMessage(_("[INFO] Transform is already at the requested value."));
        return false;
      }
      cfg.PushUndoState(undoLabel);
      operation(cfg.GetScene());
      return true;
    };



    auto parseSegment = [](const std::string &s) {
      return gui::console::ParseTransformCommandSegment(s);
    };

    auto refreshSelectionAfterTransform = [&]() {
      const auto selFixtures = cfg.GetSelectedFixtures();
      const auto selTrusses = cfg.GetSelectedTrusses();
      const auto selSupports = cfg.GetSelectedSupports();
      const auto selSceneObjects = cfg.GetSelectedSceneObjects();

      if (!selFixtures.empty() && FixtureTablePanel::Instance()) {
        FixtureTablePanel::Instance()->ReloadData();
        FixtureTablePanel::Instance()->SelectByUuid(selFixtures, false);
      }
      if (!selTrusses.empty() && TrussTablePanel::Instance()) {
        TrussTablePanel::Instance()->ReloadData();
        TrussTablePanel::Instance()->SelectByUuid(selTrusses, false);
      }
      if (!selSupports.empty() && HoistTablePanel::Instance()) {
        HoistTablePanel::Instance()->ReloadData();
        HoistTablePanel::Instance()->SelectByUuid(selSupports, false);
      }
      if (!selSceneObjects.empty() && SceneObjectTablePanel::Instance()) {
        SceneObjectTablePanel::Instance()->ReloadData();
        SceneObjectTablePanel::Instance()->SelectByUuid(selSceneObjects, false);
      }

      std::vector<std::string> mergedSelection;
      const auto appendSelection = [&](const std::vector<std::string> &source) {
        mergedSelection.insert(mergedSelection.end(), source.begin(), source.end());
      };
      appendSelection(selFixtures);
      appendSelection(selTrusses);
      appendSelection(selSupports);
      appendSelection(selSceneObjects);

      if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetSelectedFixtures(mergedSelection);
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
      }
      if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->SetSelectedUuids(mergedSelection);
    };

    auto isCmd = [](const std::string &tok, bool allowAxis,
                    bool allowRangeSeparator) {
      if (tok.empty())
        return false;
      std::string l = tok;
      std::transform(l.begin(), l.end(), l.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (allowRangeSeparator && (l == "t" || l == "thru"))
        return false;
      if (l == "clear" || l == "pos" || l == "rot" || l[0] == 'f' ||
          l[0] == 't')
        return true;
      if (allowAxis && (l == "x" || l == "y" || l == "z"))
        return true;
      return false;
    };

    std::stringstream ts(lower);
    std::vector<std::string> tokens;
    std::string tok;
    while (ts >> tok)
      tokens.push_back(tok);

    size_t i = 0;
    while (i < tokens.size()) {
      std::string word = tokens[i];
      std::string lw = word;
      std::transform(lw.begin(), lw.end(), lw.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      size_t j = i + 1;
      bool allowAxis = (lw != "pos" && lw != "rot");
      bool allowRangeSeparator =
          (lw == "pos" || lw == "rot" || lw == "x" || lw == "y" ||
           lw == "z" || (!lw.empty() && (lw[0] == 'f' || lw[0] == 't')));
      while (j < tokens.size() &&
             !isCmd(tokens[j], allowAxis, allowRangeSeparator))
        ++j;

      if (lw == "clear") {
        cfg.PushUndoState("cli clear");
        cfg.SetSelectedFixtures({});
        cfg.SetSelectedTrusses({});
        cfg.SetSelectedSceneObjects({});
        if (FixtureTablePanel::Instance())
          FixtureTablePanel::Instance()->SelectByUuid({});
        if (TrussTablePanel::Instance())
          TrussTablePanel::Instance()->SelectByUuid({});
        if (SceneObjectTablePanel::Instance())
          SceneObjectTablePanel::Instance()->SelectByUuid({});
        if (Viewer3DPanel::Instance()) {
          Viewer3DPanel::Instance()->SetSelectedFixtures({});
          Viewer3DPanel::Instance()->Refresh();
        }
        if (Viewer2DPanel::Instance())
          Viewer2DPanel::Instance()->SetSelectedUuids({});
      } else if (lw == "pos" || lw == "rot") {
        bool isRot = (lw == "rot");
        std::vector<std::string> segmentTokens;
        for (size_t k = i + 1; k < j; ++k)
          segmentTokens.push_back(tokens[k]);

        bool useGroupRotation = false;

        std::optional<std::array<float, 3>> explicitPivotMm;
        if (isRot && segmentTokens.size() > 1) {
          std::array<float, 3> parsedPivotMm{};
          if (parsePivotToken(segmentTokens.back(), parsedPivotMm)) {
            explicitPivotMm = parsedPivotMm;
            segmentTokens.pop_back();
          }
        }

        std::string rest;
        for (size_t k = 0; k < segmentTokens.size(); ++k) {
          if (k > 0)
            rest += ' ';
          rest += segmentTokens[k];
        }
        const auto selFixtures = cfg.GetSelectedFixtures();
        const auto selTrusses = cfg.GetSelectedTrusses();
        const auto selSupports = cfg.GetSelectedSupports();
        const auto selSceneObjects = cfg.GetSelectedSceneObjects();
        bool validTransform = false;
        bool appliedTransform = false;
        if (rest.find(',') != std::string::npos) {
          auto parts = split(rest, ',');
          std::vector<gui::console::TransformCommandSegment> segments;
          for (size_t idx = 0; idx < parts.size() && idx < 3; ++idx)
            segments.push_back(parseSegment(parts[idx]));
          validTransform = !segments.empty() &&
                           std::all_of(segments.begin(), segments.end(),
                                       [](const auto &segment) {
                                         return !segment.values.empty() &&
                                                segment.remainder.empty();
                                       });
          validTransform = validTransform && hasEffectiveTargets();
          if (validTransform) {
            appliedTransform = executeTransform(
                std::string("cli ") + lw, [&](MvrScene &targetScene) {
              for (size_t idx = 0; idx < segments.size(); ++idx) {
                const auto &segment = segments[idx];
                if (isRot) {
                  applyRotEffective(targetScene, static_cast<int>(idx),
                                    segment.values, segment.relative,
                                    segment.space);
                } else {
                  applyPosEffective(targetScene, static_cast<int>(idx),
                                    segment.values, segment.relative,
                                    segment.space);
                }
              }
            });
          }
        } else {
          std::stringstream ps(rest);
          std::string ax;
          ps >> ax;
          int axis = 0;
          const bool validAxis = ax == "x" || ax == "y" || ax == "z";
          if (ax == "y")
            axis = 1;
          else if (ax == "z")
            axis = 2;
          std::string valsStr;
          std::getline(ps, valsStr);
          valsStr = trim(valsStr);
          const auto segment = parseSegment(valsStr);
          useGroupRotation = segment.group;
          validTransform = validAxis && !segment.values.empty() &&
                           segment.remainder.empty() && hasEffectiveTargets();
          if (validTransform && isRot && useGroupRotation) {
            if (!segment.values.empty()) {
              const auto pivotMm =
                  explicitPivotMm.value_or(computeSelectionBoundsCenterMm().value_or(
                      std::array<float, 3>{0.0f, 0.0f, 0.0f}));
              const float angleDeg = segment.values[0];
              appliedTransform = executeTransform(
                  std::string("cli ") + lw, [&](MvrScene &targetScene) {
                    rotateEffectiveAroundPivot(targetScene, axis, angleDeg,
                                               pivotMm, segment.space);
                  });
            }
          } else if (validTransform && isRot) {
            appliedTransform = executeTransform(
                std::string("cli ") + lw, [&](MvrScene &targetScene) {
                  applyRotEffective(targetScene, axis, segment.values,
                                    segment.relative, segment.space);
                });
          } else if (validTransform) {
            appliedTransform = executeTransform(
                std::string("cli ") + lw, [&](MvrScene &targetScene) {
                  applyPosEffective(targetScene, axis, segment.values,
                                    segment.relative, segment.space);
                });
          }
        }
        if (validTransform) {
          if (appliedTransform)
            refreshSelectionAfterTransform();
        } else {
          AppendMessage(_("[ERROR] Invalid transform: provide a valid axis, finite numeric values, valid modifiers, and a non-empty selection."));
          return;
        }
      } else if (lw == "x" || lw == "y" || lw == "z") {
        std::string rest;
        for (size_t k = i + 1; k < j; ++k) {
          if (k > i + 1)
            rest += ' ';
          rest += tokens[k];
        }
        const auto selFixtures = cfg.GetSelectedFixtures();
        const auto selTrusses = cfg.GetSelectedTrusses();
        const auto selSupports = cfg.GetSelectedSupports();
        const auto selSceneObjects = cfg.GetSelectedSceneObjects();
        int axis = (lw == "x") ? 0 : (lw == "y" ? 1 : 2);
        const auto segment = parseSegment(rest);
        if (!segment.values.empty() && segment.remainder.empty() &&
            hasEffectiveTargets()) {
          const bool applied = executeTransform(
              "cli pos", [&](MvrScene &targetScene) {
                applyPosEffective(targetScene, axis, segment.values,
                                  segment.relative, segment.space);
              });
          if (applied)
            refreshSelectionAfterTransform();
        } else {
          AppendMessage(_("[ERROR] Invalid transform: provide finite numeric values, valid modifiers, and a non-empty selection."));
          return;
        }
      } else if (!lw.empty() && (std::isdigit(lw[0]) || lw[0] == '-' ||
                                 lw[0] == '+') &&
                 word.find(',') != std::string::npos) {
        auto parts = split(word, ',');
        std::vector<gui::console::TransformCommandSegment> segments;
        for (const auto &part : parts)
          segments.push_back(parseSegment(part));
        const bool validTriplet =
            segments.size() == 3 && hasEffectiveTargets() &&
            std::all_of(segments.begin(), segments.end(),
                        [](const auto &segment) {
                          return !segment.values.empty() &&
                                 segment.remainder.empty() && !segment.group;
                        });
        if (!validTriplet) {
          AppendMessage(_("[ERROR] Invalid transform triplet: provide three finite numeric components and a non-empty selection."));
          return;
        }
        const bool applied = executeTransform(
            "cli pos", [&](MvrScene &targetScene) {
              for (size_t idx = 0; idx < segments.size(); ++idx)
                applyPosEffective(targetScene, static_cast<int>(idx),
                                  segments[idx].values,
                                  segments[idx].relative,
                                  segments[idx].space);
            });
        if (applied)
          refreshSelectionAfterTransform();
      } else if (!lw.empty() && lw[0] == 'f') {
        std::vector<std::string> sub(tokens.begin() + i + 1,
                                     tokens.begin() + j);
        handleSelection(true, false, sub);
      } else if (!lw.empty() && lw[0] == 't') {
        std::vector<std::string> sub(tokens.begin() + i + 1,
                                     tokens.begin() + j);
        handleSelection(false, true, sub);
      } else {
        AppendMessage("[ERROR] Syntax error");
        return;
      }
      i = j;
    }

    AppendMessage("[INFO] OK");
  } catch (const std::exception &e) {
    AppendMessage("[ERROR] " + wxString::FromUTF8(e.what()));
  }
}
