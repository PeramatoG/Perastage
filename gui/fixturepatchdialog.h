#pragma once
#include <wx/wx.h>
#include "fixture.h"

class FixturePatchDialog : public wxDialog {
public:
    FixturePatchDialog(wxWindow* parent, const Fixture& fixture);
    int GetFixtureId() const;
    int GetUniverse() const;
    int GetChannel() const;
private:
    wxTextCtrl* idCtrl;
    wxTextCtrl* uniCtrl;
    wxTextCtrl* chCtrl;
};
