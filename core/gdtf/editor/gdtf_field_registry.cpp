#include "gdtf_field_registry.h"

#include <array>

namespace gdtf {
namespace {

constexpr std::array<GdtfFieldDescriptor, 29> kDescriptors = {{
    {GdtfFieldId::FixtureId, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "fixture_id", "Fixture ID", true, false},
    {GdtfFieldId::FixtureInstanceName,
     GdtfFieldOwnership::MvrProjectInstanceLevel, "fixture_instance_name",
     "Name", true, false},
    {GdtfFieldId::FixtureTypeName, GdtfFieldOwnership::GdtfTypeLevel,
     "fixture_type_name", "Type", true, false},
    {GdtfFieldId::Layer, GdtfFieldOwnership::MvrProjectInstanceLevel, "layer",
     "Layer", true, false},
    {GdtfFieldId::HangPosition, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "hang_position", "Hang position", true, false},
    {GdtfFieldId::Universe, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "universe", "Universe", true, false},
    {GdtfFieldId::DmxAddress, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "dmx_address", "Channel", true, false},
    {GdtfFieldId::ModeName, GdtfFieldOwnership::ContextSpecific, "mode_name",
     "Mode", true, false},
    {GdtfFieldId::ChannelCount, GdtfFieldOwnership::DerivedReadOnly,
     "channel_count", "Ch Count", false, false},
    {GdtfFieldId::SourceFileReference, GdtfFieldOwnership::ContextSpecific,
     "source_file_reference", "Model File", true, false},
    {GdtfFieldId::PositionX, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "position_x", "Position X", true, false},
    {GdtfFieldId::PositionY, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "position_y", "Position Y", true, false},
    {GdtfFieldId::PositionZ, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "position_z", "Position Z", true, false},
    {GdtfFieldId::Roll, GdtfFieldOwnership::MvrProjectInstanceLevel, "roll",
     "Roll", true, false},
    {GdtfFieldId::Pitch, GdtfFieldOwnership::MvrProjectInstanceLevel, "pitch",
     "Pitch", true, false},
    {GdtfFieldId::Yaw, GdtfFieldOwnership::MvrProjectInstanceLevel, "yaw",
     "Yaw", true, false},
    {GdtfFieldId::PowerConsumption, GdtfFieldOwnership::GdtfTypeLevel,
     "power_consumption", "Power", true, false},
    {GdtfFieldId::Weight, GdtfFieldOwnership::GdtfTypeLevel, "weight", "Weight",
     true, false},
    {GdtfFieldId::FixtureCategory,
     GdtfFieldOwnership::ProjectClassificationOverride, "fixture_category",
     "Category", true, false},
    {GdtfFieldId::VisualColor,
     GdtfFieldOwnership::ProjectClassificationOverride, "visual_color",
     "Visual color", true, false},
    {GdtfFieldId::MvrFixtureColor, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "mvr_fixture_color", "MVR color", true, false},
    {GdtfFieldId::Manufacturer, GdtfFieldOwnership::GdtfTypeLevel,
     "manufacturer", "Manufacturer", true, false},
    {GdtfFieldId::ModelName, GdtfFieldOwnership::GdtfTypeLevel, "model_name",
     "Model", true, false},
    {GdtfFieldId::TrussName, GdtfFieldOwnership::MvrProjectInstanceLevel,
     "truss_name", "Name", true, false},
    {GdtfFieldId::TrussLength, GdtfFieldOwnership::GdtfTypeLevel,
     "truss_length", "Length", true, false},
    {GdtfFieldId::TrussWidth, GdtfFieldOwnership::GdtfTypeLevel, "truss_width",
     "Width", true, false},
    {GdtfFieldId::TrussHeight, GdtfFieldOwnership::GdtfTypeLevel,
     "truss_height", "Height", true, false},
    {GdtfFieldId::TrussCrossSection, GdtfFieldOwnership::GdtfTypeLevel,
     "truss_cross_section", "Cross section", true, false},
    {GdtfFieldId::TrussLoad, GdtfFieldOwnership::DerivedReadOnly, "truss_load",
     "Load", false, false},
}};

} // namespace

// Finds the descriptor for a stable GDTF editor field identifier.
const GdtfFieldDescriptor *FindGdtfFieldDescriptor(GdtfFieldId id) {
  for (const auto &descriptor : kDescriptors) {
    if (descriptor.id == id)
      return &descriptor;
  }
  return nullptr;
}

// Returns the fields currently shown by Edit Fixture in table-column order.
std::vector<GdtfFieldDescriptor> CurrentFixtureEditFieldDescriptors() {
  return {{*FindGdtfFieldDescriptor(GdtfFieldId::FixtureId),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureInstanceName),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureTypeName),
           *FindGdtfFieldDescriptor(GdtfFieldId::Layer),
           *FindGdtfFieldDescriptor(GdtfFieldId::HangPosition),
           *FindGdtfFieldDescriptor(GdtfFieldId::Universe),
           *FindGdtfFieldDescriptor(GdtfFieldId::DmxAddress),
           *FindGdtfFieldDescriptor(GdtfFieldId::ModeName),
           *FindGdtfFieldDescriptor(GdtfFieldId::ChannelCount),
           *FindGdtfFieldDescriptor(GdtfFieldId::SourceFileReference),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionX),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionY),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionZ),
           *FindGdtfFieldDescriptor(GdtfFieldId::Roll),
           *FindGdtfFieldDescriptor(GdtfFieldId::Pitch),
           *FindGdtfFieldDescriptor(GdtfFieldId::Yaw),
           *FindGdtfFieldDescriptor(GdtfFieldId::PowerConsumption),
           *FindGdtfFieldDescriptor(GdtfFieldId::Weight),
           *FindGdtfFieldDescriptor(GdtfFieldId::FixtureCategory),
           *FindGdtfFieldDescriptor(GdtfFieldId::VisualColor),
           *FindGdtfFieldDescriptor(GdtfFieldId::MvrFixtureColor)}};
}

