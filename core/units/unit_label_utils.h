#pragma once

#include "units.h"

#include <string>

namespace Units {

std::string LabelWithUnit(const std::string &label, const std::string &unitSuffix);
std::string DistanceValueWithUnit(double valueMm, DistanceUnitSystem unitSystem,
                                  ValueFormatContext context);

} // namespace Units
