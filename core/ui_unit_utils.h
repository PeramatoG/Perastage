#pragma once

#include <optional>
#include <string>

namespace UiUnitUtils {

enum class DistanceUnitSystem { Metric, Imperial };
enum class WeightUnitSystem { Metric, Imperial };
enum class ValueFormatContext { Table, Label, Inspector };

DistanceUnitSystem ParseDistanceUnitSystem(const std::optional<std::string> &rawValue);
WeightUnitSystem ParseWeightUnitSystem(const std::optional<std::string> &rawValue);

std::string DistanceUnitSuffix(DistanceUnitSystem unitSystem);
std::string WeightUnitSuffix(WeightUnitSystem unitSystem);

std::string FormatDistanceFromMillimeters(double valueMm,
                                          DistanceUnitSystem unitSystem,
                                          ValueFormatContext context);
std::string FormatWeightFromKilograms(double valueKg,
                                      WeightUnitSystem unitSystem,
                                      ValueFormatContext context);

std::optional<double> ParseDistanceToMillimeters(const std::string &rawValue,
                                                 DistanceUnitSystem unitSystem);
std::optional<double> ParseWeightToKilograms(const std::string &rawValue,
                                             WeightUnitSystem unitSystem);

bool NearlyEqualDistanceMillimeters(double lhsMm, double rhsMm, double toleranceMm);
bool NearlyEqualWeightKilograms(double lhsKg, double rhsKg, double toleranceKg);

} // namespace UiUnitUtils
