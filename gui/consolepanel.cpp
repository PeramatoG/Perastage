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
#include "configmanager.h"
#include "guiconfigservices.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "mainwindow.h"
#include "matrixutils.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <fstream>
#include <charconv>
#include <cctype>
#include <exception>
#include <sstream>
#include <vector>
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

wxString BuildConsoleHelpContent() {
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

  return "Console commands:\n"
         "- clear\n"
         "- f ...\n"
         "- t ...\n"
         "- pos x|y|z <values>\n"
         "- pos <x>,<y>,<z>\n"
         "- x|y|z <values>\n"
         "- rot x|y|z <values>\n"
         "Examples:\n"
         "- f 1-5\n"
         "- pos x 1 4\n"
         "- rot z -- 10";
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
      "Show available console commands and examples.");
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
  wxDialog helpDialog(this, wxID_ANY, "Console commands", wxDefaultPosition,
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

    enum class TransformTarget { Fixtures, Trusses, Supports, SceneObjects };

    auto applyPos = [&](const std::vector<std::string> &sel,
                        TransformTarget target, int axis,
                        const std::vector<float> &vals, bool relative) {
      if (sel.empty() || vals.empty())
        return;
      auto &scene = cfg.GetScene();
      size_t n = sel.size();
      float start = vals[0];
      float end = vals.size() > 1 ? vals[1] : vals[0];
      for (size_t i = 0; i < n; i++) {
        float v = (vals.size() > 1 && n > 1)
                      ? start + (end - start) * (float)i / (float)(n - 1)
                      : start;
        v *= 1000.0f;
        if (target == TransformTarget::Fixtures) {
          auto it = scene.fixtures.find(sel[i]);
          if (it != scene.fixtures.end()) {
            if (relative)
              it->second.transform.o[axis] += v;
            else
              it->second.transform.o[axis] = v;
          }
        } else if (target == TransformTarget::Trusses) {
          auto it = scene.trusses.find(sel[i]);
          if (it != scene.trusses.end()) {
            if (relative)
              it->second.transform.o[axis] += v;
            else
              it->second.transform.o[axis] = v;
          }
        } else if (target == TransformTarget::Supports) {
          auto it = scene.supports.find(sel[i]);
          if (it != scene.supports.end()) {
            if (relative)
              it->second.transform.o[axis] += v;
            else
              it->second.transform.o[axis] = v;
          }
        } else {
          auto it = scene.sceneObjects.find(sel[i]);
          if (it != scene.sceneObjects.end()) {
            if (relative)
              it->second.transform.o[axis] += v;
            else
              it->second.transform.o[axis] = v;
          }
        }
      }
    };

    auto applyRot = [&](const std::vector<std::string> &sel,
                        TransformTarget target, int axis,
                        const std::vector<float> &vals, bool relative) {
      if (sel.empty() || vals.empty())
        return;
      auto &scene = cfg.GetScene();
      size_t n = sel.size();
      float start = vals[0];
      float end = vals.size() > 1 ? vals[1] : vals[0];
      for (size_t i = 0; i < n; i++) {
        float ang = (vals.size() > 1 && n > 1)
                        ? start + (end - start) * (float)i / (float)(n - 1)
                        : start;
        int eAxis = 0;
        switch (axis) {
        case 0: eAxis = 2; break; // roll (X)
        case 1: eAxis = 1; break; // pitch (Y)
        default: eAxis = 0; break; // yaw (Z)
        }
        if (target == TransformTarget::Fixtures) {
          auto it = scene.fixtures.find(sel[i]);
          if (it != scene.fixtures.end()) {
            auto e = MatrixUtils::MatrixToEuler(it->second.transform);
            if (relative)
              e[eAxis] += ang;
            else
              e[eAxis] = ang;
            Matrix m = MatrixUtils::EulerToMatrix(e[0], e[1], e[2]);
            m.o = it->second.transform.o;
            it->second.transform = m;
          }
        } else if (target == TransformTarget::Trusses) {
          auto it = scene.trusses.find(sel[i]);
          if (it != scene.trusses.end()) {
            auto e = MatrixUtils::MatrixToEuler(it->second.transform);
            if (relative)
              e[eAxis] += ang;
            else
              e[eAxis] = ang;
            Matrix m = MatrixUtils::EulerToMatrix(e[0], e[1], e[2]);
            m.o = it->second.transform.o;
            it->second.transform = m;
          }
        } else if (target == TransformTarget::Supports) {
          auto it = scene.supports.find(sel[i]);
          if (it != scene.supports.end()) {
            auto e = MatrixUtils::MatrixToEuler(it->second.transform);
            if (relative)
              e[eAxis] += ang;
            else
              e[eAxis] = ang;
            Matrix m = MatrixUtils::EulerToMatrix(e[0], e[1], e[2]);
            m.o = it->second.transform.o;
            it->second.transform = m;
          }
        } else {
          auto it = scene.sceneObjects.find(sel[i]);
          if (it != scene.sceneObjects.end()) {
            auto e = MatrixUtils::MatrixToEuler(it->second.transform);
            if (relative)
              e[eAxis] += ang;
            else
              e[eAxis] = ang;
            Matrix m = MatrixUtils::EulerToMatrix(e[0], e[1], e[2]);
            m.o = it->second.transform.o;
            it->second.transform = m;
          }
        }
      }
    };

    auto parseVals = [&](const std::string &s, bool &relative) {
      relative = false;
      std::string str = trim(s);
      float sign = 1.0f;
      if (str.rfind("++", 0) == 0) {
        relative = true;
        str = trim(str.substr(2));
      } else if (str.rfind("--", 0) == 0) {
        relative = true;
        sign = -1.0f;
        str = trim(str.substr(2));
      }
      std::stringstream ss(str);
      std::vector<float> vals;
      float v;
      while (ss >> v)
        vals.push_back(sign * v);
      return vals;
    };

    auto refreshSelectionAfterTransform = [&]() {
          const auto selFixtures = cfg.GetSelectedFixtures();
          const auto selTrusses = cfg.GetSelectedTrusses();
          const auto selSupports = cfg.GetSelectedSupports();
          const auto selSceneObjects = cfg.GetSelectedSceneObjects();

          if (!selFixtures.empty() && FixtureTablePanel::Instance()) {
            FixtureTablePanel::Instance()->ReloadData();
            FixtureTablePanel::Instance()->SelectByUuid(selFixtures);
          }
          if (!selTrusses.empty() && TrussTablePanel::Instance()) {
            TrussTablePanel::Instance()->ReloadData();
            TrussTablePanel::Instance()->SelectByUuid(selTrusses);
          }
          if (!selSupports.empty() && HoistTablePanel::Instance()) {
            HoistTablePanel::Instance()->ReloadData();
            HoistTablePanel::Instance()->SelectByUuid(selSupports);
          }
          if (!selSceneObjects.empty() && SceneObjectTablePanel::Instance()) {
            SceneObjectTablePanel::Instance()->ReloadData();
            SceneObjectTablePanel::Instance()->SelectByUuid(selSceneObjects);
          }

          std::vector<std::string> primarySelection = selFixtures;
          if (primarySelection.empty())
            primarySelection = selTrusses;
          if (primarySelection.empty())
            primarySelection = selSupports;
          if (primarySelection.empty())
            primarySelection = selSceneObjects;

          if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->SetSelectedFixtures(primarySelection);
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
          }
          if (Viewer2DPanel::Instance())
            Viewer2DPanel::Instance()->SetSelectedUuids(primarySelection);
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
        cfg.PushUndoState(std::string("cli ") + lw);
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
        if (rest.find(',') != std::string::npos) {
          auto parts = split(rest, ',');
          for (size_t idx = 0; idx < parts.size() && idx < 3; ++idx) {
            bool rel = false;
            auto vals = parseVals(parts[idx], rel);
            if (isRot) {
              applyRot(selFixtures, TransformTarget::Fixtures, (int)idx, vals,
                       rel);
              applyRot(selTrusses, TransformTarget::Trusses, (int)idx, vals,
                       rel);
              applyRot(selSupports, TransformTarget::Supports, (int)idx, vals,
                       rel);
              applyRot(selSceneObjects, TransformTarget::SceneObjects,
                       (int)idx, vals, rel);
            } else {
              applyPos(selFixtures, TransformTarget::Fixtures, (int)idx, vals,
                       rel);
              applyPos(selTrusses, TransformTarget::Trusses, (int)idx, vals,
                       rel);
              applyPos(selSupports, TransformTarget::Supports, (int)idx, vals,
                       rel);
              applyPos(selSceneObjects, TransformTarget::SceneObjects,
                       (int)idx, vals, rel);
            }
          }
        } else {
          std::stringstream ps(rest);
          std::string ax;
          ps >> ax;
          int axis = 0;
          if (!ax.empty()) {
            char c = ax[0];
            if (c == 'x')
              axis = 0;
            else if (c == 'y')
              axis = 1;
            else
              axis = 2;
          }
          std::string valsStr;
          std::getline(ps, valsStr);
          valsStr = trim(valsStr);
          bool rel = false;
          auto vals = parseVals(valsStr, rel);
          if (isRot) {
            applyRot(selFixtures, TransformTarget::Fixtures, axis, vals, rel);
            applyRot(selTrusses, TransformTarget::Trusses, axis, vals, rel);
            applyRot(selSupports, TransformTarget::Supports, axis, vals, rel);
            applyRot(selSceneObjects, TransformTarget::SceneObjects, axis, vals,
                     rel);
          } else {
            applyPos(selFixtures, TransformTarget::Fixtures, axis, vals, rel);
            applyPos(selTrusses, TransformTarget::Trusses, axis, vals, rel);
            applyPos(selSupports, TransformTarget::Supports, axis, vals, rel);
            applyPos(selSceneObjects, TransformTarget::SceneObjects, axis, vals,
                     rel);
          }
        }
        refreshSelectionAfterTransform();
      } else if (lw == "x" || lw == "y" || lw == "z") {
        cfg.PushUndoState("cli pos");
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
        bool rel = false;
        auto vals = parseVals(rest, rel);
        applyPos(selFixtures, TransformTarget::Fixtures, axis, vals, rel);
        applyPos(selTrusses, TransformTarget::Trusses, axis, vals, rel);
        applyPos(selSupports, TransformTarget::Supports, axis, vals, rel);
        applyPos(selSceneObjects, TransformTarget::SceneObjects, axis, vals,
                 rel);
        refreshSelectionAfterTransform();
      } else if (!lw.empty() && (std::isdigit(lw[0]) || lw[0] == '-' ||
                                 lw[0] == '+') &&
                 word.find(',') != std::string::npos) {
        cfg.PushUndoState("cli pos");
        const auto selFixtures = cfg.GetSelectedFixtures();
        const auto selTrusses = cfg.GetSelectedTrusses();
        const auto selSupports = cfg.GetSelectedSupports();
        const auto selSceneObjects = cfg.GetSelectedSceneObjects();
        auto parts = split(word, ',');
        for (size_t idx = 0; idx < parts.size() && idx < 3; ++idx) {
          bool rel = false;
          auto vals = parseVals(parts[idx], rel);
          applyPos(selFixtures, TransformTarget::Fixtures, (int)idx, vals, rel);
          applyPos(selTrusses, TransformTarget::Trusses, (int)idx, vals, rel);
          applyPos(selSupports, TransformTarget::Supports, (int)idx, vals, rel);
          applyPos(selSceneObjects, TransformTarget::SceneObjects, (int)idx,
                   vals, rel);
        }
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
