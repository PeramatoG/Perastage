#include "gdtf_editable_values.h"

#include <cmath>
#include <locale>
#include <sstream>

namespace gdtf {
namespace {

// Parses a floating-point value for editable numeric GDTF fields.
std::optional<float> ParseFloat(const std::string &text) {
  if (text.empty())
    return std::nullopt;

  std::istringstream stream(text);
  stream.imbue(std::locale::classic());
  stream >> std::noskipws;

  float value = 0.0f;
  if (!(stream >> value))
    return std::nullopt;

  if (stream.peek() != std::char_traits<char>::eof())
    return std::nullopt;

  if (!std::isfinite(value))
    return std::nullopt;

  return value;
}

// Formats a floating-point value for stable session comparisons.
std::string FormatFloat(float value) { return std::to_string(value); }

} // namespace

// Assigns an editable value when the field belongs to the GDTF editor model.
bool SetEditableValue(GdtfEditableValues &values, GdtfFieldId fieldId,
                      const std::string &value) {
  if (!IsGdtfSessionValueSupported(fieldId))
    return false;
  switch (fieldId) {
  case GdtfFieldId::FixtureTypeName:
    values.fixtureTypeName = value;
    return true;
  case GdtfFieldId::Manufacturer:
    values.manufacturer = value;
    return true;
  case GdtfFieldId::ModelName:
    values.modelName = value;
    return true;
  case GdtfFieldId::ModeName:
    values.modeName = value;
    return true;
  case GdtfFieldId::Weight:
    if (auto parsed = ParseFloat(value)) {
      values.weightKg = *parsed;
      return true;
    }
    return false;
  case GdtfFieldId::PowerConsumption:
    if (auto parsed = ParseFloat(value)) {
      values.powerConsumptionW = *parsed;
      return true;
    }
    return false;
  case GdtfFieldId::TrussLength:
    if (auto parsed = ParseFloat(value)) {
      values.trussLengthMm = *parsed;
      return true;
    }
    return false;
  case GdtfFieldId::TrussWidth:
    if (auto parsed = ParseFloat(value)) {
      values.trussWidthMm = *parsed;
      return true;
    }
    return false;
  case GdtfFieldId::TrussHeight:
    if (auto parsed = ParseFloat(value)) {
      values.trussHeightMm = *parsed;
      return true;
    }
    return false;
  case GdtfFieldId::TrussCrossSection:
    values.trussCrossSection = value;
    return true;
  case GdtfFieldId::SourceFileReference:
    values.sourceFileReference = value;
    return true;
  default:
    return false;
  }
}

// Reads an editable field as a string for validation and host adapters.
std::optional<std::string> GetEditableValue(const GdtfEditableValues &values,
                                            GdtfFieldId fieldId) {
  switch (fieldId) {
  case GdtfFieldId::FixtureTypeName:
    return values.fixtureTypeName;
  case GdtfFieldId::Manufacturer:
    return values.manufacturer;
  case GdtfFieldId::ModelName:
    return values.modelName;
  case GdtfFieldId::ModeName:
    return values.modeName;
  case GdtfFieldId::Weight:
    return values.weightKg
               ? std::optional<std::string>(FormatFloat(*values.weightKg))
               : std::nullopt;
  case GdtfFieldId::PowerConsumption:
    return values.powerConsumptionW ? std::optional<std::string>(FormatFloat(
                                          *values.powerConsumptionW))
                                    : std::nullopt;
  case GdtfFieldId::TrussLength:
    return values.trussLengthMm
               ? std::optional<std::string>(FormatFloat(*values.trussLengthMm))
               : std::nullopt;
  case GdtfFieldId::TrussWidth:
    return values.trussWidthMm
               ? std::optional<std::string>(FormatFloat(*values.trussWidthMm))
               : std::nullopt;
  case GdtfFieldId::TrussHeight:
    return values.trussHeightMm
               ? std::optional<std::string>(FormatFloat(*values.trussHeightMm))
               : std::nullopt;
  case GdtfFieldId::TrussCrossSection:
    return values.trussCrossSection;
  case GdtfFieldId::SourceFileReference:
    return values.sourceFileReference;
  default:
    return std::nullopt;
  }
}

// Compares two editable value sets for session dirty tracking.
bool operator==(const GdtfEditableValues &lhs, const GdtfEditableValues &rhs) {
  return lhs.fixtureTypeName == rhs.fixtureTypeName &&
         lhs.manufacturer == rhs.manufacturer &&
         lhs.modelName == rhs.modelName && lhs.modeName == rhs.modeName &&
         lhs.weightKg == rhs.weightKg &&
         lhs.powerConsumptionW == rhs.powerConsumptionW &&
         lhs.trussLengthMm == rhs.trussLengthMm &&
         lhs.trussWidthMm == rhs.trussWidthMm &&
         lhs.trussHeightMm == rhs.trussHeightMm &&
         lhs.trussCrossSection == rhs.trussCrossSection &&
         lhs.sourceFileReference == rhs.sourceFileReference;
}

// Compares two editable value sets for inequality.
bool operator!=(const GdtfEditableValues &lhs, const GdtfEditableValues &rhs) {
  return !(lhs == rhs);
}

} // namespace gdtf
