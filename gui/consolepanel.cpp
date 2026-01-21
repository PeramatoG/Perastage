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
#include <exception>
#include <sstream>
#include <vector>

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
}

void ConsolePanel::AppendMessage(const wxString &msg) {
  if (!m_textCtrl)
    return;

  if (msg == m_lastMessage) {
    m_repeatCount++;
    wxString combined = wxString::Format("%s (repeated %zu times)", msg,
                                        m_repeatCount);
    long endPos = m_textCtrl->GetLastPosition();
    if (m_lastLineStart < endPos)
      m_textCtrl->Remove(m_lastLineStart, endPos);
    m_textCtrl->AppendText(combined + "\n");
  } else {
    m_lastMessage = msg;
    m_repeatCount = 1;
    m_lastLineStart = m_textCtrl->GetLastPosition();
    m_textCtrl->AppendText(msg + "\n");
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
          AppendMessage(
              wxString::Format("Invalid selection id: %s",
                               wxString::FromUTF8(token)));
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
        if (fixtures) {
          if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->ReloadData();
        } else {
          if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->ReloadData();
        }
        if (Viewer3DPanel::Instance()) {
          Viewer3DPanel::Instance()->UpdateScene();
          Viewer3DPanel::Instance()->Refresh();
        }
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
        if (fixtures) {
          if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->ReloadData();
        } else {
          if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->ReloadData();
        }
        if (Viewer3DPanel::Instance()) {
          Viewer3DPanel::Instance()->UpdateScene();
          Viewer3DPanel::Instance()->Refresh();
        }
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
        if (fixtures) {
          if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->ReloadData();
        } else {
          if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->ReloadData();
        }
        if (Viewer3DPanel::Instance()) {
          Viewer3DPanel::Instance()->UpdateScene();
          Viewer3DPanel::Instance()->Refresh();
        }
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
    AppendMessage(wxString::Format("Error: %s", e.what()));
  }
}
