#include "units/units.h"
#include "units/unit_label_utils.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

void AssertImperialDistanceRoundTrip() {
  constexpr double kInputFeet = 12.345;
  const auto parsedMm = Units::ParseDistanceToMillimeters(
      "12.345", Units::DistanceUnitSystem::Imperial);
  assert(parsedMm.has_value());

  const std::string printedFeet = Units::FormatDistanceFromMillimeters(
      *parsedMm, Units::DistanceUnitSystem::Imperial,
      Units::ValueFormatContext::Table);
  assert(printedFeet == "12.345");

  const auto reparsedMm = Units::ParseDistanceToMillimeters(
      printedFeet, Units::DistanceUnitSystem::Imperial);
  assert(reparsedMm.has_value());

  assert(Units::NearlyEqualDistanceMillimeters(*parsedMm, *reparsedMm,
                                                     0.001));

  const double driftFeet =
      std::abs(((*reparsedMm - *parsedMm) / 304.8));
  assert(driftFeet < 1e-6);
  assert(std::abs(((*parsedMm / 304.8) - kInputFeet)) < 1e-9);
}

void AssertImperialWeightRoundTrip() {
  constexpr double kInputPounds = 77.77;
  const auto parsedKg = Units::ParseWeightToKilograms(
      "77.77", Units::WeightUnitSystem::Imperial);
  assert(parsedKg.has_value());

  const std::string printedLb = Units::FormatWeightFromKilograms(
      *parsedKg, Units::WeightUnitSystem::Imperial,
      Units::ValueFormatContext::Table);
  assert(printedLb == "77.77");

  const auto reparsedKg = Units::ParseWeightToKilograms(
      printedLb, Units::WeightUnitSystem::Imperial);
  assert(reparsedKg.has_value());

  assert(Units::NearlyEqualWeightKilograms(*parsedKg, *reparsedKg,
                                                 1e-6));
  const double driftPounds = std::abs((*reparsedKg - *parsedKg) * 2.2046226218487757);
  assert(driftPounds < 1e-6);
  assert(std::abs((*parsedKg * 2.2046226218487757 - kInputPounds)) < 1e-9);
}

void AssertDistanceParsingWithSuffixes() {
  const auto meters = Units::ParseDistanceToMillimeters(
      "2.5m", Units::DistanceUnitSystem::Imperial);
  assert(meters.has_value());
  assert(std::abs(*meters - 2500.0) < 1e-6);

  const auto feetInches = Units::ParseDistanceToMillimeters(
      "5' 6\"", Units::DistanceUnitSystem::Metric);
  assert(feetInches.has_value());
  assert(std::abs(*feetInches - (66.0 * 25.4)) < 1e-6);

  const auto inches = Units::ParseDistanceToMillimeters(
      "18in", Units::DistanceUnitSystem::Metric);
  assert(inches.has_value());
  assert(std::abs(*inches - (18.0 * 25.4)) < 1e-6);
}

void AssertWeightParsingWithSuffixes() {
  const auto kilograms = Units::ParseWeightToKilograms(
      "10kg", Units::WeightUnitSystem::Imperial);
  assert(kilograms.has_value());
  assert(std::abs(*kilograms - 10.0) < 1e-9);

  const auto pounds = Units::ParseWeightToKilograms(
      "220lb", Units::WeightUnitSystem::Metric);
  assert(pounds.has_value());
  assert(std::abs(*pounds * 2.2046226218487757 - 220.0) < 1e-6);
}

void AssertLabelWithUnitHelper() {
  assert(Units::LabelWithUnit("Weight", "kg") == "Weight (kg)");
}

} // namespace

int main() {
  AssertImperialDistanceRoundTrip();
  AssertImperialWeightRoundTrip();
  AssertDistanceParsingWithSuffixes();
  AssertWeightParsingWithSuffixes();
  AssertLabelWithUnitHelper();
  std::cout << "ui_unit_utils_test passed" << std::endl;
  return 0;
}
