#include "unit_label_utils.h"

namespace Units {

// Builds a display label by appending the unit suffix in parentheses.
std::string LabelWithUnit(const std::string &label, const std::string &unitSuffix) {
  return label + " (" + unitSuffix + ")";
}

// Formats a distance value together with the active display unit suffix.
std::string DistanceValueWithUnit(double valueMm, DistanceUnitSystem unitSystem,
                                  ValueFormatContext context) {
  return FormatDistanceFromMillimeters(valueMm, unitSystem, context) + " " +
         DistanceUnitSuffix(unitSystem);
}

} // namespace Units
