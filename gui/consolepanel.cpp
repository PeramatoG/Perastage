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
    m_textCtrl->Bind(wxEVT_SCROLLWIN, &ConsolePanel::OnScroll, this);
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
