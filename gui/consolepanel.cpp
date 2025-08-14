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
