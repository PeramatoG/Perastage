#include "ui_unit_utils.h"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace UiUnitUtils {
namespace {

constexpr double kMillimetersPerMeter = 1000.0;
constexpr double kMillimetersPerFoot = 304.8;
constexpr double kPoundsPerKilogram = 2.2046226218487757;

int DistanceDecimalsFor(ValueFormatContext context) {
  switch (context) {
  case ValueFormatContext::Table:
    return 3;
  case ValueFormatContext::Label:
    return 2;
  case ValueFormatContext::Inspector:
    return 4;
  }
  return 3;
}

int WeightDecimalsFor(ValueFormatContext context) {
  switch (context) {
  case ValueFormatContext::Table:
    return 2;
  case ValueFormatContext::Label:
    return 1;
  case ValueFormatContext::Inspector:
    return 3;
  }
  return 2;
}

std::optional<double> ParseDouble(const std::string &rawValue) {
  std::string trimmed = rawValue;
  const auto begin = trimmed.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos)
    return std::nullopt;
  const auto end = trimmed.find_last_not_of(" \t\r\n");
  trimmed = trimmed.substr(begin, end - begin + 1);

  double value = 0.0;
  const char *start = trimmed.data();
  const char *finish = trimmed.data() + trimmed.size();
  auto result = std::from_chars(start, finish, value);
  if (result.ec != std::errc() || result.ptr != finish)
    return std::nullopt;
  return value;
}

std::string FormatNumber(double value, int decimals) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(decimals) << value;
  return stream.str();
}

} // namespace

DistanceUnitSystem ParseDistanceUnitSystem(const std::optional<std::string> &rawValue) {
  if (rawValue && *rawValue == "imperial")
    return DistanceUnitSystem::Imperial;
  return DistanceUnitSystem::Metric;
}

WeightUnitSystem ParseWeightUnitSystem(const std::optional<std::string> &rawValue) {
  if (rawValue && *rawValue == "imperial")
    return WeightUnitSystem::Imperial;
  return WeightUnitSystem::Metric;
}

std::string DistanceUnitSuffix(DistanceUnitSystem unitSystem) {
  return unitSystem == DistanceUnitSystem::Imperial ? "ft" : "m";
}

std::string WeightUnitSuffix(WeightUnitSystem unitSystem) {
  return unitSystem == WeightUnitSystem::Imperial ? "lb" : "kg";
}

std::string FormatDistanceFromMillimeters(double valueMm,
                                          DistanceUnitSystem unitSystem,
                                          ValueFormatContext context) {
  const double displayValue =
      unitSystem == DistanceUnitSystem::Imperial ? valueMm / kMillimetersPerFoot
                                                 : valueMm / kMillimetersPerMeter;
  return FormatNumber(displayValue, DistanceDecimalsFor(context));
}

std::string FormatWeightFromKilograms(double valueKg,
                                      WeightUnitSystem unitSystem,
                                      ValueFormatContext context) {
  const double displayValue =
      unitSystem == WeightUnitSystem::Imperial ? valueKg * kPoundsPerKilogram
                                               : valueKg;
  return FormatNumber(displayValue, WeightDecimalsFor(context));
}

std::optional<double> ParseDistanceToMillimeters(const std::string &rawValue,
                                                 DistanceUnitSystem unitSystem) {
  const auto parsed = ParseDouble(rawValue);
  if (!parsed.has_value())
    return std::nullopt;
  return unitSystem == DistanceUnitSystem::Imperial
             ? *parsed * kMillimetersPerFoot
             : *parsed * kMillimetersPerMeter;
}

std::optional<double> ParseWeightToKilograms(const std::string &rawValue,
                                             WeightUnitSystem unitSystem) {
  const auto parsed = ParseDouble(rawValue);
  if (!parsed.has_value())
    return std::nullopt;
  return unitSystem == WeightUnitSystem::Imperial ? *parsed / kPoundsPerKilogram
                                                  : *parsed;
}

bool NearlyEqualDistanceMillimeters(double lhsMm, double rhsMm, double toleranceMm) {
  return std::abs(lhsMm - rhsMm) <= toleranceMm;
}

bool NearlyEqualWeightKilograms(double lhsKg, double rhsKg, double toleranceKg) {
  return std::abs(lhsKg - rhsKg) <= toleranceKg;
}

} // namespace UiUnitUtils
