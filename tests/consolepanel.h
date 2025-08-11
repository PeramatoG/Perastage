#pragma once
#include <wx/string.h>
class ConsolePanel {
public:
    static ConsolePanel* Instance();
    void AppendMessage(const wxString&);
};
