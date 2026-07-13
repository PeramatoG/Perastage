#include "localized_unit_labels.h"

namespace ui {

// Builds a localized display label by appending an untranslated unit suffix.
wxString LocalizedLabelWithUnit(const wxString &translatedLabel,
                                const wxString &unitSuffix) {
  return translatedLabel + " (" + unitSuffix + ")";
}

// Builds a localized display label with unit suffix and trailing colon.
wxString LocalizedLabelWithUnitColon(const wxString &translatedLabel,
                                     const wxString &unitSuffix) {
  return LocalizedLabelWithUnit(translatedLabel, unitSuffix) + ":";
}

} // namespace ui
