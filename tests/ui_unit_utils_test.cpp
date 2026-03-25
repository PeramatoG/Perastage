#include "ui_unit_utils.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

void AssertImperialDistanceRoundTrip() {
  constexpr double kInputFeet = 12.345;
  const auto parsedMm = UiUnitUtils::ParseDistanceToMillimeters(
      "12.345", UiUnitUtils::DistanceUnitSystem::Imperial);
  assert(parsedMm.has_value());

  const std::string printedFeet = UiUnitUtils::FormatDistanceFromMillimeters(
      *parsedMm, UiUnitUtils::DistanceUnitSystem::Imperial,
      UiUnitUtils::ValueFormatContext::Table);
  assert(printedFeet == "12.345");

  const auto reparsedMm = UiUnitUtils::ParseDistanceToMillimeters(
      printedFeet, UiUnitUtils::DistanceUnitSystem::Imperial);
  assert(reparsedMm.has_value());

  assert(UiUnitUtils::NearlyEqualDistanceMillimeters(*parsedMm, *reparsedMm,
                                                     0.001));

  const double driftFeet =
      std::abs(((*reparsedMm - *parsedMm) / 304.8));
  assert(driftFeet < 1e-6);
  assert(std::abs(((*parsedMm / 304.8) - kInputFeet)) < 1e-9);
}

void AssertImperialWeightRoundTrip() {
  constexpr double kInputPounds = 77.77;
  const auto parsedKg = UiUnitUtils::ParseWeightToKilograms(
      "77.77", UiUnitUtils::WeightUnitSystem::Imperial);
  assert(parsedKg.has_value());

  const std::string printedLb = UiUnitUtils::FormatWeightFromKilograms(
      *parsedKg, UiUnitUtils::WeightUnitSystem::Imperial,
      UiUnitUtils::ValueFormatContext::Table);
  assert(printedLb == "77.77");

  const auto reparsedKg = UiUnitUtils::ParseWeightToKilograms(
      printedLb, UiUnitUtils::WeightUnitSystem::Imperial);
  assert(reparsedKg.has_value());

  assert(UiUnitUtils::NearlyEqualWeightKilograms(*parsedKg, *reparsedKg,
                                                 1e-6));
  const double driftPounds = std::abs((*reparsedKg - *parsedKg) * 2.2046226218487757);
  assert(driftPounds < 1e-6);
  assert(std::abs((*parsedKg * 2.2046226218487757 - kInputPounds)) < 1e-9);
}

} // namespace

int main() {
  AssertImperialDistanceRoundTrip();
  AssertImperialWeightRoundTrip();
  std::cout << "ui_unit_utils_test passed" << std::endl;
  return 0;
}
