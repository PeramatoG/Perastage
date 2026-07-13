#pragma once

#include <wx/string.h>

namespace ui {

wxString LocalizedLabelWithUnit(const wxString &translatedLabel,
                                const wxString &unitSuffix);
wxString LocalizedLabelWithUnitColon(const wxString &translatedLabel,
                                     const wxString &unitSuffix);

} // namespace ui
