#include "localized_unit_labels.h"

#include <wx/intl.h>

namespace ui {

// Builds a localized display label by appending an untranslated unit suffix.
wxString LocalizedLabelWithUnit(const wxString &sourceLabel,
                                const wxString &unitSuffix) {
  return wxGetTranslation(sourceLabel) + " (" + unitSuffix + ")";
}

// Builds a localized display label with unit suffix and trailing colon.
wxString LocalizedLabelWithUnitColon(const wxString &sourceLabel,
                                     const wxString &unitSuffix) {
  return LocalizedLabelWithUnit(sourceLabel, unitSuffix) + ":";
}

} // namespace ui
