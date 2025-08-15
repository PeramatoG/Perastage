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
#pragma once

#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <vector>

// Simple panel to display log messages in a console-like view
class ConsolePanel : public wxPanel
{
public:
    explicit ConsolePanel(wxWindow* parent);

    // Append a message to the console
    void AppendMessage(const wxString& msg);

    // Access singleton instance
    static ConsolePanel* Instance();
    static void SetInstance(ConsolePanel* panel);

private:
    wxTextCtrl* m_textCtrl = nullptr;
    wxTextCtrl* m_inputCtrl = nullptr;
    bool m_autoScroll = true;
    std::vector<wxString> m_history;
    size_t m_historyIndex = 0;
    void OnScroll(wxScrollWinEvent& event);
    void OnCommandEnter(wxCommandEvent& event);
    void OnInputFocus(wxFocusEvent& event);
    void OnInputKillFocus(wxFocusEvent& event);
    void OnInputKeyDown(wxKeyEvent& event);
    void ProcessCommand(const wxString& cmd);
};