// Returns the fields currently shown by Edit Truss in table-column order plus
// cross section.
std::vector<GdtfFieldDescriptor> CurrentTrussEditFieldDescriptors() {
  return {{*FindGdtfFieldDescriptor(GdtfFieldId::TrussName),
           *FindGdtfFieldDescriptor(GdtfFieldId::Layer),
           *FindGdtfFieldDescriptor(GdtfFieldId::SourceFileReference),
           *FindGdtfFieldDescriptor(GdtfFieldId::HangPosition),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionX),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionY),
           *FindGdtfFieldDescriptor(GdtfFieldId::PositionZ),
           *FindGdtfFieldDescriptor(GdtfFieldId::Roll),
           *FindGdtfFieldDescriptor(GdtfFieldId::Pitch),
           *FindGdtfFieldDescriptor(GdtfFieldId::Yaw),
           *FindGdtfFieldDescriptor(GdtfFieldId::Manufacturer),
           *FindGdtfFieldDescriptor(GdtfFieldId::ModelName),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussLength),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussWidth),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussHeight),
           *FindGdtfFieldDescriptor(GdtfFieldId::Weight),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussLoad),
           *FindGdtfFieldDescriptor(GdtfFieldId::TrussCrossSection)}};
}

// Reports whether a field may be edited as independent session state.
bool IsGdtfFieldIndependentlyEditable(GdtfFieldId id) {
  const auto *descriptor = FindGdtfFieldDescriptor(id);
  return descriptor && descriptor->independentlyEditable;
}

// Converts a field ownership enum to a stable diagnostic string.
const char *ToString(GdtfFieldOwnership ownership) {
  switch (ownership) {
  case GdtfFieldOwnership::GdtfTypeLevel:
    return "GDTF type-level";
  case GdtfFieldOwnership::MvrProjectInstanceLevel:
    return "MVR/project instance-level";
  case GdtfFieldOwnership::DerivedReadOnly:
    return "derived/read-only";
  case GdtfFieldOwnership::ProjectClassificationOverride:
    return "project classification/override";
  case GdtfFieldOwnership::ContextSpecific:
    return "context-specific";
  case GdtfFieldOwnership::UnsupportedFuture:
    return "unsupported/future";
  }
  return "unknown";
}

} // namespace gdtf
