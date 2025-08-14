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
#include "mainwindow.h"
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "viewer3dpanel.h"
#include "matrixutils.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

ConsolePanel::ConsolePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_textCtrl = new wxTextCtrl(this, wxID_ANY, "",
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY);
    m_textCtrl->SetBackgroundColour(*wxBLACK);
    m_textCtrl->SetForegroundColour(wxColour(200, 200, 200));
    wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_textCtrl->SetFont(font);
    m_inputCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                 wxDefaultSize, wxTE_PROCESS_ENTER);
    m_inputCtrl->SetFont(font);
    m_inputCtrl->Bind(wxEVT_TEXT_ENTER, &ConsolePanel::OnCommandEnter, this);
    m_inputCtrl->Bind(wxEVT_SET_FOCUS, &ConsolePanel::OnInputFocus, this);
    m_inputCtrl->Bind(wxEVT_KILL_FOCUS, &ConsolePanel::OnInputKillFocus, this);
    m_inputCtrl->Bind(wxEVT_KEY_DOWN, &ConsolePanel::OnInputKeyDown, this);
    const wxEventTypeTag<wxScrollWinEvent> scrollEvents[] = {
        wxEVT_SCROLLWIN_TOP,
        wxEVT_SCROLLWIN_BOTTOM,
        wxEVT_SCROLLWIN_LINEUP,
        wxEVT_SCROLLWIN_LINEDOWN,
        wxEVT_SCROLLWIN_PAGEUP,
        wxEVT_SCROLLWIN_PAGEDOWN,
        wxEVT_SCROLLWIN_THUMBTRACK,
        wxEVT_SCROLLWIN_THUMBRELEASE
    };
    for (const auto& evt : scrollEvents)
        m_textCtrl->Bind(evt, &ConsolePanel::OnScroll, this);
    sizer->Add(m_textCtrl, 1, wxEXPAND | wxALL, 5);
    sizer->Add(m_inputCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    SetSizer(sizer);
}

void ConsolePanel::AppendMessage(const wxString& msg)
{
    if (!m_textCtrl) return;
    m_textCtrl->AppendText(msg + "\n");
    if (m_autoScroll)
        m_textCtrl->ShowPosition(m_textCtrl->GetLastPosition());
}

static ConsolePanel* s_instance = nullptr;

ConsolePanel* ConsolePanel::Instance()
{
    return s_instance;
}

void ConsolePanel::SetInstance(ConsolePanel* panel)
{
    s_instance = panel;
}

void ConsolePanel::OnScroll(wxScrollWinEvent& event)
{
    if (!m_textCtrl) {
        event.Skip();
        return;
    }
    int maxPos = m_textCtrl->GetScrollRange(wxVERTICAL) - m_textCtrl->GetScrollThumb(wxVERTICAL);
    int pos = event.GetPosition();
    m_autoScroll = (pos >= maxPos);
    event.Skip();
}

// --- Input handling ---

void ConsolePanel::OnCommandEnter(wxCommandEvent& event)
{
    wxString cmd = m_inputCtrl ? m_inputCtrl->GetValue() : wxString();
    if (m_inputCtrl)
        m_inputCtrl->Clear();
    ProcessCommand(cmd);
}

void ConsolePanel::OnInputFocus(wxFocusEvent& event)
{
    if (MainWindow::Instance())
        MainWindow::Instance()->EnableShortcuts(false);
    event.Skip();
}

void ConsolePanel::OnInputKillFocus(wxFocusEvent& event)
{
    if (MainWindow::Instance())
        MainWindow::Instance()->EnableShortcuts(true);
    event.Skip();
}

void ConsolePanel::OnInputKeyDown(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_ESCAPE) {
        if (MainWindow::Instance())
            MainWindow::Instance()->EnableShortcuts(true);
        m_inputCtrl->Clear();
        if (m_textCtrl)
            m_textCtrl->SetFocus();
        return;
    }
    event.Skip();
}

static std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return std::string();
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        parts.push_back(trim(item));
    return parts;
}

