#include "units.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace Units {
namespace {

constexpr double kMillimetersPerMeter = 1000.0;
constexpr double kMillimetersPerFoot = 304.8;
constexpr double kMillimetersPerInch = 25.4;
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

std::string Trim(const std::string &rawValue) {
  const auto begin = rawValue.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos)
    return {};
  const auto end = rawValue.find_last_not_of(" \t\r\n");
  return rawValue.substr(begin, end - begin + 1);
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::optional<double> ParseDouble(const std::string &rawValue) {
  const std::string trimmed = Trim(rawValue);
  if (trimmed.empty())
    return std::nullopt;

  double value = 0.0;
  const char *start = trimmed.data();
  const char *finish = trimmed.data() + trimmed.size();
  const auto result = std::from_chars(start, finish, value);
  if (result.ec != std::errc() || result.ptr != finish)
    return std::nullopt;
  return value;
}

std::string FormatNumber(double value, int decimals) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(decimals) << value;
  return stream.str();
}

std::optional<double> ParseDistanceFeetInches(const std::string &normalized) {
  const auto quotePos = normalized.find('\'');
  if (quotePos == std::string::npos)
    return std::nullopt;

  const auto feet = ParseDouble(normalized.substr(0, quotePos));
  if (!feet.has_value())
    return std::nullopt;

  std::string inchesPart = Trim(normalized.substr(quotePos + 1));
  if (!inchesPart.empty() && inchesPart.back() == '"')
    inchesPart.pop_back();

  double inches = 0.0;
  if (!Trim(inchesPart).empty()) {
    const auto parsedInches = ParseDouble(inchesPart);
    if (!parsedInches.has_value())
      return std::nullopt;
    inches = *parsedInches;
  }

  return (*feet * 12.0 + inches) * kMillimetersPerInch;
}

std::optional<double> ParseDistanceWithSuffix(const std::string &normalized) {
  static constexpr const char *kDistanceSuffixes[] = {"m", "ft", "in"};
  for (const char *suffix : kDistanceSuffixes) {
    const std::string suffixString = suffix;
    if (normalized.size() <= suffixString.size())
      continue;
    if (normalized.rfind(suffixString) != normalized.size() - suffixString.size())
      continue;

    const auto value = ParseDouble(normalized.substr(0, normalized.size() - suffixString.size()));
    if (!value.has_value())
      return std::nullopt;

    if (suffixString == "m")
      return *value * kMillimetersPerMeter;
    if (suffixString == "ft")
      return *value * kMillimetersPerFoot;
    return *value * kMillimetersPerInch;
  }
  return std::nullopt;
}

std::optional<double> ParseWeightWithSuffix(const std::string &normalized) {
  static constexpr const char *kWeightSuffixes[] = {"kg", "lb"};
  for (const char *suffix : kWeightSuffixes) {
    const std::string suffixString = suffix;
    if (normalized.size() <= suffixString.size())
      continue;
    if (normalized.rfind(suffixString) != normalized.size() - suffixString.size())
      continue;

    const auto value = ParseDouble(normalized.substr(0, normalized.size() - suffixString.size()));
    if (!value.has_value())
      return std::nullopt;

    if (suffixString == "kg")
      return *value;
    return *value / kPoundsPerKilogram;
  }
  return std::nullopt;
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

double DistanceMillimetersToDisplay(double valueMm, DistanceUnitSystem unitSystem) {
  return unitSystem == DistanceUnitSystem::Imperial ? valueMm / kMillimetersPerFoot
                                                     : valueMm / kMillimetersPerMeter;
}

double DistanceDisplayToMillimeters(double value, DistanceUnitSystem unitSystem) {
  return unitSystem == DistanceUnitSystem::Imperial ? value * kMillimetersPerFoot
                                                     : value * kMillimetersPerMeter;
}

double WeightKilogramsToDisplay(double valueKg, WeightUnitSystem unitSystem) {
  return unitSystem == WeightUnitSystem::Imperial ? valueKg * kPoundsPerKilogram
                                                   : valueKg;
}

double WeightDisplayToKilograms(double value, WeightUnitSystem unitSystem) {
  return unitSystem == WeightUnitSystem::Imperial ? value / kPoundsPerKilogram
                                                   : value;
}

std::string FormatDistanceFromMillimeters(double valueMm,
                                          DistanceUnitSystem unitSystem,
                                          ValueFormatContext context) {
  return FormatNumber(DistanceMillimetersToDisplay(valueMm, unitSystem),
                      DistanceDecimalsFor(context));
}

std::string FormatWeightFromKilograms(double valueKg,
                                      WeightUnitSystem unitSystem,
                                      ValueFormatContext context) {
  return FormatNumber(WeightKilogramsToDisplay(valueKg, unitSystem),
                      WeightDecimalsFor(context));
}

std::optional<double> ParseDistanceToMillimeters(const std::string &rawValue,
                                                 DistanceUnitSystem unitSystem) {
  std::string normalized = ToLower(rawValue);
  normalized = Trim(normalized);
  if (normalized.empty())
    return std::nullopt;

  if (const auto value = ParseDistanceFeetInches(normalized); value.has_value())
    return value;

  normalized.erase(
      std::remove_if(normalized.begin(), normalized.end(),
                     [](unsigned char c) { return std::isspace(c) != 0; }),
      normalized.end());

  if (const auto value = ParseDistanceWithSuffix(normalized); value.has_value())
    return value;

  const auto parsed = ParseDouble(normalized);
  if (!parsed.has_value())
    return std::nullopt;
  return DistanceDisplayToMillimeters(*parsed, unitSystem);
}

std::optional<double> ParseWeightToKilograms(const std::string &rawValue,
                                             WeightUnitSystem unitSystem) {
  std::string normalized = ToLower(rawValue);
  normalized = Trim(normalized);
  if (normalized.empty())
    return std::nullopt;

  normalized.erase(
      std::remove_if(normalized.begin(), normalized.end(),
                     [](unsigned char c) { return std::isspace(c) != 0; }),
      normalized.end());

  if (const auto value = ParseWeightWithSuffix(normalized); value.has_value())
    return value;

  const auto parsed = ParseDouble(normalized);
  if (!parsed.has_value())
    return std::nullopt;
  return WeightDisplayToKilograms(*parsed, unitSystem);
}

bool NearlyEqualDistanceMillimeters(double lhsMm, double rhsMm, double toleranceMm) {
  return std::abs(lhsMm - rhsMm) <= toleranceMm;
}

bool NearlyEqualWeightKilograms(double lhsKg, double rhsKg, double toleranceKg) {
  return std::abs(lhsKg - rhsKg) <= toleranceKg;
}

} // namespace Units
