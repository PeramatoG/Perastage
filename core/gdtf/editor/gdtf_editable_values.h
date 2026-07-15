#pragma once

#include "gdtf_field_registry.h"

#include <optional>
#include <string>

namespace gdtf {

struct GdtfEditableValues {
  std::optional<std::string> fixtureTypeName;
  std::optional<std::string> manufacturer;
  std::optional<std::string> modelName;
  std::optional<std::string> modeName;
  std::optional<std::string> fixtureTypeDescription;
  std::optional<float> weightKg;
  std::optional<float> powerConsumptionW;
  std::optional<float> trussLengthMm;
  std::optional<float> trussWidthMm;
  std::optional<float> trussHeightMm;
  std::optional<std::string> trussCrossSectionType;
  std::optional<std::string> trussCrossSection;
  std::optional<std::string> sourceFileReference;
};

bool SetEditableValue(GdtfEditableValues &values, GdtfFieldId fieldId,
                      const std::string &value);
std::optional<std::string> GetEditableValue(const GdtfEditableValues &values,
                                            GdtfFieldId fieldId);
bool operator==(const GdtfEditableValues &lhs, const GdtfEditableValues &rhs);
bool operator!=(const GdtfEditableValues &lhs, const GdtfEditableValues &rhs);

} // namespace gdtf
