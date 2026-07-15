#pragma once

#include <string>
#include <vector>

namespace gdtf {

enum class GdtfFieldId {
  FixtureId,
  FixtureInstanceName,
  FixtureTypeName,
  Layer,
  HangPosition,
  Universe,
  DmxAddress,
  ModeName,
  ChannelCount,
  SourceFileReference,
  PositionX,
  PositionY,
  PositionZ,
  Roll,
  Pitch,
  Yaw,
  PowerConsumption,
  Weight,
  FixtureCategory,
  VisualColor,
  MvrFixtureColor,
  Manufacturer,
  ModelName,
  TrussName,
  TrussLength,
  TrussWidth,
  TrussHeight,
  FixtureTypeDescription,
  TrussCrossSectionType,
  TrussCrossSection,
  TrussLoad
};

enum class GdtfFieldOwnership {
  GdtfTypeLevel,
  MvrProjectInstanceLevel,
  DerivedReadOnly,
  ProjectClassificationOverride,
  ContextSpecific,
  UnsupportedFuture
};

enum class GdtfFieldValueKind {
  DocumentValue,
  ContextSelection,
  DerivedReadOnly,
  HostProjectValue,
  UnsupportedFuture
};

struct GdtfFieldDescriptor {
  GdtfFieldId id;
  GdtfFieldOwnership ownership;
  GdtfFieldValueKind defaultValueKind;
  const char *stableName;
  const char *displayName;
  bool hostDialogEditable = false;
  bool sessionValueSupported = false;
  bool derived = false;
  bool repeatedFamily = false;
};

const GdtfFieldDescriptor *FindGdtfFieldDescriptor(GdtfFieldId id);
std::vector<GdtfFieldDescriptor> CurrentFixtureEditFieldDescriptors();
std::vector<GdtfFieldDescriptor> CurrentTrussEditFieldDescriptors();
bool IsGdtfSessionValueSupported(GdtfFieldId id);
const char *ToString(GdtfFieldOwnership ownership);
const char *ToString(GdtfFieldValueKind defaultValueKind);

} // namespace gdtf
