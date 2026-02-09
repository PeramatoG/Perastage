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
#include "fixturetablepanel.h"
#include "mainwindow.h"
#include "matrixutils.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <deque>
#include <exception>
#include <mutex>
#include <sstream>
#include <vector>


namespace {
std::mutex g_consoleQueueMutex;
std::deque<wxString> g_consoleQueue;
size_t g_droppedMessages = 0;

constexpr size_t kMaxConsoleMessageLength = 8 * 1024;
constexpr int kFlushIntervalMs = 50;
constexpr size_t kFlushBatchSize = 250;
constexpr size_t kMaxPendingMessages = 10000;

wxString TruncateConsoleMessage(const wxString &msg) {
  const wxString suffix = "... (truncated)";
  if (msg.length() <= kMaxConsoleMessageLength)
    return msg;
  size_t keepLength =
      kMaxConsoleMessageLength > suffix.length()
          ? kMaxConsoleMessageLength - suffix.length()
          : 0;
  return msg.Left(keepLength) + suffix;
}
} // namespace

ConsolePanel::ConsolePanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  m_textCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                              wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  m_textCtrl->SetBackgroundColour(*wxBLACK);
  m_textCtrl->SetForegroundColour(wxColour(200, 200, 200));
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
  const wxEventTypeTag<wxScrollWinEvent> scrollEvents[] = {
      wxEVT_SCROLLWIN_TOP,        wxEVT_SCROLLWIN_BOTTOM,
      wxEVT_SCROLLWIN_LINEUP,     wxEVT_SCROLLWIN_LINEDOWN,
      wxEVT_SCROLLWIN_PAGEUP,     wxEVT_SCROLLWIN_PAGEDOWN,
      wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE};
  for (const auto &evt : scrollEvents)
    m_textCtrl->Bind(evt, &ConsolePanel::OnScroll, this);
  sizer->Add(m_textCtrl, 1, wxEXPAND | wxALL, 5);
  sizer->Add(m_inputCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  SetSizer(sizer);

  Bind(wxEVT_TIMER, &ConsolePanel::OnFlushTimer, this, m_flushTimer.GetId());
  m_flushTimer.Start(kFlushIntervalMs);
}

ConsolePanel::~ConsolePanel() {
  if (m_flushTimer.IsRunning())
    m_flushTimer.Stop();
}

void ConsolePanel::EnqueueMessage(const wxString &msg) {
  std::lock_guard<std::mutex> lock(g_consoleQueueMutex);
  if (g_consoleQueue.size() >= kMaxPendingMessages) {
    const size_t overflow = g_consoleQueue.size() - kMaxPendingMessages + 1;
    g_consoleQueue.erase(g_consoleQueue.begin(),
                         g_consoleQueue.begin() + static_cast<std::ptrdiff_t>(overflow));
    g_droppedMessages += overflow;
  }
  g_consoleQueue.push_back(TruncateConsoleMessage(msg));
}

void ConsolePanel::AppendMessage(const wxString &msg) {
  EnqueueMessage(msg);
}