void ConsolePanel::ProcessCommand(const wxString& cmdWx)
{
    std::string cmd = std::string(cmdWx.ToUTF8());
    cmd = trim(cmd);
    if (cmd.empty())
        return;
    std::string lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

    ConfigManager& cfg = ConfigManager::Get();

    if (lower.rfind("clear", 0) == 0) {
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
        if (Viewer3DPanel::Instance())
            Viewer3DPanel::Instance()->SetSelectedFixtures({});
        if (Viewer3DPanel::Instance())
            Viewer3DPanel::Instance()->Refresh();
        return;
    }

    auto handleSelection = [&](bool fixtures, bool clearSel, const std::vector<std::string>& tokens){
        auto& scene = cfg.GetScene();
        std::vector<std::string> current = clearSel ? std::vector<std::string>() : (fixtures ? cfg.GetSelectedFixtures() : cfg.GetSelectedTrusses());
        auto addId = [&](int id){
            std::string uid;
            if (fixtures) {
                for (const auto& [u,f] : scene.fixtures) if (f.fixtureId == id) { uid = u; break; }
            } else {
                for (const auto& [u,t] : scene.trusses) if (t.unitNumber == id) { uid = u; break; }
            }
            if (!uid.empty() && std::find(current.begin(), current.end(), uid) == current.end())
                current.push_back(uid);
        };
        auto removeId = [&](int id){
            auto it = current.begin();
            while (it != current.end()) {
                const auto& map = fixtures ? scene.fixtures : scene.trusses;
                auto fit = map.find(*it);
                int fid = -1;
                if (fit != map.end())
                    fid = fixtures ? fit->second.fixtureId : fit->second.unitNumber;
                if (fid == id)
                    it = current.erase(it);
                else
                    ++it;
            }
        };
        char mode = '+';
        for (size_t i = 0; i < tokens.size();) {
            const std::string& tok = tokens[i];
            if (tok == "+" || tok == "-") {
                mode = tok[0];
                ++i;
                continue;
            }
            int a = std::stoi(tok);
            if (i + 1 < tokens.size() && tokens[i+1] != "+" && tokens[i+1] != "-") {
                int b = std::stoi(tokens[i+1]);
                if (a > b) std::swap(a,b);
                for (int n=a; n<=b; ++n) {
                    if (mode=='+') addId(n); else removeId(n);
                }
                i += 2;
            } else {
                if (mode=='+') addId(a); else removeId(a);
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
        if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->SetSelectedFixtures(current);
            Viewer3DPanel::Instance()->Refresh();
        }
    };

    auto applyPos = [&](const std::vector<std::string>& sel, bool fixtures, int axis, const std::vector<float>& vals){
        if (sel.empty() || vals.empty()) return;
        auto& scene = cfg.GetScene();
        size_t n = sel.size();
        float start = vals[0];
        float end = vals.size() > 1 ? vals[1] : vals[0];
        for (size_t i=0;i<n;i++) {
            float v = (vals.size()>1 && n>1) ? start + (end-start)*(float)i/(float)(n-1) : start;
            v *= 1000.0f;
            if (fixtures) {
                auto it = scene.fixtures.find(sel[i]);
                if (it!=scene.fixtures.end()) it->second.transform.o[axis] = v;
            } else {
                auto it = scene.trusses.find(sel[i]);
                if (it!=scene.trusses.end()) it->second.transform.o[axis] = v;
            }
        }
    };

    auto applyRot = [&](const std::vector<std::string>& sel, bool fixtures, int axis, const std::vector<float>& vals){
        if (sel.empty() || vals.empty()) return;
        auto& scene = cfg.GetScene();
        size_t n = sel.size();
        float start = vals[0];
        float end = vals.size() > 1 ? vals[1] : vals[0];
        for (size_t i=0;i<n;i++) {
            float ang = (vals.size()>1 && n>1) ? start + (end-start)*(float)i/(float)(n-1) : start;
            if (fixtures) {
                auto it = scene.fixtures.find(sel[i]);
                if (it!=scene.fixtures.end()) {
                    auto e = MatrixUtils::MatrixToEuler(it->second.transform);
                    e[axis] = ang;
                    Matrix m = MatrixUtils::EulerToMatrix(e[0], e[1], e[2]);
                    m.o = it->second.transform.o;
                    it->second.transform = m;
                }
            } else {
                auto it = scene.trusses.find(sel[i]);
                if (it!=scene.trusses.end()) {
                    auto e = MatrixUtils::MatrixToEuler(it->second.transform);
                    e[axis] = ang;
                    Matrix m = MatrixUtils::EulerToMatrix(e[0], e[1], e[2]);
                    m.o = it->second.transform.o;
                    it->second.transform = m;
                }
            }
        }
    };

    std::stringstream ss(lower);
    std::string first;
    ss >> first;
    if (first == "pos" || first == "rot") {
        bool isRot = (first == "rot");
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);
        std::vector<std::string> selFixtures = cfg.GetSelectedFixtures();
        std::vector<std::string> selTrusses = cfg.GetSelectedTrusses();
        bool fixtures = !selFixtures.empty();
        std::vector<std::string>& sel = fixtures ? selFixtures : selTrusses;
        if (rest.find(',') != std::string::npos) {
            auto parts = split(rest, ',');
            for (size_t i=0; i<parts.size() && i<3; ++i) {
                std::stringstream ps(parts[i]);
                std::vector<float> vals; float v; while (ps >> v) vals.push_back(v);
                if (isRot) applyRot(sel, fixtures, (int)i, vals); else applyPos(sel, fixtures, (int)i, vals);
            }
        } else {
            std::stringstream ps(rest);
            std::string ax; ps >> ax;
            int axis = 0;
            if (!ax.empty()) {
                char c = ax[0];
                if (c=='x') axis = 0; else if (c=='y') axis = 1; else axis = 2;
            }
            std::vector<float> vals; float v; while (ps >> v) vals.push_back(v);
            if (isRot) applyRot(sel, fixtures, axis, vals); else applyPos(sel, fixtures, axis, vals);
        }
        if (fixtures) {
            if (FixtureTablePanel::Instance()) FixtureTablePanel::Instance()->Refresh();
        } else {
            if (TrussTablePanel::Instance()) TrussTablePanel::Instance()->Refresh();
        }
        if (Viewer3DPanel::Instance()) Viewer3DPanel::Instance()->Refresh();
        return;
    }

    // Selection command
    {
        std::stringstream ts(lower);
        std::string cmdWord;
        ts >> cmdWord;
        std::vector<std::string> tokens;
        std::string t;
        while (ts >> t) tokens.push_back(t);
        if (!cmdWord.empty()) {
            if (cmdWord[0] == 'f') {
                handleSelection(true, false, tokens);
                return;
            }
            if (cmdWord[0] == 't') {
                handleSelection(false, true, tokens);
                return;
            }
        }
    }
}
