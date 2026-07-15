#pragma once

#include <string>
#include <vector>

namespace gdtf {

enum class DescriptionDiagnosticCode {
  None,
  MalformedXml,
  MissingRoot,
  MissingFixtureType,
  MissingDmxModes,
  MissingUsableDmxMode,
  UnknownElement,
  MissingLocalResource,
  MissingWheelMediaResource,
  AmbiguousWheelMediaResource,
  NonCanonicalWheelMediaCaseMatch
};

struct DescriptionDiagnostic {
  DescriptionDiagnosticCode code = DescriptionDiagnosticCode::None;
  std::string message;
  std::string path;
};

struct GdtfRevisionInfo {
  std::string text;
  std::string date;
  std::string userId;
  std::string modifiedBy;
};

struct GdtfWheelSlotInfo {
  std::string name;
  std::string mediaFileName;
  std::vector<std::string> resourceReferences;
};

struct GdtfWheelInfo {
  std::string name;
  std::vector<GdtfWheelSlotInfo> slots;
};

struct GdtfDescriptionSnapshot {
  std::string dataVersion;
  std::string fixtureTypeName;
  std::string manufacturer;
  std::string shortName;
  std::string longName;
  std::string description;
  std::string fixtureTypeId;
  std::string thumbnail;
  std::string createDate;
  std::string revision;
  bool weightKgPresent = false;
  float weightKg = 0.0f;
  bool powerConsumptionWPresent = false;
  float powerConsumptionW = 0.0f;
  std::string modelColorHex;
  std::string trussCrossSectionType;
  std::string trussCrossSection;
  std::vector<GdtfRevisionInfo> revisions;
  std::vector<std::string> dmxModeNames;
  std::vector<GdtfWheelInfo> wheels;
  std::vector<DescriptionDiagnostic> diagnostics;

  bool Success() const;
};

GdtfDescriptionSnapshot ReadGdtfDescription(
    const std::string &descriptionXml,
    const std::vector<std::string> &archiveEntryPaths = {});

} // namespace gdtf