void ConsolePanel::OnFlushTimer(wxTimerEvent &event) {
  std::deque<wxString> batch;
  size_t dropped = 0;
  {
    std::lock_guard<std::mutex> lock(g_consoleQueueMutex);
    dropped = g_droppedMessages;
    g_droppedMessages = 0;
    size_t count = std::min(kFlushBatchSize, g_consoleQueue.size());
    for (size_t i = 0; i < count; ++i) {
      batch.push_back(std::move(g_consoleQueue.front()));
      g_consoleQueue.pop_front();
    }
  }

  if (!m_textCtrl) {
    event.Skip();
    return;
  }

  if (dropped > 0) {
    batch.push_front(
        wxString::Format("%zu mensajes omitidos por backlog de UI", dropped));
  }
  if (batch.empty()) {
    event.Skip();
    return;
  }

  std::vector<std::pair<wxString, size_t>> groups;
  for (const auto &rawMsg : batch) {
    const wxString msg = TruncateConsoleMessage(rawMsg);
    if (!groups.empty() && groups.back().first == msg) {
      groups.back().second++;
    } else {
      groups.push_back({msg, 1});
    }
  }

  const long currentPos = m_textCtrl->GetLastPosition();
  bool replaceLastLine = false;
  const long previousLineStart = m_lastLineStart;
  wxString chunk;
  size_t groupIndex = 0;

  if (!groups.empty() && groups.front().first == m_lastMessage) {
    m_repeatCount += groups.front().second;
    const wxString combined = wxString::Format("%s (repeated %zu times)",
                                               m_lastMessage, m_repeatCount);
    replaceLastLine = (m_lastLineStart < currentPos);
    chunk += combined + "\n";
    groupIndex = 1;
  }

  for (; groupIndex < groups.size(); ++groupIndex) {
    const auto &group = groups[groupIndex];
    m_lastMessage = group.first;
    m_repeatCount = group.second;
    m_lastLineStart = currentPos + chunk.length();
    if (group.second > 1) {
      chunk += wxString::Format("%s (repeated %zu times)", group.first,
                                group.second);
    } else {
      chunk += group.first;
    }
    chunk += "\n";
  }

  if (replaceLastLine)
    m_textCtrl->Remove(previousLineStart, currentPos);

  if (!chunk.empty()) {
    m_textCtrl->AppendText(chunk);
    if (m_autoScroll)
      m_textCtrl->ShowPosition(m_textCtrl->GetLastPosition());
  }

  event.Skip();
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

  AppendMessage(cmdWx);

  try {
    std::string lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    ConfigManager &cfg = ConfigManager::Get();

    auto handleSelection = [&](bool fixtures, bool clearSel,
                               const std::vector<std::string> &tokens) {
      auto &scene = cfg.GetScene();
      std::vector<std::string> current =
          clearSel ? std::vector<std::string>()
                   : (fixtures ? cfg.GetSelectedFixtures()
                               : cfg.GetSelectedTrusses());
      auto parseId = [&](const std::string &token, int &value) {
        if (token.empty()) {
          AppendMessage("Invalid selection id: empty token");
          return false;
        }
        auto begin = token.data();
        auto end = token.data() + token.size();
        auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end) {
          AppendMessage(wxString::Format("Invalid selection id: %s",
                                         wxString::FromUTF8(token).c_str()));
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

    auto applyPos = [&](const std::vector<std::string> &sel, bool fixtures,
                        int axis, const std::vector<float> &vals,
                        bool relative) {
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
        if (fixtures) {
          auto it = scene.fixtures.find(sel[i]);
          if (it != scene.fixtures.end()) {
            if (relative)
              it->second.transform.o[axis] += v;
            else
              it->second.transform.o[axis] = v;
          }
        } else {
          auto it = scene.trusses.find(sel[i]);
          if (it != scene.trusses.end()) {
            if (relative)
              it->second.transform.o[axis] += v;
            else
              it->second.transform.o[axis] = v;
          }
        }
      }
    };

    auto applyRot = [&](const std::vector<std::string> &sel, bool fixtures,
                        int axis, const std::vector<float> &vals,
                        bool relative) {
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
        if (fixtures) {
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
        } else {
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

    auto refreshSelectionAfterTransform =
        [&](const std::vector<std::string> &sel, bool fixtures) {
          if (fixtures) {
            if (FixtureTablePanel::Instance()) {
              FixtureTablePanel::Instance()->ReloadData();
              FixtureTablePanel::Instance()->SelectByUuid(sel);
            }
          } else {
            if (TrussTablePanel::Instance()) {
              TrussTablePanel::Instance()->ReloadData();
              TrussTablePanel::Instance()->SelectByUuid(sel);
            }
          }
          if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->SetSelectedFixtures(sel);
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
          }
          if (Viewer2DPanel::Instance())
            Viewer2DPanel::Instance()->SetSelectedUuids(sel);
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
          (!lw.empty() && (lw[0] == 'f' || lw[0] == 't'));
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
        std::vector<std::string> selFixtures = cfg.GetSelectedFixtures();
        std::vector<std::string> selTrusses = cfg.GetSelectedTrusses();
        bool fixtures = !selFixtures.empty();
        std::vector<std::string> &sel = fixtures ? selFixtures : selTrusses;
        if (rest.find(',') != std::string::npos) {
          auto parts = split(rest, ',');
          for (size_t idx = 0; idx < parts.size() && idx < 3; ++idx) {
            bool rel = false;
            auto vals = parseVals(parts[idx], rel);
            if (isRot)
              applyRot(sel, fixtures, (int)idx, vals, rel);
            else
              applyPos(sel, fixtures, (int)idx, vals, rel);
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
          if (isRot)
            applyRot(sel, fixtures, axis, vals, rel);
          else
            applyPos(sel, fixtures, axis, vals, rel);
        }
        refreshSelectionAfterTransform(sel, fixtures);
      } else if (lw == "x" || lw == "y" || lw == "z") {
        cfg.PushUndoState("cli pos");
        std::string rest;
        for (size_t k = i + 1; k < j; ++k) {
          if (k > i + 1)
            rest += ' ';
          rest += tokens[k];
        }
        std::vector<std::string> selFixtures = cfg.GetSelectedFixtures();
        std::vector<std::string> selTrusses = cfg.GetSelectedTrusses();
        bool fixtures = !selFixtures.empty();
        std::vector<std::string> &sel = fixtures ? selFixtures : selTrusses;
        int axis = (lw == "x") ? 0 : (lw == "y" ? 1 : 2);
        bool rel = false;
        auto vals = parseVals(rest, rel);
        applyPos(sel, fixtures, axis, vals, rel);
        refreshSelectionAfterTransform(sel, fixtures);
      } else if (!lw.empty() && (std::isdigit(lw[0]) || lw[0] == '-' ||
                                 lw[0] == '+') &&
                 word.find(',') != std::string::npos) {
        cfg.PushUndoState("cli pos");
        std::vector<std::string> selFixtures = cfg.GetSelectedFixtures();
        std::vector<std::string> selTrusses = cfg.GetSelectedTrusses();
        bool fixtures = !selFixtures.empty();
        std::vector<std::string> &sel = fixtures ? selFixtures : selTrusses;
        auto parts = split(word, ',');
        for (size_t idx = 0; idx < parts.size() && idx < 3; ++idx) {
          bool rel = false;
          auto vals = parseVals(parts[idx], rel);
          applyPos(sel, fixtures, (int)idx, vals, rel);
        }
        refreshSelectionAfterTransform(sel, fixtures);
      } else if (!lw.empty() && lw[0] == 'f') {
        std::vector<std::string> sub(tokens.begin() + i + 1,
                                     tokens.begin() + j);
        handleSelection(true, false, sub);
      } else if (!lw.empty() && lw[0] == 't') {
        std::vector<std::string> sub(tokens.begin() + i + 1,
                                     tokens.begin() + j);
        handleSelection(false, true, sub);
      } else {
        AppendMessage("Syntax error");
        return;
      }
      i = j;
    }

    AppendMessage("OK");
  } catch (const std::exception &e) {
    AppendMessage("Error: " + wxString::FromUTF8(e.what()));
  }
}
