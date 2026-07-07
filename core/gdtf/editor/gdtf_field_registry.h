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

struct GdtfFieldDescriptor {
  GdtfFieldId id;
  GdtfFieldOwnership ownership;
  const char *stableName;
  const char *displayName;
  bool independentlyEditable = false;
  bool repeatedFamily = false;
};

const GdtfFieldDescriptor *FindGdtfFieldDescriptor(GdtfFieldId id);
std::vector<GdtfFieldDescriptor> CurrentFixtureEditFieldDescriptors();
std::vector<GdtfFieldDescriptor> CurrentTrussEditFieldDescriptors();
bool IsGdtfFieldIndependentlyEditable(GdtfFieldId id);
const char *ToString(GdtfFieldOwnership ownership);

} // namespace gdtf
