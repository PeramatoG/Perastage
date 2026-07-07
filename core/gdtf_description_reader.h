#pragma once

#include <string>
#include <vector>

namespace gdtf {

enum class DescriptionDiagnosticCode {
  None,
  MalformedXml,
  MissingRoot,
  MissingFixtureType,
  UnknownElement,
  MissingLocalResource
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
