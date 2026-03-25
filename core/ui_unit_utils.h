#pragma once

#include "units/units.h"

namespace UiUnitUtils {

using DistanceUnitSystem = Units::DistanceUnitSystem;
using WeightUnitSystem = Units::WeightUnitSystem;
using ValueFormatContext = Units::ValueFormatContext;

using Units::DistanceDisplayToMillimeters;
using Units::DistanceMillimetersToDisplay;
using Units::DistanceUnitSuffix;
using Units::FormatDistanceFromMillimeters;
using Units::FormatWeightFromKilograms;
using Units::NearlyEqualDistanceMillimeters;
using Units::NearlyEqualWeightKilograms;
using Units::ParseDistanceToMillimeters;
using Units::ParseDistanceUnitSystem;
using Units::ParseWeightToKilograms;
using Units::ParseWeightUnitSystem;
using Units::WeightDisplayToKilograms;
using Units::WeightKilogramsToDisplay;
using Units::WeightUnitSuffix;

} // namespace UiUnitUtils
