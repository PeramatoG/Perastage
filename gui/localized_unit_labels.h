#pragma once

#include <wx/string.h>

namespace ui {

wxString LocalizedLabelWithUnit(const wxString &sourceLabel,
                                const wxString &unitSuffix);
wxString LocalizedLabelWithUnitColon(const wxString &sourceLabel,
                                     const wxString &unitSuffix);

} // namespace ui
